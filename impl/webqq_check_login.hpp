
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

#pragma once

#include <iostream>

#include <boost/log/trivial.hpp>
#include <boost/make_shared.hpp>
#include <boost/function.hpp>
#include <boost/asio.hpp>
#include <boost/regex.hpp>


#include <avhttp.hpp>
#include <avhttp/async_read_body.hpp>

#include "webqq_impl.hpp"

#include "constant.hpp"

#include "lwqq_cookie.hpp"

namespace webqq {
namespace qqimpl {
namespace detail {

inline std::string generate_clientid()
{
	srand( time( NULL ) );
	return boost::str( boost::format( "%d%d%d" ) % (rand() % 90 + 10) % (rand() % 90 + 10) % (rand() % 90 + 10) );
}

// qq 登录办法
class SYMBOL_HIDDEN check_login_op : boost::asio::coroutine{
public:
	check_login_op( boost::shared_ptr<qqimpl::WebQQ> webqq, webqq::webqq_handler_string_t handler)
		: m_webqq( webqq ), m_handler(handler)
	{
		// 首先获得版本.
		stream = boost::make_shared<avhttp::http_stream>( boost::ref(m_webqq->get_ioservice()));
		buffer = boost::make_shared<boost::asio::streambuf>();
		BOOST_LOG_TRIVIAL(debug) << "Get webqq version from " <<  LWQQ_URL_VERSION ;
		avhttp::async_read_body( *stream, LWQQ_URL_VERSION, * buffer,  *this );
	}

	// 在这里实现　QQ 的登录.
	void operator()(const boost::system::error_code& ec, std::size_t bytes_transfered)
	{
		std::string response;
		response.resize(bytes_transfered);
		buffer->sgetn(&response[0], bytes_transfered);

		boost::regex ex;
		boost::cmatch what;

		BOOST_ASIO_CORO_REENTER(this)
		{
			ex.set_expression("ptuiV\\(([0-9]*)\\);");

			if(boost::regex_search(response.c_str(), what, ex))
			{
				m_webqq->m_version = what[1];
			}

			BOOST_LOG_TRIVIAL(info) << "Get webqq version: " << m_webqq->m_version;

			// 接着获得验证码.

			m_webqq->m_clientid.clear();
			m_webqq->m_cookies.clear();
			m_webqq->m_groups.clear();
			m_webqq->m_psessionid.clear();
			m_webqq->m_vfwebqq.clear();
			m_webqq->m_status = LWQQ_STATUS_OFFLINE;
			//获取验证码.

			stream = boost::make_shared<avhttp::http_stream>(boost::ref(m_webqq->get_ioservice()));
			buffer = boost::make_shared<boost::asio::streambuf>();

			stream->request_options(
				avhttp::request_opts()
				(avhttp::http_options::cookie, boost::str(boost::format("chkuin=%s") % m_webqq->m_qqnum))
				(avhttp::http_options::connection, "close")
			);

			BOOST_ASIO_CORO_YIELD avhttp::async_read_body(*stream,
					/*url*/ boost::str(boost::format("%s%s?uin=%s&appid=%s") % LWQQ_URL_CHECK_HOST % VCCHECKPATH % m_webqq->m_qqnum % APPID),
					*buffer,
					*this);

			// 解析验证码，然后带验证码登录.

			/**
			*
			* The http message body has two format:
			*
			* ptui_checkVC('1','9ed32e3f644d968809e8cbeaaf2cce42de62dfee12c14b74');
			* ptui_checkVC('0','!LOB');
			* The former means we need verify code image and the second
			* parameter is vc_type.
			* The later means we don't need the verify code image. The second
			* parameter is the verify code. The vc_type is in the header
			* "Set-Cookie".
			*/

			ex.set_expression("ptui_checkVC\\('([0-9])',[ ]?'([0-9a-zA-Z!]*)'");

			if(boost::regex_search(response.c_str(), what, ex))
			{
				std::string type = what[1];
				std::string vc = what[2];

				/* We need get the ptvfsession from the header "Set-Cookie" */
				if(type == "0")
				{
					update_cookies(&(m_webqq->m_cookies), stream->response_options().header_string(), "ptvfsession");
					m_webqq->m_cookies.update();

					m_handler(boost::system::error_code(), vc);
					return;
				}
				else if(type == "1")
				{
					m_webqq->get_verify_image(vc ,  *this);
					return;
				}
			}

			// 未知错误.
			m_handler(error::make_error_code(error::login_failed_other), std::string());
		}
	}

	void operator()(boost::system::error_code ec, std::string vc)
	{
		if (!ec)
			ec = error::login_check_need_vc;
		m_handler(ec, vc);
	}
private:
	boost::shared_ptr<qqimpl::WebQQ> m_webqq;
	webqq::webqq_handler_string_t m_handler;

	read_streamptr stream;
	boost::shared_ptr<boost::asio::streambuf> buffer;
};

}
}
}
