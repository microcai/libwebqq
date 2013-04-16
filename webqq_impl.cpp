/*
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

#include "boost/timedcall.hpp"

#include "constant.hpp"
#include "webqq.h"
#include "webqq_impl.h"
#include "md5.hpp"
#include "url.hpp"
#include "utf8.hpp"
#include "webqq_login.hpp"
#include "boost/consolestr.hpp"

using namespace qqimpl;

static std::string generate_clientid();

///low level special char mapping
static std::string parse_unescape( std::string source );

static std::string create_post_data( std::string vfwebqq )
{
	std::string m = boost::str( boost::format( "{\"h\":\"hello\",\"vfwebqq\":\"%s\"}" ) % vfwebqq );
	return std::string( "r=" ) + url_encode( m.c_str() );
}

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
			  std::string _qqnum, std::string _passwd)
	: m_io_service( _io_service ), m_qqnum( _qqnum ), m_passwd( _passwd ), m_status( LWQQ_STATUS_OFFLINE ),
	m_fetch_cface(0), m_msg_queue( 20 ) //　最多保留最后的20条未发送消息.
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
}

/**login*/
void WebQQ::login()
{
	// start login process, will call login_withvc later
	detail::corologin( *this );
}

// login to server with vc. called by login code or by user
// if no verify image needed, then called by login
// if verify image needed, then the user should listen to signeedvc and call this
void WebQQ::login_withvc( std::string vccode )
{
	std::cout << "vc code is \"" << vccode << "\"" << std::endl;
	detail::corologin_vc( *this, vccode );
}

void WebQQ::send_group_message( qqGroup& group, std::string msg, send_group_message_cb donecb )
{
	send_group_message( group.gid, msg, donecb );
}

void WebQQ::send_group_message( std::string group, std::string msg, send_group_message_cb donecb )
{
	//check if already in sending a message
	m_msg_queue.push_back( boost::make_tuple( group, msg, donecb ) );

	if( !m_group_msg_insending ) {
		m_group_msg_insending = true;
		send_group_message_internal( group, msg, donecb );
	}
}

void WebQQ::send_group_message_internal( std::string group, std::string msg, send_group_message_cb donecb )
{
	//unescape for POST
	std::string postdata = boost::str(
							   boost::format( "r={\"group_uin\":\"%s\", "
									   "\"content\":\"["
									   "\\\"%s\\\","
									   "[\\\"font\\\",{\\\"name\\\":\\\"宋体\\\",\\\"size\\\":\\\"9\\\",\\\"style\\\":[0,0,0],\\\"color\\\":\\\"000000\\\"}]"
									   "]\","
									   "\"msg_id\":%ld,"
									   "\"clientid\":\"%s\","
									   "\"psessionid\":\"%s\"}&clientid=%s&psessionid=%s" )
							   % group % parse_unescape( msg ) % m_msg_id % m_clientid % m_psessionid
							   % m_clientid
							   % m_psessionid
						   );

	read_streamptr stream( new avhttp::http_stream( m_io_service ) );
	stream->request_options(
		avhttp::request_opts()
		( avhttp::http_options::request_method, "POST" )
		( avhttp::http_options::cookie, m_cookies.lwcookies )
		( avhttp::http_options::referer, "http://d.web2.qq.com/proxy.html?v=20101025002" )
		( avhttp::http_options::content_type, "application/x-www-form-urlencoded; charset=UTF-8" )
		( avhttp::http_options::request_body, postdata )
		( avhttp::http_options::content_length, boost::lexical_cast<std::string>( postdata.length() ) )
		( avhttp::http_options::connection, "close" )
	);

	async_http_download( stream, LWQQ_URL_SEND_QUN_MSG,
						 boost::bind( &WebQQ::cb_send_msg, this, boost::asio::placeholders::error, _2, _3, donecb )
					   );
}

