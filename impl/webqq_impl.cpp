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
#include <string>
#include <iostream>

#include <boost/log/trivial.hpp>
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

#include "boost/timedcall.hpp"
#include "boost/consolestr.hpp"
#include "boost/urlencode.hpp"

#include "webqq_impl.hpp"
#include "constant.hpp"

#include "utf8.hpp"
#include "lwqq_status.hpp"
#include "clean_cache.hpp"
#include "webqq_check_login.hpp"
#include "webqq_login.hpp"
#include "webqq_verify_image.hpp"
#include "webqq_group_list.hpp"
#include "webqq_group_qqnumber.hpp"
#include "webqq_poll_message.hpp"
#include "group_message_sender.hpp"

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
		std::string _qqnum, std::string _passwd)
: m_io_service( _io_service ), m_qqnum( _qqnum ), m_passwd( _passwd ), m_status( LWQQ_STATUS_OFFLINE ),
	m_cookie_mgr("webqqcookies"),
	m_vc_queue(_io_service, 1),
	m_group_message_queue(_io_service, 20), // 最多保留最后的20条未发送消息.
	m_group_refresh_queue(_io_service)
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

	if (!boost::filesystem::exists("cache"))
		boost::filesystem::create_directories("cache");
}

/*
 * 这是 webqq 的一个内部循环，也是最重要的一个循环
 */
class internal_loop_op : boost::asio::coroutine
{
public:
	internal_loop_op(boost::asio::io_service & io_service, boost::shared_ptr<WebQQ> _webqq)
	  : m_io_service(io_service), m_webqq(_webqq)
	{
		avloop_idle_post(m_io_service,
			boost::asio::detail::bind_handler(*this,boost::system::error_code()));
	}

	void operator()(boost::system::error_code ec)
	{
		BOOST_ASIO_CORO_REENTER(this)
		{for (;m_webqq->m_status!= LWQQ_STATUS_QUITTING;){
			do{
				// try check_login
				BOOST_ASIO_CORO_YIELD
					detail::check_login( m_webqq,*this );
				if (ec){
					BOOST_LOG_TRIVIAL(error) << "发生错误: " <<  ec.message() <<  " 重试中...";
					BOOST_ASIO_CORO_YIELD boost::delayedcallsec(
						m_io_service, 300, boost::asio::detail::bind_handler(*this,ec));
				}
			}while (ec);

			// then retrive vc, can be pushed by check_login or login_withvc
			BOOST_ASIO_CORO_YIELD m_webqq->m_vc_queue.async_pop(*this);
			// 注意，下一行其实回调已经完成登录了.

			if (ec)
			{
				if (ec == error::login_failed_wrong_vc)
				{
					m_webqq->m_sigbadvc();
				}
				// 查找问题， 报告问题啊！
				if (ec == error::login_failed_wrong_passwd)
				{
					// 密码问题,  直接退出了.
					BOOST_LOG_TRIVIAL(error) << ec.message();
					BOOST_LOG_TRIVIAL(error) << "停止登录, 请修改密码重启 avbot";
					m_webqq->m_status = LWQQ_STATUS_QUITTING;
					return;
				}
				if (ec == error::login_failed_blocked_account)
				{
					BOOST_LOG_TRIVIAL(error) << "300s 后重试...";
					// 帐号冻结, 多等些时间, 嘻嘻
					BOOST_ASIO_CORO_YIELD boost::delayedcallsec(
						m_io_service, 300, boost::asio::detail::bind_handler(*this,ec));
				}

				BOOST_LOG_TRIVIAL(error) << "30s 后重试...";

				BOOST_ASIO_CORO_YIELD boost::delayedcallsec(
						m_io_service, 30, boost::asio::detail::bind_handler(*this,ec));
			}

			// 进入 message 循环.
			while (m_webqq->m_status == LWQQ_STATUS_ONLINE)
			{
				// TODO, 每 12个小时刷新群列表.

				// 获取一次消息。
				BOOST_ASIO_CORO_YIELD m_webqq->async_poll_message(*this);

				// 判断消息处理结果

				if (ec == error::poll_failed_need_login || ec == error::poll_failed_user_kicked_off)
				{
					// 重新登录

					// 延时 60s
					BOOST_ASIO_CORO_YIELD boost::delayedcallsec(
						m_io_service, 60, boost::asio::detail::bind_handler(*this,ec));

					m_webqq->m_status = LWQQ_STATUS_OFFLINE;
				}else if ( ec == error::poll_failed_network_error )
				{
					// 等待等待就好了，等待 12s
					BOOST_ASIO_CORO_YIELD boost::delayedcallsec(
						m_io_service, 12, boost::asio::detail::bind_handler(*this,ec));
				}
			}

			// 掉线了，自动重新进入循环， for (;;) 嘛
		}}
	}

