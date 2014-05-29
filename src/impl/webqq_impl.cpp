﻿/*
 * Copyright (C) 2012 - 2013  微蔡 <microcai@fedoraproject.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include <string.h>
#include <string>
#include <iostream>

#include <boost/random.hpp>
#include <boost/system/system_error.hpp>
#include <boost/bind.hpp>
#include <boost/bind/protect.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/format.hpp>
#include <boost/property_tree/json_parser.hpp>
namespace js = boost::property_tree::json_parser;
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <boost/assign.hpp>
#include <boost/scope_exit.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/json_create_escapes_utf8.hpp>

#include "boost/timedcall.hpp"
#include "boost/stringencodings.hpp"
#include "boost/urlencode.hpp"

#include "webqq_impl.hpp"
#include "constant.hpp"

#include "webqq_status.hpp"
#include "webqq_verify_image.hpp"
#include "webqq_keepalive.hpp"
#include "group_message_sender.hpp"
#include "webqq_loop.hpp"
#include "webqq_send_offfile.hpp"

#ifdef WIN32

#include <stdarg.h>

// '_vsnprintf': This function or variable may be unsafe
#pragma warning(disable:4996)

inline int snprintf(char* buf, int len, char const* fmt, ...)
{
	va_list lp;
	va_start(lp, fmt);
	int ret = _vsnprintf(buf, len, fmt, lp);
	va_end(lp);
	if (ret < 0) { buf[len-1] = 0; ret = len-1; }
	return ret;
}

#endif // WIN32

namespace webqq{
namespace qqimpl{

static void dummy(){}

///low level special char mapping
static pt::wptree json_parse( const wchar_t * doc )
{
	pt::wptree jstree;
	std::wstringstream stream;
	stream <<  doc ;
	js::read_json( stream, jstree );
	return jstree;
}

// build webqq and setup defaults
WebQQ::WebQQ( boost::asio::io_service& _io_service,
	std::string _qqnum, std::string _passwd, bool no_persistent_db)
	: m_io_service( _io_service )
	, m_qqnum( _qqnum )
	, m_passwd( _passwd )
	, m_status( LWQQ_STATUS_OFFLINE )
	, m_cookie_mgr(no_persistent_db? ":memory:": "webqq_persistent")
	, m_buddy_mgr(no_persistent_db? ":memory:": "webqq_persistent")
	, m_vc_queue(_io_service, 1)
	, m_group_message_queue(_io_service, 20) // 最多保留最后的20条未发送消息.
	, m_group_refresh_queue(_io_service)
{
#ifndef _WIN32
	/* Set msg_id */
	timeval tv;
	long v;
	gettimeofday( &tv, NULL );
	v = tv.tv_usec;
	v = ( v - v % 1000 ) / 1000;
	v = v % 10000 * 10000;
	m_msg_id = v;
#else
	m_msg_id = std::rand();
#endif

	init_face_map();

	m_fetch_groups = true;
}

void WebQQ::start()
{
	webqq_loop_start(get_ioservice(), shared_from_this());

	group_message_sender(shared_from_this());

	webqq_keepalive(shared_from_this());
}

void WebQQ::stop()
{
	m_group_message_queue.cancele();
	m_group_refresh_queue.cancele();
	m_status = LWQQ_STATUS_QUITTING;
}

// last step of a login process
// and this will be callded every other minutes to prevent foce kick off.
void  WebQQ::change_status(LWQQ_STATUS status, boost::function<void (boost::system::error_code) > handler)
{
	async_change_status(shared_from_this(), status, handler);
}

void WebQQ::send_group_message( qqGroup& group, std::string msg, send_group_message_cb donecb )
{
	send_group_message( group.gid, msg, donecb );
}

void WebQQ::send_group_message( std::string group, std::string msg, send_group_message_cb donecb )
{
	m_group_message_queue.push(boost::make_tuple( group, msg, donecb ));
}

void WebQQ::update_group_member(boost::shared_ptr<qqGroup> group, webqq::webqq_handler_t handler)
{
	m_group_refresh_queue.push(
		boost::make_tuple(handler, group->gid)
	);
}

void WebQQ::send_offline_file(std::string uin, std::string filename, webqq::webqq_handler_t handler)
{
	async_send_offline_file(shared_from_this(), uin, filename, handler);
}

