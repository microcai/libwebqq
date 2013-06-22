
#pragma once

#include <boost/function.hpp>
#include <boost/asio.hpp>
#include <boost/property_tree/json_parser.hpp>
namespace js = boost::property_tree::json_parser;

#include "boost/timedcall.hpp"
#include "boost/coro/coro.hpp"
#include "boost/coro/yield.hpp"

#include "webqq.h"

#include "webqq_impl.h"

#include "httpagent.hpp"

#include "constant.hpp"
#include "md5.hpp"
#include "url.hpp"
#include "boost/consolestr.hpp"

namespace qqimpl {
namespace detail {

static std::string lwqq_status_to_str( LWQQ_STATUS status )
{
	switch( status ) {
		case LWQQ_STATUS_ONLINE: return "online"; break;
		case LWQQ_STATUS_OFFLINE: return "offline"; break;
		case LWQQ_STATUS_AWAY: return "away"; break;
		case LWQQ_STATUS_HIDDEN: return "hidden"; break;
		case LWQQ_STATUS_BUSY: return "busy"; break;
		case LWQQ_STATUS_CALLME: return "callme"; break;
		case LWQQ_STATUS_SLIENT: return "slient"; break;
		default: return "unknow"; break;
	}
}

class lwqq_change_status
{
public:
	lwqq_change_status(qqimpl::WebQQ & webqq, LWQQ_STATUS status, boost::function<void (boost::system::error_code) > handler)
		: m_webqq(webqq), m_handler(handler)
	{
		read_streamptr stream;

		std::string msg = boost::str(
					boost::format( "{\"status\":\"%s\",\"ptwebqq\":\"%s\","
									"\"passwd_sig\":""\"\",\"clientid\":\"%s\""
									", \"psessionid\": \"%s\"}" )
					% lwqq_status_to_str( LWQQ_STATUS_ONLINE )
					% m_webqq.m_cookies.ptwebqq
					% m_webqq.m_clientid
					% (m_webqq.m_psessionid.empty() ? std::string("null") : m_webqq.m_psessionid)
				);

		msg = boost::str( boost::format( "r=%s" ) % url_encode( msg.c_str() ) );

		stream.reset( new avhttp::http_stream( m_webqq.get_ioservice() ) );
		stream->request_options(
			avhttp::request_opts()
			( avhttp::http_options::request_method, "POST" )
			( avhttp::http_options::cookie, m_webqq.m_cookies.lwcookies )
			( avhttp::http_options::referer, "http://d.web2.qq.com/proxy.html?v=20110331002&callback=1&id=2" )
			( avhttp::http_options::content_type, "application/x-www-form-urlencoded; charset=UTF-8" )
			( avhttp::http_options::request_body, msg )
			( avhttp::http_options::content_length, boost::lexical_cast<std::string>( msg.length() ) )
			( avhttp::http_options::connection, "close" )
		);

		async_http_download( stream, LWQQ_URL_SET_STATUS , * this );
	}

	void operator()( const boost::system::error_code& ec, read_streamptr stream, boost::asio::streambuf & buffer )
	{
		std::string msg;
		pt::ptree json;
		std::istream response( &buffer );

		//　登录步骤.
		//处理!
		try {
			js::read_json( response, json );
			js::write_json( std::cout, json );

			if( json.get<std::string>( "retcode" ) == "0" ) {
				m_webqq.m_psessionid = json.get_child( "result" ).get<std::string>( "psessionid" );
				m_webqq.m_vfwebqq = json.get_child( "result" ).get<std::string>( "vfwebqq" );
				m_webqq.m_status = LWQQ_STATUS_ONLINE;

				m_handler(boost::system::error_code());
				return;
			}
		} catch( const pt::json_parser_error & jserr ) {
			std::cerr <<  __FILE__ << " : " <<__LINE__ << " :" <<  "parse json error :" <<  jserr.what() <<  std::endl;
		} catch( const pt::ptree_bad_path & jserr ) {
			std::cerr <<  __FILE__ << " : " <<__LINE__ << " :" << "parse bad path error : " <<  jserr.what() <<  std::endl;
		}

		if(ec)
			m_handler(ec);
		else
			m_handler(boost::system::errc::make_error_code(boost::system::errc::protocol_error));
	}
private:
	qqimpl::WebQQ & m_webqq;
	boost::function<void (boost::system::error_code) > m_handler;
};

// 用于更新在线状态, 每 10 分钟更新一下.
class lwqq_update_status{
public:
	lwqq_update_status(qqimpl::WebQQ & webqq, std::string ptwebqq)
		:m_webqq(webqq), m_ptwebqq(ptwebqq)
	{
		boost::delayedcallsec(m_webqq.get_ioservice(), 600, *this);
	}

	void operator()()
	{
		if(m_webqq.m_cookies.ptwebqq == m_ptwebqq)
		{
			lwqq_change_status(m_webqq, LWQQ_STATUS_ONLINE, *this);
		}
	}

	void operator()(boost::system::error_code ec)
	{
		if(m_webqq.m_cookies.ptwebqq == m_ptwebqq)
			boost::delayedcallsec(m_webqq.get_ioservice(), 600, *this);
	}

private:
	qqimpl::WebQQ & m_webqq;
	std::string m_ptwebqq;
};

}
}