	// check_login 回调到这里
	void operator()(boost::system::error_code ec, std::string vc)
	{
		if (ec)
		{
			if (ec == error::login_check_need_vc)
			{
				m_webqq->m_signeedvc(vc);
				ec = boost::system::error_code();
			}
		}else{
			m_webqq->m_vc_queue.push(vc);
		}
		// 回到上面。
		m_io_service.post(boost::asio::detail::bind_handler(*this, ec));
	}

	void operator()(std::string v)
	{
		BOOST_LOG_TRIVIAL(info) << "vc code is \"" << v << "\"" ;
		// 回调会进入 async_pop 的下一行
		detail::login_vc_op op( m_webqq, v, *this);
	}

private:
	boost::asio::io_service & m_io_service;
	boost::shared_ptr<WebQQ> m_webqq;
};

static internal_loop_op internal_loop(boost::asio::io_service & io_service, boost::shared_ptr<WebQQ> _webqq)
{
	return internal_loop_op(io_service, _webqq);
}

class group_auto_refresh_op : boost::asio::coroutine
{
public:
	group_auto_refresh_op(boost::shared_ptr<WebQQ> _webqq)
		:m_webqq(_webqq)
	{
		m_webqq->m_group_refresh_queue.async_pop(
			boost::bind<void>(*this, boost::system::error_code(), _1)
		);

		m_last_sync = boost::posix_time::from_time_t(std::time(NULL));
	}

	// pop call back
	void operator()(boost::system::error_code ec, WebQQ::group_refresh_queue_type v)
	{
		int type;
		// 检查最后一次同步时间.
		boost::posix_time::ptime curtime = boost::posix_time::from_time_t(std::time(NULL));

		BOOST_ASIO_CORO_REENTER(this)
		{for (;m_webqq->m_status!= LWQQ_STATUS_QUITTING;){

			if (boost::posix_time::time_duration(curtime - m_last_sync).minutes() <= 30)
			{
				// 需要最少休眠 30min 才能再次发起一次，否则会被 TX 拉黑

				BOOST_ASIO_CORO_YIELD
					boost::delayedcallsec(m_webqq->get_ioservice(),
						boost::posix_time::time_duration(curtime - m_last_sync).seconds(),
						boost::asio::detail::bind_handler(*this, ec, v)
					);
			}

			// good, 现在可以更新了.

			// 先检查 type
			type = v.get<1>();


			if (type == 0) // 更新全部.
			{
				BOOST_ASIO_CORO_YIELD m_webqq->update_group_list(
					boost::bind<void>(*this, _1, v)
				);

				if (ec)
				{
					// 重来！，how？ 当然是 push 咯！
					m_webqq->m_group_refresh_queue.push(v);
				}
				else
				{
					// 检查是否刷新了群 GID, 如果是，就要刷新群列表！
					if ( ! m_webqq->m_groups.empty() &&
						! m_webqq->m_groups.begin()->second->memberlist.empty())
					{
						// 刷新群列表！
						// 接着是刷新群成员列表.
						for (iter = m_webqq->m_groups.begin(); iter != m_webqq->m_groups.end(); ++iter)
						{
							BOOST_ASIO_CORO_YIELD
								m_webqq->update_group_member(iter->second , boost::bind<void>(*this, _1, v));

							using namespace boost::asio::detail;
							BOOST_ASIO_CORO_YIELD
								boost::delayedcallms(m_webqq->get_ioservice(), 530, bind_handler(*this, ec, v));
						}
					}
				}
			}
			else if (type == 1)
			{
				if (m_webqq->get_Group_by_gid(v.get<2>()))
				{
					// 更新特定的 group 即可！
					BOOST_ASIO_CORO_YIELD m_webqq->update_group_member(
								m_webqq->get_Group_by_gid(v.get<2>()),
								boost::bind<void>(*this, _1, v)
					);
					if (ec){
						// 应该是群GID都变了，重新刷新
						m_webqq->m_group_refresh_queue.push(
							boost::make_tuple(
								webqq::webqq_handler_t(),
								0,
								std::string(),
								std::string()
							)
						);
					}
				}
			}

			// 更新完毕
			if (v.get<0>()){
				v.get<0>()(ec);
			}

			m_last_sync = boost::posix_time::from_time_t(std::time(NULL));

			// 继续获取，嘻嘻.
			BOOST_ASIO_CORO_YIELD m_webqq->m_group_refresh_queue.async_pop(
				boost::bind<void>(*this, ec, _1)
			);
		}}
	}

private:
	boost::posix_time::ptime m_last_sync;
	boost::shared_ptr<WebQQ> m_webqq;