class SYMBOL_HIDDEN buddy_uin_to_qqnumber {
public:
	// 将　qqBuddy 里的　uin 转化为　qq 号码.
	template<class Handler>
	buddy_uin_to_qqnumber( boost::shared_ptr<WebQQ> _webqq, std::string uin, Handler handler )
		: _io_service(_webqq->get_ioservice())
		, m_webqq(_webqq)
	{
		std::string url = boost::str(
			boost::format( "%s/api/get_friend_uin2?tuin=%s&verifysession=&type=1&code=&vfwebqq=%s" )
			% "http://s.web2.qq.com"
			% uin
			% _webqq->m_vfwebqq
		);

		m_stream = boost::make_shared<avhttp::http_stream>(boost::ref(_io_service));
		m_webqq->m_cookie_mgr.get_cookie(url, *m_stream);
		m_stream->request_options(
			avhttp::request_opts()
			( avhttp::http_options::http_version , "HTTP/1.0" )
			( avhttp::http_options::referer, "http://s.web2.qq.com/proxy.html?v=201304220930&callback=1&id=3" )
			( avhttp::http_options::content_type, "UTF-8" )
			( avhttp::http_options::connection, "close" )
		);

		boost::shared_ptr<boost::asio::streambuf> buffer = boost::make_shared<boost::asio::streambuf>();

		avhttp::async_read_body(
			*m_stream, url, *buffer,
			boost::bind<void>( *this, _1, buffer, handler)
		);
	}

	template <class Handler>
	void operator()( const boost::system::error_code& ec, boost::shared_ptr<boost::asio::streambuf> buffer, Handler handler )
	{
		// 获得的返回代码类似
		// {"retcode":0,"result":{"uiuin":"","account":2664046919,"uin":721281587}}
		pt::ptree jsonobj;
		std::iostream resultjson( buffer.get() );

		try
		{
			// 处理.
			pt::json_parser::read_json( resultjson, jsonobj );
			int retcode = jsonobj.get<int>("retcode");
			if (retcode ==0)
			{
				std::string qqnum = jsonobj.get<std::string>( "result.account" );
				return _io_service.post(
					boost::asio::detail::bind_handler(handler, qqnum)
				);
			}
		}
		catch(const pt::ptree_error & badpath)
		{
			AVLOG_ERR <<  __FILE__ << " : " << __LINE__ << " : " <<  "bad path" <<  badpath.what();
		}

		_io_service.post( boost::asio::detail::bind_handler( handler, std::string( "" ) ) );
	}
private:
	boost::asio::io_service& _io_service;
	boost::shared_ptr<WebQQ> m_webqq;
	read_streamptr m_stream;
};

class SYMBOL_HIDDEN update_group_member_qq_op : boost::asio::coroutine {
public:
	update_group_member_qq_op( boost::shared_ptr<WebQQ>  _webqq, boost::shared_ptr<qqGroup> _group )
		: group( _group ), m_webqq( _webqq )
	{
		m_uins = m_webqq->m_buddy_mgr.get_group_all_buddies_uin(_group->gid);

		m_webqq->get_ioservice().post(
			boost::asio::detail::bind_handler(*this, std::string())
		);
	}

	void operator()( std::string qqnum )
	{
		//我说了是一个一个的更新对吧，可不能一次发起　N 个连接同时更新，会被TX拉黑名单的.
		BOOST_ASIO_CORO_REENTER( this )
		{
			for( i = 0 ; i < m_uins.size() ; i++ )
			{
				if (!m_webqq->m_buddy_mgr.buddy_has_qqnum(m_uins[i]))
				{
					BOOST_ASIO_CORO_YIELD buddy_uin_to_qqnumber( m_webqq, m_uins[i], *this );
					if ( qqnum == "-1")
						return;
					else
						m_webqq->m_buddy_mgr.map_buddy_qqnum(m_uins[i], qqnum);
				}
			}
		}
	}
private:
	boost::shared_ptr<qqGroup> group;
	boost::shared_ptr<WebQQ> m_webqq;

	std::vector<std::string> m_uins;

	int i;
};

//　将组成员的 QQ 号码一个一个更新过来.
void WebQQ::update_group_member_qq(boost::shared_ptr<qqGroup> group )
{
	update_group_member_qq_op op( shared_from_this(), group );
}

qqGroup_ptr WebQQ::get_Group_by_gid( std::string gid )
{
	return m_buddy_mgr.get_group_by_gid(gid);
}

qqGroup_ptr WebQQ::get_Group_by_qq( std::string qq )
{
	return m_buddy_mgr.get_group_by_qq(qq);
}

