
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
#include <boost/property_tree/json_parser.hpp>
namespace js = boost::property_tree::json_parser;
#include <boost/regex.hpp>

#include <avhttp.hpp>
#include <avhttp/async_read_body.hpp>

#include <boost/hash.hpp>

#include "boost/timedcall.hpp"
#include "boost/urlencode.hpp"
#include "boost/consolestr.hpp"

#include "webqq_impl.hpp"

#include "constant.hpp"

#include "lwqq_cookie.hpp"

namespace webqq {
namespace qqimpl {
namespace detail {


inline std::string lutil_md5_data(const std::string & data)
{
	boost::hashes::md5::digest_type md5sum ;
	md5sum = boost::hashes::compute_digest<boost::hashes::md5>(data);
	return md5sum.str();
}

inline std::string lutil_md5_digest(const std::string & data)
{
	boost::hashes::md5::digest_type md5sum ;
	md5sum = boost::hashes::compute_digest<boost::hashes::md5>(data);
	return std::string(reinterpret_cast<const char*>(md5sum.c_array()), md5sum.static_size);
}

static void upcase_string( char *str, int len )
{
	int i;

	for( i = 0; i < len; ++i ) {
		if( islower( str[i] ) )
			str[i] = toupper( str[i] );
	}
}

static std::string parse_version( boost::asio::streambuf& buffer )
{
	const char* response = boost::asio::buffer_cast<const char*> ( buffer.data() );

	if( strstr( response, "ptuiV" ) ) {
		char* s, *t;
		s = ( char* ) strchr( response, '(' );
		t = ( char* ) strchr( response, ')' );

		if( !s || !t ) {
			return "";
		}

		s++;
		std::vector<char> v( t - s + 1 );
		strncpy( v.data(), s, t - s );

		BOOST_LOG_TRIVIAL(info) << "Get webqq version: " <<  v.data();
		return v.data();
	}

	return "";
}

// ptui_checkVC('0','!IJG, ptui_checkVC('0','!IJG', '\x00\x00\x00\x00\x54\xb3\x3c\x53');
static std::string parse_verify_uin( const char *str )
{
	const char *start;
	const char *end;

	start = strchr( str, '\\' );

	if( !start )
		return "";

	end = strchr( start, '\'' );

	if( !end )
		return "";

	return std::string( start, end - start );
}

/**
 * I hacked the javascript file named comm.js, which received from tencent
 * server, and find that fuck tencent has changed encryption algorithm
 * for password in webqq3 . The new algorithm is below(descripted with javascript):
 * var M=C.p.value; // M is the qq password
 * var I=hexchar2bin(md5(M)); // Make a md5 digest
 * var H=md5(I+pt.uin); // Make md5 with I and uin(see below)
 * var G=md5(H+C.verifycode.value.toUpperCase());
 *
 * @param pwd User's password
 * @param vc Verify Code. e.g. "!M6C"
 * @param uin A string like "\x00\x00\x00\x00\x54\xb3\x3c\x53", NB: it
 *        must contain 8 hexadecimal number, in this example, it equaled
 *        to "0x0,0x0,0x0,0x0,0x54,0xb3,0x3c,0x53"
 *
 * @return Encoded password on success, else NULL on failed
 */
static std::string lwqq_enc_pwd( const std::string & pwd, const std::string & vc, const std::string &uin )
{
	int i;
	int uin_byte_length;
	char _uin[9] = {0};

	/* Calculate the length of uin (it must be 8?) */
	uin_byte_length = uin.length() / 4;

	/**
	 * Ok, parse uin from string format.
	 * "\x00\x00\x00\x00\x54\xb3\x3c\x53" -> {0,0,0,0,54,b3,3c,53}
	 */
	for( i = 0; i < uin_byte_length ; i++ ) {
		char u[5] = {0};
		char tmp;
		strncpy( u, & uin [  i * 4 + 2 ] , 2 );

		errno = 0;
		tmp = strtol( u, NULL, 16 );

		if( errno ) {
			return NULL;
		}

		_uin[i] = tmp;
	}

	/* Equal to "var I=hexchar2bin(md5(M));" */
	std::string I =  lutil_md5_digest( pwd);

	/* Equal to "var H=md5(I+pt.uin);" */
	std::string H = lutil_md5_data( I + std::string(_uin, uin_byte_length));

	/* Equal to var G=md5(H+C.verifycode.value.toUpperCase()); */
	std::string G = H + (vc);
	boost::to_upper(G);
	std::string final =  lutil_md5_data(G);
	boost::to_upper(final);
	return final;
}

static std::string generate_clientid()
{
	int r;
	struct timeval tv;
	long t;

	srand( time( NULL ) );
	r = rand() % 90 + 10;

#ifdef WIN32
	return boost::str( boost::format( "%d%d%d" ) % r % r % r );
#else

	if( gettimeofday( &tv, NULL ) ) {
		return NULL;
	}

	t = tv.tv_usec % 1000000;
	return boost::str( boost::format( "%d%ld" ) % r % t );
#endif
}

// qq 登录办法
class SYMBOL_HIDDEN check_login_op : boost::asio::coroutine {
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
	void operator()( const boost::system::error_code& ec, std::size_t bytes_transfered )
	{
		BOOST_ASIO_CORO_REENTER( this )
		{
			m_webqq->m_version = parse_version( *buffer );

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
				( avhttp::http_options::cookie, boost::str( boost::format( "chkuin=%s" ) % m_webqq->m_qqnum ) )
				( avhttp::http_options::connection, "close" )
			);

			BOOST_ASIO_CORO_YIELD avhttp::async_read_body( *stream,
										/*url*/ boost::str( boost::format( "%s%s?uin=%s&appid=%s" ) % LWQQ_URL_CHECK_HOST % VCCHECKPATH % m_webqq->m_qqnum % APPID ),
										*buffer,
										*this );

			// 解析验证码，然后带验证码登录.
			parse_verify_code(bytes_transfered);
		}
	}