void WebQQ::cb_send_msg( const boost::system::error_code& ec, read_streamptr stream, boost::asio::streambuf & buffer, boost::function<void ( const boost::system::error_code& ec )> donecb )
{
	pt::ptree jstree;
	std::istream	response( &buffer );

	try {
		js::read_json( response, jstree );

		if( jstree.get<int>( "retcode" ) == 108 ) {
			// 已经断线，重新登录
			m_status = LWQQ_STATUS_UNKNOW;
			// 10s 后登录.
			boost::delayedcallsec( m_io_service, 10, boost::bind( &WebQQ::login, this ) );
			m_group_msg_insending = false;
			return ;
		}

	} catch( const pt::json_parser_error & jserr ) {
		std::istream	response( &buffer );
		std::cerr <<  __FILE__ << " : " << __LINE__ << " : " << "parse json error : " << jserr.what()
 			<<  "\n=========\n" <<  jserr.message() << "\n=========" <<  std::endl;
		m_msg_queue.pop_front();
	} catch( const pt::ptree_bad_path & badpath ) {
		std::cerr << __FILE__ << " : " << __LINE__ << " : " <<  "bad path " <<  badpath.what() <<  std::endl;
	}

	if (!m_msg_queue.empty())
		m_msg_queue.pop_front();

	if( m_msg_queue.empty() ) {
		m_group_msg_insending = false;
	} else {
		boost::tuple<std::string, std::string, send_group_message_cb> v = m_msg_queue.front();
		boost::delayedcallms( m_io_service, 500, boost::bind( &WebQQ::send_group_message_internal, this, boost::get<0>( v ), boost::get<1>( v ), boost::get<2>( v ) ) );
	}

	m_io_service.post( boost::asio::detail::bind_handler( donecb, ec ) );
}

void WebQQ::update_group_list()
{
	std::cout <<  "getting group list" <<  std::endl;
	/* Create post data: {"h":"hello","vfwebqq":"4354j53h45j34"} */
	std::string postdata = create_post_data( this->m_vfwebqq );
	std::string url = boost::str( boost::format( "%s/api/get_group_name_list_mask2" ) % "http://s.web2.qq.com" );

	read_streamptr stream( new avhttp::http_stream( m_io_service ) );
	stream->request_options(
		avhttp::request_opts()
		( avhttp::http_options::request_method, "POST" )
		( avhttp::http_options::cookie, m_cookies.lwcookies )
		( avhttp::http_options::referer, "http://s.web2.qq.com/proxy.html?v=20101025002" )
		( avhttp::http_options::content_type, "application/x-www-form-urlencoded; charset=UTF-8" )
		( avhttp::http_options::request_body, postdata )
		( avhttp::http_options::content_length, boost::lexical_cast<std::string>( postdata.length() ) )
		( avhttp::http_options::connection, "close" )
	);

	async_http_download( stream, url,
						 boost::bind( &WebQQ::cb_group_list, this, boost::asio::placeholders::error, _2, _3 )
					   );
}

void WebQQ::update_group_qqmember(boost::shared_ptr<qqGroup> group )
{
	std::string url;

	url = boost::str(
			  boost::format( "%s/api/get_friend_uin2?tuin=%s&verifysession=&type=1&code=&vfwebqq=%s&t=%ld" )
			  % "http://s.web2.qq.com"
			  % group->code
			  % m_vfwebqq
			  % time( NULL )
		  );
	read_streamptr stream( new avhttp::http_stream( m_io_service ) );
	stream->request_options(
		avhttp::request_opts()
		( avhttp::http_options::cookie, m_cookies.lwcookies )
		( avhttp::http_options::referer, LWQQ_URL_REFERER_QUN_DETAIL )
		( avhttp::http_options::connection, "close" )
	);

	async_http_download( stream, url,
						 boost::bind( &WebQQ::cb_group_qqnumber, this, boost::asio::placeholders::error, _2, _3, group)
					   );
}

void WebQQ::update_group_member(boost::shared_ptr<qqGroup> group )
{
	read_streamptr stream( new avhttp::http_stream( m_io_service ) );

	std::string url = boost::str(
						  boost::format( "%s/api/get_group_info_ext2?gcode=%s&vfwebqq=%s&t=%ld" )
						  % "http://s.web2.qq.com"
						  % group->code
						  % m_vfwebqq
						  % std::time( NULL )
					  );
	stream->request_options(
		avhttp::request_opts()
		( avhttp::http_options::http_version , "HTTP/1.0" )
		( avhttp::http_options::cookie, m_cookies.lwcookies )
		( avhttp::http_options::referer, LWQQ_URL_REFERER_QUN_DETAIL )
		( avhttp::http_options::connection, "close" )
	);

	async_http_download( stream, url,
						 boost::bind( &WebQQ::cb_group_member, this, boost::asio::placeholders::error, _2, _3, group)
					   );
}