std::vector< qqBuddy_ptr > WebQQ::get_buddies()
{
	return m_buddy_mgr.get_buddies();
}

void WebQQ::get_verify_image( std::string vcimgid, webqq::webqq_handler_string_t handler)
{
	detail::get_verify_image_op op(shared_from_this(), vcimgid, handler);
}

void WebQQ::cb_newbee_group_join( qqGroup_ptr group,  std::string uid )
{
	if (group)
	// 报告新人入群.
	boost::delayedcallsec(get_ioservice(), 30,
		boost::bind(boost::ref(signewbuddy), group, group->get_Buddy_by_uin(uid))
	);
}

void WebQQ::cb_fetch_aid(const boost::system::error_code& ec, read_streamptr stream,  boost::shared_ptr<boost::asio::streambuf> buf, boost::function<void(const boost::system::error_code&, std::string)> handler)
{
	if (!ec)
	{
		// 获取到咯, 更新 verifysession
		m_cookie_mgr.save_cookie(*stream);

		handler(boost::system::error_code(), std::string(boost::asio::buffer_cast<const char*>(buf->data()), boost::asio::buffer_size(buf->data())));
		return;
	}
	handler(ec, std::string());
}

void WebQQ::fetch_aid(std::string arg, boost::function<void(const boost::system::error_code&, std::string)> handler)
{
	std::string url = boost::str(
		boost::format("http://captcha.qq.com/getimage?%s") % arg
	);

	read_streamptr stream(new avhttp::http_stream(m_io_service));
	m_cookie_mgr.get_cookie(url, *stream);
	stream->request_options(
		avhttp::request_opts()
			(avhttp::http_options::referer, "http://web.qq.com/")
			(avhttp::http_options::connection, "close")
	);

	boost::shared_ptr<boost::asio::streambuf> buffer = boost::make_shared<boost::asio::streambuf>();
	avhttp::async_read_body(*stream, url,*buffer, boost::bind(&WebQQ::cb_fetch_aid, shared_from_this(), _1, stream, buffer, handler ));
}

static void cb_search_group_vcode(const boost::system::error_code& ec, std::string vcodedata, webqq::search_group_handler handler, qqGroup_ptr group)
{
	if (!ec){
		handler(group, 1, vcodedata);
	}else{
		group.reset();
		handler(group, 0, vcodedata);
	}
}

void WebQQ::cb_search_group(std::string groupqqnum, const boost::system::error_code& ec, read_streamptr stream,  boost::shared_ptr<boost::asio::streambuf> buf, webqq::search_group_handler handler)
{
	pt::ptree	jsobj;
	std::istream jsondata(buf.get());
	qqGroup_ptr  group;

	if (!ec){
		// 读取 json 格式
		js::read_json(jsondata, jsobj);
		group.reset(new qqGroup);
		group->qqnum = groupqqnum;
		try{
			if(jsobj.get<int>("retcode") == 0){
				group->qqnum = jsobj.get<std::string>("result..GE");
				group->code = jsobj.get<std::string>("result..GEX");
			}else if(jsobj.get<int>("retcode") == 100110){
				// 需要验证码, 先获取验证码图片，然后回调
				fetch_aid(boost::str(boost::format("aid=1003901&%ld") % std::time(NULL)), boost::bind(cb_search_group_vcode, _1, _2, handler, group) );
				return;
			}else if (jsobj.get<int>("retcode")==100102){
				// 验证码错误
				group.reset();
			}
		}catch (...){
			group.reset();
		}
	}
	handler(group, 0, "");
}

void WebQQ::search_group(std::string groupqqnum, std::string vfcode, webqq::search_group_handler handler)
{
	// GET /keycgi/qqweb/group/search.do?pg=1&perpage=10&all=82069263&c1=0&c2=0&c3=0&st=0&vfcode=&type=1&vfwebqq=59b09b83f622d820cd9ee4e04d4f4e4664e6704ee7ac487ce00595f8c539476b49fdcc372e1d11ea&t=1365138435110 HTTP/1.1
	std::string url = boost::str(
		boost::format("%s/keycgi/qqweb/group/search.do?pg=1&perpage=10&all=%s&c1=0&c2=0&c3=0&st=0&vfcode=%s&type=1&vfwebqq=%s&t=%ld")
			%  "http://cgi.web2.qq.com" % groupqqnum % vfcode %  m_vfwebqq % std::time(NULL)
	);

	read_streamptr stream(new avhttp::http_stream(m_io_service));
	m_cookie_mgr.get_cookie(url, *stream);
	stream->request_options(avhttp::request_opts()
		(avhttp::http_options::content_type, "utf-8")
		(avhttp::http_options::referer, "http://cgi.web2.qq.com/proxy.html?v=201304220930&callback=1&id=2")
		(avhttp::http_options::connection, "close")
	);

	boost::shared_ptr<boost::asio::streambuf> buffer = boost::make_shared<boost::asio::streambuf>();
	avhttp::async_read_body(*stream, url, * buffer, boost::bind(&WebQQ::cb_search_group, shared_from_this(), groupqqnum, _1, stream, buffer, handler));
}