	void parse_verify_code(std::size_t bytes_transfered)
	{
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
		std::string response;
		response.resize(bytes_transfered);
		buffer->sgetn(&response[0], bytes_transfered);
		boost::cmatch what;

		boost::regex ex("ptui_checkVC\\('([0-9])',[ ]?'([0-9a-zA-Z!]*)'");

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

	void operator()(boost::system::error_code ec, std::string vc)
	{
		if (!ec)
			ec = error::login_failed_need_vc;
		m_handler(ec, vc);
	}
private:
	boost::shared_ptr<qqimpl::WebQQ> m_webqq;
	webqq::webqq_handler_string_t m_handler;

	read_streamptr stream;
	boost::shared_ptr<boost::asio::streambuf> buffer;
};

// qq 登录办法-验证码登录
class SYMBOL_HIDDEN corologin_vc : boost::asio::coroutine {
public:
	typedef void result_type;

	corologin_vc( boost::shared_ptr<qqimpl::WebQQ> webqq, std::string _vccode )
		: m_webqq( webqq ), vccode( _vccode ) {
		std::string md5 = lwqq_enc_pwd( m_webqq->m_passwd.c_str(), vccode.c_str(), m_webqq->m_verifycode.uin.c_str() );

		// do login !
		std::string url = boost::str(
							  boost::format(
								  "%s/login?u=%s&p=%s&verifycode=%s&"
								  "webqq_type=%d&remember_uin=1&aid=%s&login2qq=1&"
								  "u1=http%%3A%%2F%%2Fweb.qq.com%%2Floginproxy.html"
								  "%%3Flogin2qq%%3D1%%26webqq_type%%3D10&h=1&ptredirect=0&"
								  "ptlang=2052&from_ui=1&pttype=1&dumy=&fp=loginerroralert&"
								  "action=2-11-7438&mibao_css=m_webqq&t=1&g=1" )
							  % LWQQ_URL_LOGIN_HOST
							  % m_webqq->m_qqnum
							  % md5
							  % vccode
							  % m_webqq->m_status
							  % APPID
						  );

		stream = boost::make_shared<avhttp::http_stream>( boost::ref( m_webqq->get_ioservice() ) );
		stream->request_options(
			avhttp::request_opts()
			( avhttp::http_options::cookie, m_webqq->m_cookies.lwcookies )
			( avhttp::http_options::connection, "close" )
		);

		buffer = boost::make_shared<boost::asio::streambuf>();

		avhttp::async_read_body( *stream, url, *buffer, *this );
	}