class SYMBOL_HIDDEN buddy_uin_to_qqnumber {
public:
	typedef void result_type;
	// 将　qqBuddy 里的　uin 转化为　qq 号码.
	template<class Handler>
	buddy_uin_to_qqnumber( WebQQ & _webqq, std::string uin, Handler handler )
		: _io_service( _webqq.get_ioservice() ) {
		read_streamptr stream;
		std::string url = boost::str(
							  boost::format( "%s/api/get_friend_uin2?tuin=%s&verifysession=&type=1&code=&vfwebqq=%s" )
							  % "http://s.web2.qq.com" % uin % _webqq.m_vfwebqq
						  );

		stream.reset( new avhttp::http_stream( _webqq.get_ioservice() ) );
		stream->request_options(
			avhttp::request_opts()
			( avhttp::http_options::http_version , "HTTP/1.0" )
			( avhttp::http_options::cookie, _webqq.m_cookies.lwcookies )
			( avhttp::http_options::referer, "http://s.web2.qq.com/proxy.html?v=20110412001&callback=1&id=3" )
			( avhttp::http_options::content_type, "UTF-8" )
			( avhttp::http_options::connection, "close" )
		);

		async_http_download( stream, url, boost::bind( *this, _1, _2, _3, handler ) );
	}

	template <class Handler>
	void operator()( const boost::system::error_code& ec, read_streamptr stream,  boost::asio::streambuf & buffer, Handler handler )
	{
		// 获得的返回代码类似
		// {"retcode":0,"result":{"uiuin":"","account":2664046919,"uin":721281587}}
		pt::ptree jsonobj;
		std::iostream resultjson( &buffer );

		try {
			// 处理.
			pt::json_parser::read_json( resultjson, jsonobj );
			int retcode = jsonobj.get<int>("retcode");
			if (retcode ==  99999 ){
				_io_service.post( boost::asio::detail::bind_handler( handler, std::string("-1") ) );
			}else{
				std::string qqnum = jsonobj.get<std::string>( "result.account" );

				_io_service.post( boost::asio::detail::bind_handler( handler, qqnum ) );
			}
			return ;
		} catch( const pt::json_parser_error & jserr ) {
			std::cerr <<  __FILE__ << " : " << __LINE__ << " : " << "parse json error : " <<  jserr.what() <<  std::endl;
		} catch( const pt::ptree_bad_path & badpath ) {
			std::cerr <<  __FILE__ << " : " << __LINE__ << " : " <<  "bad path" <<  badpath.what() << std::endl;
			js::write_json( std::cout, jsonobj );
		}

		_io_service.post( boost::asio::detail::bind_handler( handler, std::string( "" ) ) );
	}
private:
	boost::asio::io_service& _io_service;
};

class SYMBOL_HIDDEN update_group_member_qq : boost::coro::coroutine {
public:
	typedef void result_type;

	update_group_member_qq( WebQQ & _webqq, boost::shared_ptr<qqGroup> _group )
		: group( _group ), m_webqq( _webqq ) {
		m_webqq.get_ioservice().post( boost::bind( *this, "" ) );
	}

	void operator()( std::string qqnum )
	{
		//我说了是一个一个的更新对吧，可不能一次发起　N 个连接同时更新，会被TX拉黑名单的.
		reenter( this )
		{
			for( it = group->memberlist.begin();
					it != group->memberlist.end(); it++ ) {
				_yield buddy_uin_to_qqnumber( m_webqq, it->second.uin, *this );
				if ( qqnum == "-1")
					return;
				it->second.qqnum = qqnum;
			}
		}
	}
private:
	std::map< std::string, qqBuddy >::iterator it;
	boost::shared_ptr<qqGroup>	group;
	WebQQ&						m_webqq;
};

//　将组成员的 QQ 号码一个一个更新过来.
void WebQQ::update_group_member_qq(boost::shared_ptr<qqGroup> group )
{
	::update_group_member_qq( *this, group );
}

qqGroup_ptr WebQQ::get_Group_by_gid( std::string gid )
{
	grouplist::iterator it = m_groups.find( gid );

	if( it != m_groups.end() )
		return it->second;

	return qqGroup_ptr();
}

qqGroup_ptr WebQQ::get_Group_by_qq( std::string qq )
{
	grouplist::iterator it = m_groups.begin();

	for( ; it != m_groups.end(); it ++ ) {
		if( it->second->qqnum == qq )
			return it->second;
	}

	return qqGroup_ptr();
}