static void cb_join_group_vcode(const boost::system::error_code& ec, std::string vcodedata, webqq::join_group_handler handler, qqGroup_ptr group)
{
	if (!ec){
		handler(group, 1, vcodedata);
	}else{
		group.reset();
		handler(group, 0, vcodedata);
	}
}


void WebQQ::cb_join_group( qqGroup_ptr group, const boost::system::error_code& ec, read_streamptr stream, boost::shared_ptr<boost::asio::streambuf> buf, webqq::join_group_handler handler )
{
	// 检查返回值是不是 retcode == 0
	pt::ptree	jsobj;
	std::istream jsondata(buf.get());
	try{
		js::read_json(jsondata, jsobj);
		js::write_json(std::cerr, jsobj);

		if(jsobj.get<int>("retcode") == 0){
			// 搞定！群加入咯. 等待管理员吧.
			handler(group, 0, "");
			// 获取群的其他信息
			// GET http://s.web2.qq.com/api/get_group_public_info2?gcode=3272859045&vfwebqq=f08e7a200fd0be375d753d3fedfd24e99f6ba0a8063005030bb95f9fa4b7e0c30415ae74e77709e3&t=1365161430494 HTTP/1.1
		}else if(jsobj.get<int>("retcode") == 100001){
			std::cout << literal_to_localstr("原因： ") <<   jsobj.get<std::string>("tips") <<  std::endl;
			// 需要验证码, 先获取验证码图片，然后回调
			fetch_aid(boost::str(boost::format("aid=%s&_=%ld") % APPID % std::time(NULL)), boost::bind(cb_join_group_vcode, _1, _2, handler, group) );
		}else{
			// 需要验证码, 先获取验证码图片，然后回调
			fetch_aid(boost::str(boost::format("aid=%s&_=%ld") % APPID % std::time(NULL)), boost::bind(cb_join_group_vcode, _1, _2, handler, group) );
		}
	}catch (...){
		handler(qqGroup_ptr(), 0, "");
	}
}


void WebQQ::join_group(qqGroup_ptr group, std::string vfcode, webqq::join_group_handler handler )
{
	std::string url = "http://s.web2.qq.com/api/apply_join_group2";

	std::string postdata =	boost::str(
								boost::format(
									"{\"gcode\":%s,"
									"\"code\":\"%s\","
									"\"vfy\":\"%s\","
									"\"msg\":\"avbot\","
									"\"vfwebqq\":\"%s\"}" )
								% group->code
								% vfcode
								% m_cookie_mgr.get_cookie(url)["verifysession"]
								% m_vfwebqq
							);

	postdata = std::string("r=") + avhttp::detail::escape_string(postdata);

	read_streamptr stream(new avhttp::http_stream(m_io_service));
	m_cookie_mgr.get_cookie(url, *stream);

	stream->request_options(avhttp::request_opts()
		(avhttp::http_options::http_version, "HTTP/1.0")
		(avhttp::http_options::content_type, "application/x-www-form-urlencoded; charset=UTF-8")
		(avhttp::http_options::referer, "http://s.web2.qq.com/proxy.html?v=201304220930&callback=1&id=1")
		(avhttp::http_options::connection, "close")
		(avhttp::http_options::request_method, "POST")
		(avhttp::http_options::request_body, postdata)
		(avhttp::http_options::content_length, boost::lexical_cast<std::string>(postdata.length()))
	);

	boost::shared_ptr<boost::asio::streambuf> buffer = boost::make_shared<boost::asio::streambuf>();

	avhttp::async_read_body(*stream, url, * buffer, boost::bind(&WebQQ::cb_join_group, shared_from_this(), group, _1, stream, buffer, handler));
}

} // namespace qqimpl
} // namespace webqq
