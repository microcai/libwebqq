
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

#include <string>
#include <iostream>

#include <boost/log/trivial.hpp>
#include <boost/make_shared.hpp>
#include <boost/function.hpp>
#include <boost/asio.hpp>
#include <boost/regex.hpp>
#include <boost/urlencode.hpp>

#include <avhttp.hpp>
#include <avhttp/async_read_body.hpp>

#include "webqq_impl.hpp"

#include "constant.hpp"

namespace webqq {
namespace qqimpl {
namespace detail {

// qq 登录办法
template<class Handler>
class SYMBOL_HIDDEN check_login_op : boost::asio::coroutine
{
public:
	check_login_op( boost::shared_ptr<qqimpl::WebQQ> webqq, Handler handler)
		: m_webqq( webqq ), m_handler(handler)
	{
		// 首先登录一下 w.qq.com
		stream = boost::make_shared<avhttp::http_stream>( boost::ref(m_webqq->get_ioservice()));
		m_buffer = boost::make_shared<boost::asio::streambuf>();

		BOOST_LOG_TRIVIAL(debug) << "go w.qq.com";

		avhttp::async_read_body( *stream, "http://w.qq.com", * m_buffer,  *this );
	}

	// 在这里实现　QQ 的登录.
	void operator()(const boost::system::error_code& ec, std::size_t bytes_transfered)
	{
		std::string response;
		response.resize(bytes_transfered);
		m_buffer->sgetn(&response[0], bytes_transfered);

		boost::regex ex, ex2;
		boost::smatch what;
		std::string url;

		BOOST_ASIO_CORO_REENTER(this)
		{
			m_webqq->m_cookie_mgr.save_cookie(*stream);

	  		// 获得版本.
	  		stream = boost::make_shared<avhttp::http_stream>( boost::ref(m_webqq->get_ioservice()));
			m_buffer = boost::make_shared<boost::asio::streambuf>();

	  		BOOST_LOG_TRIVIAL(debug) << "Get webqq version from " <<  LWQQ_URL_VERSION ;
			BOOST_ASIO_CORO_YIELD avhttp::async_read_body(
				*stream, LWQQ_URL_VERSION, *m_buffer, *this
			);

			ex.set_expression("ptuiV\\(([0-9]*)\\);");

			if(boost::regex_search(response, what, ex))
			{
				m_webqq->m_version = what[1];
			}

			BOOST_LOG_TRIVIAL(info) << "Get webqq version: " << m_webqq->m_version;

			// 接着获得验证码.
			m_webqq->m_clientid.clear();
			m_webqq->m_groups.clear();
			m_webqq->m_psessionid.clear();
			m_webqq->m_vfwebqq.clear();
			m_webqq->m_status = LWQQ_STATUS_OFFLINE;

			// 获取　login_sig

			stream = boost::make_shared<avhttp::http_stream>(boost::ref(m_webqq->get_ioservice()));
			m_buffer = boost::make_shared<boost::asio::streambuf>();

			BOOST_ASIO_CORO_YIELD avhttp::async_read_body(*stream,/*url*/
					boost::str(
						boost::format("%s?daid=164&target=self&style=5&mibao_css=m_webqq&appid=%s&enable_qlogin=0&s_url=%s&strong_login=1&login_state=10&t=20131024001")
							% LWQQ_URL_CHECK_LOGIN_SIG_HOST
							% APPID
							% avhttp::detail::escape_string(std::string("http://w.qq.com/proxy.html"))
					),
					*m_buffer,
					*this);

			// 类似这样的
			// 是一个正常的 HTML 文件，　但是内容包含
			// g_login_sig=encodeURIComponent("PpbBnX213jzzSH8*xXyySm9qq1jAnP2uo1fXkGaC5t0ZDaxE5MzSR59qh1EhmjqA");

			if (boost::regex_search(response, what, boost::regex("g_login_sig *= *encodeURIComponent\\(\"([^\"]*)\"\\);")))
			{
				m_webqq->m_login_sig = what[1];
			}
			else if (boost::regex_search(response, what, boost::regex("g_login_sig=encodeURIComponent\\(\"([^\"]*)\"\\);")))
			{
				m_webqq->m_login_sig = what[1];
			}

			BOOST_LOG_TRIVIAL(info) << "Get g_login_sig: " << m_webqq->m_login_sig;

			//获取验证码.

			stream = boost::make_shared<avhttp::http_stream>(boost::ref(m_webqq->get_ioservice()));
			m_buffer = boost::make_shared<boost::asio::streambuf>();

			stream->request_options(
				avhttp::request_opts()
				(avhttp::http_options::connection, "close")
				(avhttp::http_options::referer, "https://ui.ptlogin2.qq.com/cgi-bin/login?daid=164&target=self&style=16&mibao_css=m_webqq&appid=501004106&enable_qlogin=0&no_verifyimg=1&s_url=http%3A%2F%2Fw.qq.com%2Fproxy.html&f_url=loginerroralert&strong_login=1&login_state=10&t=20131024001")
			);

			url = boost::str(boost::format("%s%s?uin=%s&appid=%s&js_ver=10068&js_type=0&login_sig=%s&u1=%s")
                    % LWQQ_URL_CHECK_HOST
                    % VCCHECKPATH % m_webqq->m_qqnum % APPID
                    % m_webqq->m_login_sig
                    % "http%3A%2F%2Fw.qq.com%2Fproxy.html&r=0.026550371946748808"
			);

			m_webqq->m_cookie_mgr.get_cookie(url, *stream);

			BOOST_ASIO_CORO_YIELD avhttp::async_read_body(*stream, url , *m_buffer, *this);

			// 解析验证码，然后带验证码登录.

			/**
			*
			* The http message body has two format:
			*
			* ptui_checkVC('1','9ed32e3f644d968809e8cbeaaf2cce42de62dfee12c14b74', '\x00\x00\x00\x00\x54\xb3\x3c\x53');
			* ptui_checkVC('0','!IJG', '\x00\x00\x00\x00\x54\xb3\x3c\x53');
			* The former means we need verify code image and the second
			* parameter is vc_type.
			* The later means we don't need the verify code image. The second
			* parameter is the verify code. The vc_type is in the header
			* "Set-Cookie".
			*/

			ex.set_expression("ptui_checkVC\\('([0-9])',[ ]?'([0-9a-zA-Z!]*)',[ ]?'([0-9a-zA-Z\\\\]*)'");
			ex2.set_expression("ptui_checkVC\\('([0-9])','([0-9a-zA-Z!]*)','([0-9a-zA-Z\\\\]*)'");

			if (boost::regex_search(response, what, ex) || boost::regex_search(response, what, ex2))
			{
				std::string type = what[1];
				std::string vc = what[2];
				m_webqq->m_verifycode.uin = what[3];

				/* We need get the ptvfsession from the header "Set-Cookie" */
				if(type == "0")
				{
					m_webqq->m_cookie_mgr.save_cookie(*stream);
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
	Handler m_handler;

	read_streamptr stream;
	boost::shared_ptr<boost::asio::streambuf> m_buffer;
};

template<class Handler>
check_login_op<Handler> make_check_login_op(boost::shared_ptr<qqimpl::WebQQ> webqq, Handler handler)
{
	return check_login_op<Handler>(webqq, handler);
}

} // namespace detail

template<class Handler>
void async_check_login(boost::shared_ptr<qqimpl::WebQQ> webqq, Handler handler)
{
	detail::make_check_login_op(webqq, handler);
}

} // namespace qqimpl
} // namespace webqq