void WebQQ::get_verify_image( std::string vcimgid )
{
	if( vcimgid.length() < 8 ) {
		boost::delayedcallsec( m_io_service, 10, boost::bind( &WebQQ::login, this ) );
		return ;
	}

	std::string url = boost::str(
						  boost::format( LWQQ_URL_VERIFY_IMG ) % APPID % m_qqnum
					  );

	read_streamptr stream( new avhttp::http_stream( m_io_service ) );
	stream->request_options(
		avhttp::request_opts()
		( avhttp::http_options::cookie, std::string( "chkuin=" ) + m_qqnum )
		( avhttp::http_options::connection, "close" )
	);
	async_http_download( stream, url,
						 boost::bind( &WebQQ::cb_get_verify_image, this, boost::asio::placeholders::error, _2, _3 ) );
}

void WebQQ::cb_get_verify_image( const boost::system::error_code& ec, read_streamptr stream, boost::asio::streambuf& buffer )
{
	detail::update_cookies( &m_cookies, stream->response_options().header_string() , "verifysession", 1 );

	// verify image is now in response
	signeedvc( buffer.data() );
}

void WebQQ::do_poll_one_msg( std::string ptwebqq )
{
	/* Create a POST request */
	std::string msg = boost::str(
						  boost::format( "{\"clientid\":\"%s\",\"psessionid\":\"%s\"}" )
						  % m_clientid
						  % m_psessionid
					  );

	msg = boost::str( boost::format( "r=%s" ) %  url_encode( msg.c_str() ) );

	read_streamptr pollstream( new avhttp::http_stream( m_io_service ) );
	pollstream->request_options( avhttp::request_opts()
								 ( avhttp::http_options::request_method, "POST" )
								 ( avhttp::http_options::cookie, m_cookies.lwcookies )
								 ( "cookie2", "$Version=1" )
								 ( avhttp::http_options::referer, "http://d.web2.qq.com/proxy.html?v=20101025002" )
								 ( avhttp::http_options::request_body, msg )
								 ( avhttp::http_options::content_type, "application/x-www-form-urlencoded; charset=UTF-8" )
								 ( avhttp::http_options::content_length, boost::lexical_cast<std::string>( msg.length() ) )
								 ( avhttp::http_options::connection, "close" )
							   );

	async_http_download( pollstream, "http://d.web2.qq.com/channel/poll2",
						 boost::bind( &WebQQ::cb_poll_msg, this, _1, _2, _3, ptwebqq )
					   );
}

void WebQQ::cb_poll_msg( const boost::system::error_code& ec, read_streamptr stream, boost::asio::streambuf& buf, std::string ptwebqq)
{
	if( ptwebqq != m_cookies.ptwebqq ) {
		return ;
	}

	std::wstring response = utf8_wide( std::string( boost::asio::buffer_cast<const char*>( buf.data() ) , buf.size() ) );

	pt::wptree	jsonobj;

	std::wstringstream jsondata;
	jsondata << response;

	//处理!
	try
	{
		pt::json_parser::read_json( jsondata, jsonobj );
		process_msg( jsonobj );
		//开启新的 poll
		boost::delayedcallms( get_ioservice(), 5, boost::bind( &WebQQ::do_poll_one_msg, this, ptwebqq ) );

	}
	catch( const pt::json_parser_error & jserr )
	{
		std::cerr <<  __FILE__ << " : " << __LINE__ << " : " << "parse json error : " <<  jserr.what() <<  std::endl;
		// 网络可能出了点问题，延时重试.
		boost::delayedcallsec( get_ioservice(), 5, boost::bind( &WebQQ::do_poll_one_msg, this, ptwebqq ) );
	}
	catch( const pt::ptree_bad_path & badpath )
	{
		std::cerr <<  __FILE__ << " : " << __LINE__ << " : " <<  "bad path" <<  badpath.what() << std::endl;
		js::write_json( std::wcout, jsonobj );
		//开启新的 poll
		boost::delayedcallsec( get_ioservice(), 1, boost::bind( &WebQQ::do_poll_one_msg, this, ptwebqq ) );

	}
}