	// 在这里实现　QQ 的登录.
	void operator()( const boost::system::error_code& ec, std::size_t bytes_transfered)
	{
		std::istream response( buffer.get());
		if( ( check_login( ec, *buffer ) == 0 ) && ( m_webqq->m_status == LWQQ_STATUS_ONLINE ) )
		{
			m_webqq->siglogin();
			m_webqq->m_clientid = generate_clientid();
			//change status,  this is the last step for login
			// 设定在线状态.
			m_webqq->change_status(LWQQ_STATUS_ONLINE, *this);
		}else
		{
			m_webqq->get_ioservice().post(m_webqq->m_funclogin);
		}
	}
	
	void operator()(const boost::system::error_code& ec)
	{
		if(ec)
		{
			// 修改在线状态失败!
		}
		else
		{
			//polling group list
			m_webqq->update_group_list();
			
			// 每 10 分钟修改一下在线状态.
			lwqq_update_status(m_webqq, m_webqq->m_cookies.ptwebqq);

			m_webqq->m_group_msg_insending = !m_webqq->m_msg_queue.empty();

			if( m_webqq->m_group_msg_insending )
			{
				boost::tuple<std::string, std::string, WebQQ::send_group_message_cb> v = m_webqq->m_msg_queue.front();
				boost::delayedcallms( m_webqq->get_ioservice(), 500, boost::bind( &WebQQ::send_group_message_internal, m_webqq->shared_from_this(), boost::get<0>( v ), boost::get<1>( v ), boost::get<2>( v ) ) );
				m_webqq->m_msg_queue.pop_front();
			}
		}
	}

private:
	int check_login( const boost::system::error_code& ec, boost::asio::streambuf & buffer )
	{
		const char * response = boost::asio::buffer_cast<const char*>( buffer.data() );

		BOOST_LOG_TRIVIAL(debug) << console_out_str(response);

		char *p = strstr( ( char* )response, "\'" );

		if( !p ) {
			return -1;
		}

		char buf[4] = {0};

		strncpy( buf, p + 1, 1 );
		int status = atoi( buf );

		switch( status ) {
			case 0:
				m_webqq->m_status = LWQQ_STATUS_ONLINE;
				save_cookie( &( m_webqq->m_cookies ), stream->response_options().header_string() );
				BOOST_LOG_TRIVIAL(info) <<  "login success!";
				break;

			case 1:
				BOOST_LOG_TRIVIAL(info) << "Server busy! Please try again";

				status = LWQQ_STATUS_OFFLINE;
				break;
			case 2:
				BOOST_LOG_TRIVIAL(info) << "Out of date QQ number";

				status = LWQQ_STATUS_OFFLINE;
				break;


			case 3:
				BOOST_LOG_TRIVIAL(info) << "Wrong QQ password";
				status = LWQQ_STATUS_OFFLINE;
				exit(1);
				break;

			case 4:
				BOOST_LOG_TRIVIAL(info) << "!!!!!!!!!! Wrong verify code !!!!!!!!";
				status = LWQQ_STATUS_OFFLINE;
				break;

			case 5:
   				BOOST_LOG_TRIVIAL(info) << "!!!!!!!!!! Verify failed !!!!!!!!";
				status = LWQQ_STATUS_OFFLINE;
				break;


			case 6:
				BOOST_LOG_TRIVIAL(info) << "!!!!!!!!!! You may need to try login again !!!!!!!!";
				status = LWQQ_STATUS_OFFLINE;
				break;

			case 7:
				BOOST_LOG_TRIVIAL(info) << "!!!!!!!!!! Wrong input !!!!!!!!";
				status = LWQQ_STATUS_OFFLINE;
				break;


			case 8:
				BOOST_LOG_TRIVIAL(info) << "!!!!!!!!!! Too many logins on this IP. Please try again !!!!!!!!";
				status = LWQQ_STATUS_OFFLINE;
				break;


			default:
				BOOST_LOG_TRIVIAL(error) << "!!!!!!!!!! Unknow error!!!!!!!!";
				status = LWQQ_STATUS_OFFLINE;
		}

		return status;
	}

private:
	boost::shared_ptr<qqimpl::WebQQ> m_webqq;
	read_streamptr stream;
	boost::shared_ptr<boost::asio::streambuf> buffer;
	std::string vccode;
};

}
}
}
