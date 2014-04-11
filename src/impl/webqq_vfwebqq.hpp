
#pragma once

#include <boost/log/trivial.hpp>
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

template<class Handler>
class update_vfwebqq_op
{
public:
	update_vfwebqq_op(boost::shared_ptr<qqimpl::WebQQ> webqq, Handler handler)
		: m_webqq(webqq), m_handler(handler)
	{
		std::string url =  boost::str(
			boost::format("http://s.web2.qq.com/api/getvfwebqq?ptwebqq=%s&clientid=%ld&psessionid=%s")
			% m_webqq->m_cookie_mgr.get_cookie("http://s.web2.qq.com/api/getvfwebqq")["ptwebqq"]
			% m_webqq->m_clientid
			% m_webqq->m_psessionid
		);

		stream.reset( new avhttp::http_stream( m_webqq->get_ioservice() ) );
		stream->http_cookies(m_webqq->m_cookie_mgr.get_cookie(url));
		stream->request_options(
			avhttp::request_opts()
			( avhttp::http_options::referer, "http://s.web2.qq.com/proxy.html?v=20130916001&callback=1&id=1" )
			( avhttp::http_options::content_type, "UTF-8" )
			( avhttp::http_options::connection, "close" )
		);

		m_buffer = boost::make_shared<boost::asio::streambuf>();
		avhttp::async_read_body(* stream, url , *m_buffer, *this );
	}

	void operator()( const boost::system::error_code& ec, std::size_t bytes_transfered)
	{
		std::string msg;
		pt::ptree json;
		std::istream response( m_buffer.get() );

		if (!ec){
		//　登录步骤.
		//处理!
		try {
			js::read_json( response, json );

			if( json.get<std::string>( "retcode" ) == "0" ) {
				m_webqq->m_vfwebqq = json.get<std::string>( "result.vfwebqq" );

				m_webqq->m_cookie_mgr.save_cookie("psession.qq.com", "/", "vfwebqq", m_webqq->m_vfwebqq, "session");

				m_webqq->m_status = LWQQ_STATUS_ONLINE;

				m_webqq->get_ioservice().post(
					boost::asio::detail::bind_handler(m_handler,boost::system::error_code())
				);
				return;
			}
		} catch( const pt::ptree_error & jserr ) {
			BOOST_LOG_TRIVIAL(error) <<  __FILE__ << " : " <<__LINE__ << " :" << "parse bad path error : " <<  jserr.what();
		}
		}

		m_handler(error::make_error_code(error::failed_to_change_status));
	}
private:
	boost::shared_ptr<qqimpl::WebQQ> m_webqq;
	read_streamptr stream;
	Handler m_handler;
	boost::shared_ptr<boost::asio::streambuf> m_buffer;
};

template<class Handler>
update_vfwebqq_op<Handler>
make_update_vfwebqq_op(boost::shared_ptr<qqimpl::WebQQ> webqq, Handler handler)
{
	return update_vfwebqq_op<Handler>(webqq, handler);
}

} // nsmespace detail

template<class Handler>
void  async_update_vfwebqq(boost::shared_ptr<qqimpl::WebQQ> webqq, Handler handler)
{
	detail::make_update_vfwebqq_op(webqq, handler);
}

} // nsmespace qqimpl
} // namespace webqq