template <class Handler>
class async_cface_fetch_op : boost::coro::coroutine{
	boost::asio::io_service & io_service;
	std::string	group;
	std::string who;
	boost::shared_ptr< std::vector<qqMsg> > p_msg;
	Handler &handler;
	int msg_size;
	int i;
public:
	async_cface_fetch_op(boost::asio::io_service & _io_service, Handler &_handler, std::string _group, std::string _who, std::vector<qqMsg> _msg)
	  : io_service(_io_service), group(_group), who(_who), handler(_handler), p_msg(new std::vector<qqMsg>(_msg)), msg_size(_msg.size())
	{
		i = 0;
		read_streamptr stream;
		boost::asio::streambuf buf;
		(*this)(boost::system::error_code(), stream, buf);
	}

	void operator()(const boost::system::error_code& ec, read_streamptr stream,  boost::asio::streambuf & buf)
	{
		std::string url;
		reenter(this)
		{
			for (i=0;i< msg_size ;i++)
			{
				if ((*p_msg)[i].type == qqMsg::LWQQ_MSG_CFACE){
					url = boost::str(
						boost::format( "http://w.qq.com/cgi-bin/get_group_pic?pic=%s" )
							% url_encode( (*p_msg)[i].cface )
					);
					// fetch url
					stream.reset(new avhttp::http_stream(io_service));
					_yield async_http_download(stream, url, * this);
					// store result to cface_data
					if (!ec || ec == boost::asio::error::eof){
						(*p_msg)[i].cface_data.assign(
							boost::asio::buffer_cast<const char*>(buf.data()), boost::asio::buffer_size(buf.data())
						);
					}
				}
			}
			// 处理完毕 !
 			handler(group, who, *p_msg);
		}
	}
};

template<class Handler>
static void async_cface_fetch(boost::asio::io_service & io_service, Handler & handler, std::string group, std::string who, std::vector<qqMsg> msg)
{
	async_cface_fetch_op<boost::signals2::signal< void ( const std::string group, const std::string who, const std::vector<qqMsg> & )> >
		(io_service, handler, group, who, msg);
}

void WebQQ::process_group_message( const boost::property_tree::wptree& jstree )
{
	std::string group_code = wide_utf8( jstree.get<std::wstring>( L"value.from_uin" ) );
	std::string who = wide_utf8( jstree.get<std::wstring>( L"value.send_uin" ) );

	//parse content
	std::vector<qqMsg>	messagecontent;
	bool has_cface = false;

	BOOST_FOREACH( const pt::wptree::value_type & content, jstree.get_child( L"value.content" ) ) {
		if( content.second.count( L"" ) ) {
			if( content.second.begin()->second.data() == L"font" ) {
				qqMsg msg;
				msg.type = qqMsg::LWQQ_MSG_FONT;
				msg.font = wide_utf8( content.second.rbegin()->second.get<std::wstring> ( L"name" ) );
				messagecontent.push_back( msg );
			} else if( content.second.begin()->second.data() == L"face" ) {
				qqMsg msg;
				msg.type = qqMsg::LWQQ_MSG_FACE;
				int wface = boost::lexical_cast<int>( content.second.rbegin()->second.data() );
				msg.face = facemap[wface];
				messagecontent.push_back( msg );
			} else if( content.second.begin()->second.data() == L"cface" ) {
				qqMsg msg;
				msg.type = qqMsg::LWQQ_MSG_CFACE;
				msg.cface = wide_utf8( content.second.rbegin()->second.get<std::wstring> ( L"name" ) );
				messagecontent.push_back( msg );
				has_cface = true;
			}
		} else {
			//聊天字符串就在这里.
			qqMsg msg;
			msg.type = qqMsg::LWQQ_MSG_TEXT;
			msg.text = wide_utf8( content.second.data() );
			messagecontent.push_back( msg );
		}
	}
	if (has_cface && m_fetch_cface){
		// 发起异步 图片 fetch
		async_cface_fetch(m_io_service, siggroupmessage, group_code, who, messagecontent);
	}else
		siggroupmessage( group_code, who, messagecontent );
}