	grouplist::iterator iter;
};

static group_auto_refresh_op group_auto_refresh(boost::shared_ptr<WebQQ> _webqq)
{
	return group_auto_refresh_op(_webqq);
}

void WebQQ::start_schedule_work()
{
	internal_loop(get_ioservice(), shared_from_this());

	group_message_sender(shared_from_this());

	group_auto_refresh(shared_from_this());
/*
				// 搞一个 GET 的长维护
			std::string url = "http://webqq.qq.com/web2/get_msg_tip?uin=&tp=1&id=0&retype=1&rc=1&lv=3&t=1348458711542";
			read_streamptr get_msg_tip( new avhttp::http_stream( m_io_service ) );
			get_msg_tip->request_options( avhttp::request_opts()
										  ( avhttp::http_options::cookie, m_cookie_mgr.get_cookie(url)() )
										  ( avhttp::http_options::referer, "http://d.web2.qq.com/proxy.html?v=20101025002" )
										  ( avhttp::http_options::connection, "close" )
										);
			boost::shared_ptr<boost::asio::streambuf> buffer = boost::make_shared<boost::asio::streambuf>();

			avhttp::async_read_body( *get_msg_tip, url,
								* buffer, boost::bind(&cb_get_msg_tip, _1, _2, get_msg_tip, buffer));*/


	// 开启个程序去清理过期 cache_* 文件
	// webqq 每天登录 uid 变化,  而不是每次都变化.
	// 所以 cache 有效期只有一天.
	clean_cache(get_ioservice());
}

void WebQQ::stop_schedule_work()
{
	m_group_message_queue.cancele();
	m_group_refresh_queue.cancele();
	m_status = LWQQ_STATUS_QUITTING;
}

// last step of a login process
// and this will be callded every other minutes to prevent foce kick off.
void  WebQQ::change_status(LWQQ_STATUS status, boost::function<void (boost::system::error_code) > handler)
{
	detail::lwqq_change_status op(shared_from_this(), status, handler);
}

void WebQQ::async_poll_message(webqq::webqq_handler_t handler)
{
	// pull one message, if message processed correctly, ec = 0;
	// if not, report the errors
	poll_message(shared_from_this(), handler);
}

void WebQQ::send_group_message( qqGroup& group, std::string msg, send_group_message_cb donecb )
{
	send_group_message( group.gid, msg, donecb );
}

void WebQQ::send_group_message( std::string group, std::string msg, send_group_message_cb donecb )
{
	m_group_message_queue.push(boost::make_tuple( group, msg, donecb ));
}

void WebQQ::update_group_list(webqq::webqq_handler_t handler)
{
	detail::update_group_list_op::make_update_group_list_op(shared_from_this(), handler);
}

