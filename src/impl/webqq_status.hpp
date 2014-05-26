
#pragma once

#include <boost/function.hpp>
#include <boost/asio.hpp>
#include <boost/property_tree/json_parser.hpp>
namespace js = boost::property_tree::json_parser;
#include <boost/asio/yield.hpp>
#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/json_create_escapes_utf8.hpp>

#include <avhttp/async_read_body.hpp>

#include "boost/timedcall.hpp"
#include "boost/urlencode.hpp"

#include "libwebqq/webqq.hpp"

#include "webqq_impl.hpp"

#include "constant.hpp"
#include "boost/stringencodings.hpp"

namespace webqq {
namespace qqimpl {
namespace detail {

static std::string lwqq_status_to_str(LWQQ_STATUS status )
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

template<class Handler>
class change_status_op
{
public:
	change_status_op(boost::shared_ptr<qqimpl::WebQQ> webqq, LWQQ_STATUS status, Handler handler)
		: m_webqq(webqq), m_handler(handler)
	{
		std::stringstream	post_val_r;
		boost::property_tree::ptree r;
		r.put("status", lwqq_status_to_str( LWQQ_STATUS_ONLINE ));

		avhttp::cookies cookies = m_webqq->m_cookie_mgr.get_cookie(LWQQ_URL_SET_STATUS);

		r.put("ptwebqq", cookies["ptwebqq"]);

		r.put("clientid", m_webqq->m_clientid);
		if (m_webqq->m_psessionid.empty())
			r.put_child("psessionid", boost::property_tree::ptree());
		else
			r.put("psessionid", m_webqq->m_psessionid);

		boost::property_tree::json_parser::write_json(post_val_r , r);

		std::string	msg = boost::str(
					boost::format( "r=%s" )
					% avhttp::detail::escape_string(post_val_r.str())
			  );

		stream.reset( new avhttp::http_stream( m_webqq->get_ioservice() ) );
		stream->http_cookies(cookies);
		stream->request_options(
			avhttp::request_opts()
			( avhttp::http_options::request_method, "POST" )
			( avhttp::http_options::referer, "http://d.web2.qq.com/proxy.html?v=20130916001&callback=1&id=2")
			( avhttp::http_options::content_type, "application/x-www-form-urlencoded; charset=UTF-8" )
			( avhttp::http_options::request_body, msg )
			( avhttp::http_options::content_length, boost::lexical_cast<std::string>( msg.length() ) )
			( avhttp::http_options::connection, "close" )
		);
		buf = boost::make_shared<boost::asio::streambuf>();
		avhttp::async_read_body(* stream, LWQQ_URL_SET_STATUS , * buf, * this );
	}

	void operator()( const boost::system::error_code& ec, std::size_t bytes_transfered)
	{
		std::string msg;
		pt::ptree json;
		std::istream response( buf.get() );

		if (!ec){
		//　登录步骤.
		//处理!
		try {
			js::read_json( response, json );

			if( json.get<std::string>( "retcode" ) == "0" ) {
				m_webqq->m_psessionid = json.get<std::string>( "result.psessionid" );
				m_webqq->m_vfwebqq = json.get<std::string>( "result.vfwebqq" );
				m_webqq->m_myself_uin = json.get<std::string>("result.uin");

				m_webqq->m_cookie_mgr.save_cookie("psession.qq.com", "/", "vfwebqq", m_webqq->m_vfwebqq, "session");
				m_webqq->m_cookie_mgr.save_cookie("psession.qq.com", "/", "psessionid", m_webqq->m_psessionid, "session");
				m_webqq->m_cookie_mgr.save_cookie("psession.qq.com", "/", "uin", m_webqq->m_myself_uin, "session");

				m_webqq->m_status = LWQQ_STATUS_ONLINE;

				m_webqq->get_ioservice().post(
					boost::asio::detail::bind_handler(m_handler,boost::system::error_code())
				);
				return;
			}
		} catch( const pt::ptree_error & jserr ) {
			AVLOG_ERR <<  __FILE__ << " : " <<__LINE__ << " :" << "parse bad path error : " <<  jserr.what();
		}
		}

		m_handler(error::make_error_code(error::failed_to_change_status));
	}
private:
	boost::shared_ptr<qqimpl::WebQQ> m_webqq;
	read_streamptr stream;
	Handler m_handler;
	boost::shared_ptr<boost::asio::streambuf> buf;
};

template<class Handler>
change_status_op<Handler>
make_change_status_op(boost::shared_ptr<qqimpl::WebQQ> webqq, LWQQ_STATUS status, Handler handler)
{
	return change_status_op<Handler>(webqq, status, handler);
}

} // nsmespace detail

template<class Handler>
void  async_change_status(boost::shared_ptr<qqimpl::WebQQ> webqq, LWQQ_STATUS status, Handler handler)
{
	detail::make_change_status_op(webqq, status, handler);
}

} // nsmespace qqimpl
} // namespace webqq