void WebQQ::process_msg( const pt::wptree &jstree )
{
	//在这里解析json数据.
	int retcode = jstree.get<int>( L"retcode" );

	if( retcode ) {
		if( retcode != 102 ) {
			boost::delayedcallsec( m_io_service, 15, boost::bind( &WebQQ::login, this ) );
		}

		return;
	}

	BOOST_FOREACH( const pt::wptree::value_type & result, jstree.get_child( L"result" ) ) {
		std::string poll_type = wide_utf8( result.second.get<std::wstring>( L"poll_type" ) );

		if( poll_type == "group_message" ) {
			process_group_message( result.second );
		} else if( poll_type == "sys_g_msg" ) {
			//群消息.
			if( result.second.get<std::wstring>( L"value.type" ) == L"group_join" ) {
				//新加列表，reload群列表.
				update_group_list();
			}
		} else if( poll_type == "buddylist_change" ) {
			//群列表变化了，reload列表.
			js::write_json( std::wcout, result.second );
		} else if( poll_type == "kick_message" ) {
			js::write_json( std::wcout, result.second );
			m_status = LWQQ_STATUS_OFFLINE;
			//强制下线了，重登录.
			boost::delayedcallsec( m_io_service, 15, boost::bind( &WebQQ::login, this ) );
		}
	}
}

void WebQQ::cb_group_list( const boost::system::error_code& ec, read_streamptr stream, boost::asio::streambuf& buffer )
{
	pt::ptree	jsonobj;
	std::istream jsondata( &buffer );
	bool retry = false;

	//处理!
	try {
		pt::json_parser::read_json( jsondata, jsonobj );

		//TODO, group list
		if( !( retry = !( jsonobj.get<int>( "retcode" ) == 0 ) ) ) {
			BOOST_FOREACH( pt::ptree::value_type result,
						   jsonobj.get_child( "result" ).get_child( "gnamelist" ) ) {
				boost::shared_ptr<qqGroup>	newgroup(new qqGroup);
				newgroup->gid = result.second.get<std::string>( "gid" );
				newgroup->name = result.second.get<std::string>( "name" );
				newgroup->code = result.second.get<std::string>( "code" );

				if( newgroup->gid[0] == '-' ) {
					retry = true;
					std::cerr <<  __FILE__ << " : " << __LINE__ << " : " <<  "qqGroup get error" << std::endl;
					continue;
				}

				this->m_groups.insert( std::make_pair( newgroup->gid, newgroup ) );
				std::cerr <<  __FILE__ << " : " << __LINE__ << " : " << console_out_str("qq群") << console_out_str(newgroup->gid) <<  console_out_str(newgroup->name) << std::endl;

			}
		}
	} catch( const pt::json_parser_error & jserr ) {
		retry = true;
		std::cerr << __FILE__ << " : " << __LINE__ << " : " <<  "parse json error : " <<  console_out_str(jserr.what()) << std::endl;
	} catch( const pt::ptree_bad_path & badpath ) {
		retry = true;
		std::cerr << __FILE__ << " : " << __LINE__ << " : " <<   "bad path error " <<  badpath.what() << std::endl;
	}

	if( retry ) {
		boost::delayedcallsec( m_io_service, 5, boost::bind( &WebQQ::update_group_list, this ) );
	} else {
		// fetching more budy info.
		BOOST_FOREACH( grouplist::value_type & v, m_groups ) {
			update_group_qqmember( v.second );
			update_group_member( v.second );
		}
	}
}

void WebQQ::cb_group_qqnumber( const boost::system::error_code& ec, read_streamptr stream, boost::asio::streambuf& buffer, boost::shared_ptr<qqGroup> group)
{
	pt::ptree	jsonobj;
	std::istream jsondata( &buffer );

	/**
	 * Here, we got a json object like this:
	 * {"retcode":0,"result":{"uiuin":"","account":615050000,"uin":954663841}}
	 *
	 */
	//处理!
	try {
		pt::json_parser::read_json( jsondata, jsonobj );

		//TODO, group members
		if( jsonobj.get<int>( "retcode" ) == 0 ) {
			group->qqnum = jsonobj.get<std::string>( "result.account" );
			std::cerr <<  "qq number of group" <<  console_out_str(group->name) << "is" <<  group->qqnum << std::endl;
			// 写缓存
			pt::json_parser::write_json(std::string("cache_group_qqnumber") + group->gid, jsonobj);
			//start polling messages, 2 connections!
			std::cerr << "start polling messages" <<  std::endl;

			boost::delayedcallsec( get_ioservice(), 3, boost::bind( &WebQQ::do_poll_one_msg, this, m_cookies.ptwebqq ) );

			siggroupnumber(group);

			return ;
		}else{
			std::cerr << console_out_str("获取群的QQ号码失败") <<  std::endl;
			pt::json_parser::write_json(std::cerr, jsonobj);
		}
	} catch( const pt::json_parser_error & jserr ) {

	} catch( const pt::ptree_bad_path & badpath ) {
	}

	try{
	// 读取缓存
		pt::json_parser::read_json(std::string("cache_group_qqnumber") + group->gid, jsonobj);

		group->qqnum = jsonobj.get<std::string>( "result.account" );
		std::cerr <<  "(cached) qq number of group" <<  console_out_str(group->name) << "is" <<  group->qqnum << std::endl;

		boost::delayedcallsec( get_ioservice(), 3, boost::bind( &WebQQ::do_poll_one_msg, this, m_cookies.ptwebqq ) );

		// 向用户报告一个 group 出来了.
		siggroupnumber(group);
		return;
	}catch (...){
		boost::delayedcallsec( m_io_service, 50 + boost::rand48()() % 100 , boost::bind( &WebQQ::update_group_qqmember, this, group) );
	}
}