void WebQQ::update_group_qqnumber(boost::shared_ptr<qqGroup> group, webqq::webqq_handler_t handler)
{
	detail::update_group_qqnumber_op op(shared_from_this(), group, handler);
}

void WebQQ::update_group_member(boost::shared_ptr<qqGroup> group, webqq::webqq_handler_t handler)
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
		( avhttp::http_options::cookie, m_cookie_mgr.get_cookie(url)() )
		( avhttp::http_options::referer, LWQQ_URL_REFERER_QUN_DETAIL )
		( avhttp::http_options::connection, "close" )
	);

	boost::shared_ptr<boost::asio::streambuf> buffer = boost::make_shared<boost::asio::streambuf>();

	avhttp::async_read_body( *stream, url, * buffer,
						 boost::bind( &WebQQ::cb_group_member, this, _1, stream, buffer, group, handler)
					   );
}

class SYMBOL_HIDDEN buddy_uin_to_qqnumber {
public:
	// 将　qqBuddy 里的　uin 转化为　qq 号码.
	template<class Handler>
	buddy_uin_to_qqnumber( boost::shared_ptr<WebQQ> _webqq, std::string uin, Handler handler )
		: _io_service( _webqq->get_ioservice() ) {
		read_streamptr stream;
		std::string url = boost::str(
							  boost::format( "%s/api/get_friend_uin2?tuin=%s&verifysession=&type=1&code=&vfwebqq=%s" )
							  % "http://s.web2.qq.com" % uin % _webqq->m_vfwebqq
						  );

		stream.reset( new avhttp::http_stream( _webqq->get_ioservice() ) );
		stream->request_options(
			avhttp::request_opts()
			( avhttp::http_options::http_version , "HTTP/1.0" )
			( avhttp::http_options::cookie, _webqq->m_cookie_mgr.get_cookie(url)() )
			( avhttp::http_options::referer, "http://s.web2.qq.com/proxy.html?v=201304220930&callback=1&id=3" )
			( avhttp::http_options::content_type, "UTF-8" )
			( avhttp::http_options::connection, "close" )
		);

		boost::shared_ptr<boost::asio::streambuf> buffer = boost::make_shared<boost::asio::streambuf>();

		avhttp::async_read_body(*stream, url, *buffer, boost::bind<void>( *this, _1, stream, buffer, handler ) );
	}

	template <class Handler>
	void operator()( const boost::system::error_code& ec, read_streamptr stream, boost::shared_ptr<boost::asio::streambuf> buffer, Handler handler )
	{
		// 获得的返回代码类似
		// {"retcode":0,"result":{"uiuin":"","account":2664046919,"uin":721281587}}
		pt::ptree jsonobj;
		std::iostream resultjson( buffer.get() );

		try {
			// 处理.
			pt::json_parser::read_json( resultjson, jsonobj );
			int retcode = jsonobj.get<int>("retcode");
			if (retcode ==  99999 || retcode ==  100000 ){
				_io_service.post( boost::asio::detail::bind_handler( handler, std::string("-1") ) );
			}else{
				std::string qqnum = jsonobj.get<std::string>( "result.account" );

				_io_service.post( boost::asio::detail::bind_handler( handler, qqnum ) );
			}
			return ;
		} catch( const pt::json_parser_error & jserr ) {
			BOOST_LOG_TRIVIAL(error) <<  __FILE__ << " : " << __LINE__ << " : " << "parse json error : " <<  jserr.what();
		} catch( const pt::ptree_bad_path & badpath ) {
			BOOST_LOG_TRIVIAL(error) <<  __FILE__ << " : " << __LINE__ << " : " <<  "bad path" <<  badpath.what();
			js::write_json( std::cout, jsonobj );
		}

		_io_service.post( boost::asio::detail::bind_handler( handler, std::string( "" ) ) );
	}
private:
	boost::asio::io_service& _io_service;
};