void WebQQ::cb_group_member_process_json(pt::ptree &jsonobj, boost::shared_ptr<qqGroup> group)
{
	//TODO, group members
	if( jsonobj.get<int>( "retcode" ) == 0 ) {
		group->owner = jsonobj.get<std::string>( "result.ginfo.owner" );

		BOOST_FOREACH( pt::ptree::value_type & v, jsonobj.get_child( "result.minfo" ) ) {
			qqBuddy buddy;
			pt::ptree & minfo = v.second;
			buddy.nick = minfo.get<std::string>( "nick" );
			buddy.uin = minfo.get<std::string>( "uin" );

			group->memberlist.insert( std::make_pair( buddy.uin, buddy ) );
		}

		BOOST_FOREACH( pt::ptree::value_type & v, jsonobj.get_child( "result.ginfo.members" ) ) {
			pt::ptree & minfo = v.second;
			std::string muin = minfo.get<std::string>( "muin" );
			std::string mflag = minfo.get<std::string>( "mflag" );

			try {
				group->get_Buddy_by_uin( muin )->mflag = boost::lexical_cast<unsigned int>( mflag );
			} catch( boost::bad_lexical_cast & e ) {}
		}
		try {
			BOOST_FOREACH( pt::ptree::value_type & v, jsonobj.get_child( "result.cards" ) )
			{
				pt::ptree & minfo = v.second;
				std::string muin = minfo.get<std::string>( "muin" );
				std::string card = minfo.get<std::string>( "card" );
				group->get_Buddy_by_uin( muin )->card = card;
			}
		} catch( const pt::ptree_bad_path & badpath ) {
		}
	}
}


void WebQQ::cb_group_member( const boost::system::error_code& ec, read_streamptr stream, boost::asio::streambuf& buffer, boost::shared_ptr<qqGroup> group)
{
	//处理!
	try {

		pt::ptree jsonobj;
		std::istream jsondata( &buffer );

		pt::json_parser::read_json( jsondata, jsonobj );

		cb_group_member_process_json(jsonobj, group);

		pt::json_parser::write_json( std::string("cache_group_") + console_out_str(group->name) , jsonobj );

		// 开始更新成员的 QQ 号码，一次更新一个，慢慢来.
		this->update_group_member_qq( group );

	} catch( const pt::json_parser_error & jserr ) {
		std::cerr <<  __FILE__ << " : " << __LINE__ << " : " <<  "parse json error : " <<  jserr.what() << std::endl;

		boost::delayedcallsec( m_io_service, 20, boost::bind( &WebQQ::update_group_member, this, group) );
		// 在重试之前，获取缓存文件.
		try{
		pt::ptree jsonobj;
		pt::json_parser::read_json(std::string("cache_group_") + group->name , jsonobj);
		cb_group_member_process_json(jsonobj, group);
		}catch (...){}
	} catch( const pt::ptree_bad_path & badpath ) {
	}
}