class SYMBOL_HIDDEN update_group_member_qq_op : boost::asio::coroutine {
public:
	update_group_member_qq_op( boost::shared_ptr<WebQQ>  _webqq, boost::shared_ptr<qqGroup> _group )
		: group( _group ), m_webqq( _webqq )
	{
		m_webqq->get_ioservice().post( boost::asio::detail::bind_handler(*this, std::string()));
	}

	void operator()( std::string qqnum )
	{
		//我说了是一个一个的更新对吧，可不能一次发起　N 个连接同时更新，会被TX拉黑名单的.
		reenter( this )
		{
			for( it = group->memberlist.begin(); it != group->memberlist.end(); it++ ) {
				if (it->second.qqnum.empty())
				{
					yield buddy_uin_to_qqnumber( m_webqq, it->second.uin, *this );
					if ( qqnum == "-1")
					return;
				}
				it->second.qqnum = qqnum;
			}
		}
	}
private:
	std::map< std::string, qqBuddy >::iterator it;
	boost::shared_ptr<qqGroup> group;
	boost::shared_ptr<WebQQ> m_webqq;
};

//　将组成员的 QQ 号码一个一个更新过来.
void WebQQ::update_group_member_qq(boost::shared_ptr<qqGroup> group )
{
	update_group_member_qq_op op( shared_from_this(), group );
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


void WebQQ::cb_group_member( const boost::system::error_code& ec, read_streamptr stream, boost::shared_ptr<boost::asio::streambuf> buffer, boost::shared_ptr<qqGroup> group, webqq::webqq_handler_t handler)
{
	//处理!
	try {

		pt::ptree jsonobj;
		std::istream jsondata( buffer.get() );

		pt::json_parser::read_json( jsondata, jsonobj );

		cb_group_member_process_json(jsonobj, group);

		pt::json_parser::write_json( std::string("cache/group_") + console_out_str(group->name) , jsonobj );

		// 开始更新成员的 QQ 号码，一次更新一个，慢慢来.
		this->update_group_member_qq( group );

		get_ioservice().post(boost::asio::detail::bind_handler(handler, boost::system::error_code()));

	} catch( const pt::json_parser_error & jserr ) {
		BOOST_LOG_TRIVIAL(error) <<  __FILE__ << " : " << __LINE__ << " : " <<  "parse json error : " <<  jserr.what();

		// 在重试之前，获取缓存文件.
		try{
			pt::ptree jsonobj;
			pt::json_parser::read_json(std::string("cache/group_") + group->name , jsonobj);
			cb_group_member_process_json(jsonobj, group);
			get_ioservice().post(boost::asio::detail::bind_handler(handler, boost::system::error_code()));
		}catch (...){
			boost::delayedcallsec( m_io_service, 20, boost::bind( &WebQQ::update_group_member, shared_from_this(), group, handler) );
		}
	} catch( const pt::ptree_bad_path & badpath ) {
	}
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
	stream->request_options(
		avhttp::request_opts()
			(avhttp::http_options::referer, "http://web.qq.com/")
			(avhttp::http_options::cookie, m_cookie_mgr.get_cookie(url)())
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
	stream->request_options(avhttp::request_opts()
		(avhttp::http_options::content_type, "utf-8")
		(avhttp::http_options::referer, "http://cgi.web2.qq.com/proxy.html?v=201304220930&callback=1&id=2")
		(avhttp::http_options::cookie, m_cookie_mgr.get_cookie(url)())
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
			std::cout << console_out_str("原因： ") <<   jsobj.get<std::string>("tips") <<  std::endl;
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
								% m_cookie_mgr.get_cookie(url).get_value("verifysession")
								% m_vfwebqq
							);

	postdata = std::string("r=") + boost::url_encode(postdata);

	read_streamptr stream(new avhttp::http_stream(m_io_service));
	stream->request_options(avhttp::request_opts()
		(avhttp::http_options::http_version, "HTTP/1.0")
		(avhttp::http_options::content_type, "application/x-www-form-urlencoded; charset=UTF-8")
		(avhttp::http_options::referer, "http://s.web2.qq.com/proxy.html?v=201304220930&callback=1&id=1")
		(avhttp::http_options::cookie, m_cookie_mgr.get_cookie(url)())
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