void WebQQ::cb_fetch_aid(const boost::system::error_code& ec, read_streamptr stream,  boost::asio::streambuf & buf, boost::function<void(const boost::system::error_code&, std::string)> handler)
{
	if (!ec || ec == boost::asio::error::eof)
	{
		// 获取到咯, 更新 verifysession
		detail::update_cookies(&m_cookies, stream->response_options().header_string(), "verifysession", 1);

		handler(boost::system::error_code(), std::string(boost::asio::buffer_cast<const char*>(buf.data()), boost::asio::buffer_size(buf.data())));
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
	stream->request_options(
		avhttp::request_opts()
			(avhttp::http_options::referer, "http://web.qq.com/")
			(avhttp::http_options::cookie, m_cookies.lwcookies)
			(avhttp::http_options::connection, "close")
	);

	async_http_download(stream, url, boost::bind(&WebQQ::cb_fetch_aid, this, _1, _2, _3, handler ));
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

void WebQQ::cb_search_group(std::string groupqqnum, const boost::system::error_code& ec, read_streamptr stream,  boost::asio::streambuf & buf, webqq::search_group_handler handler)
{
	pt::ptree	jsobj;
	std::istream jsondata(&buf);
	qqGroup_ptr  group;

	if (!ec || ec == boost::asio::error::eof){
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
	stream->request_options(avhttp::request_opts()
		(avhttp::http_options::content_type, "utf-8")
		(avhttp::http_options::referer, "http://cgi.web2.qq.com/proxy.html?v=20110412001&callback=1&id=2")
		(avhttp::http_options::cookie, m_cookies.lwcookies)
		(avhttp::http_options::connection, "close")
	);
	async_http_download(stream, url, boost::bind(&WebQQ::cb_search_group, this, groupqqnum, _1, _2, _3, handler));
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


void WebQQ::cb_join_group( qqGroup_ptr group, const boost::system::error_code& ec, read_streamptr stream, boost::asio::streambuf& buf, webqq::join_group_handler handler )
{
	// 检查返回值是不是 retcode == 0
	pt::ptree	jsobj;
	std::istream jsondata(&buf);
	try{
		js::read_json(jsondata, jsobj);
		js::write_json(std::cerr, jsobj);

		if(jsobj.get<int>("retcode") == 0){
			// 搞定！群加入咯. 等待管理员吧.
			handler(group, 0, "");
			// 获取群的其他信息
			// GET http://s.web2.qq.com/api/get_group_public_info2?gcode=3272859045&vfwebqq=f08e7a200fd0be375d753d3fedfd24e99f6ba0a8063005030bb95f9fa4b7e0c30415ae74e77709e3&t=1365161430494 HTTP/1.1
		}else if(jsobj.get<int>("retcode") == 100001){
			std::cout << console_out_str("原因：") <<   jsobj.get<std::string>("tips") <<  std::endl;
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
								% group->code % vfcode % m_cookies.verifysession % m_vfwebqq
							);

	postdata = std::string("r=") + url_encode(postdata);

	read_streamptr stream(new avhttp::http_stream(m_io_service));
	stream->request_options(avhttp::request_opts()
		(avhttp::http_options::http_version, "HTTP/1.0")
		(avhttp::http_options::content_type, "application/x-www-form-urlencoded; charset=UTF-8")
		(avhttp::http_options::referer, "http://s.web2.qq.com/proxy.html?v=20110412001&callback=1&id=1")
		(avhttp::http_options::cookie, m_cookies.lwcookies)
		(avhttp::http_options::connection, "close")
		(avhttp::http_options::request_method, "POST")
		(avhttp::http_options::request_body, postdata)
		(avhttp::http_options::content_length, boost::lexical_cast<std::string>(postdata.length()))
	);
	async_http_download(stream, url, boost::bind(&WebQQ::cb_join_group, this, group, _1, _2, _3, handler));
}


static std::string parse_unescape( std::string source )
{
	boost::replace_all( source, "\\", "\\\\\\\\" );
	boost::replace_all( source, "\r", "" );
	boost::replace_all( source, "\n", "\\\\r\\\\n" );
	boost::replace_all( source, "\t", "\\\\t" );
	boost::replace_all( source, "\"", "\\\\u0022" );
	boost::replace_all( source, "&", "\\\\u0026" );
	boost::replace_all( source, "\'", "\\\\u0027" );
	boost::replace_all( source, ":", "\\\\u003A" );
	boost::replace_all( source, ";", "\\\\u003B" );
	boost::replace_all( source, "+", "\\\\u002B" );
	boost::replace_all( source, "%", "\\\\u0025" );
	boost::replace_all( source, "`", "\\\\u0060" );
	boost::replace_all( source, "[", "\\\\u005B" );
	boost::replace_all( source, "]", "\\\\u005D" );
	boost::replace_all( source, ",", "\\\\u002C" );
	boost::replace_all( source, "{", "\\\\u007B" );
	boost::replace_all( source, "}", "\\\\u007D" );
	return source;
}
