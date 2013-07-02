
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
#include <boost/make_shared.hpp>
#include <boost/function.hpp>
#include <boost/asio.hpp>
#include <boost/property_tree/json_parser.hpp>
namespace js = boost::property_tree::json_parser;

#include <avhttp.hpp>
#include <boost/asio/yield.hpp>

#include "boost/timedcall.hpp"


#include "httpagent.hpp"

#include "webqq_impl.h"

#include "constant.hpp"
#include "md5.hpp"
#include "url.hpp"
#include "boost/consolestr.hpp"

namespace qqimpl {
namespace detail {

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

		std::cout << "Get webqq version: " <<  v.data() <<  std::endl;
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
	char buf[128] = {0};
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
	lutil_md5_digest( ( unsigned char * )pwd.c_str(), pwd.length(), ( char * )buf );

	/* Equal to "var H=md5(I+pt.uin);" */
	memcpy( buf + 16, _uin, uin_byte_length );
	lutil_md5_data( ( unsigned char * )buf, 16 + uin_byte_length, ( char * )buf );

	/* Equal to var G=md5(H+C.verifycode.value.toUpperCase()); */
	std::sprintf( buf + strlen( buf ), /*sizeof(buf) - strlen(buf),*/ "%s", vc.c_str() );
	upcase_string( buf, strlen( buf ) );

	lutil_md5_data( ( unsigned char * )buf, strlen( buf ), ( char * )buf );
	upcase_string( buf, strlen( buf ) );

	/* OK, seems like every is OK */
	return buf;
}

static std::string get_cookie( const std::string & cookie, std::string key )
{
	std::string searchkey = key + "=";
	std::string::size_type keyindex = cookie.find( searchkey );

	if( keyindex == std::string::npos )
		return "";

	keyindex += searchkey.length();
	std::string::size_type valend = cookie.find( "; ", keyindex );
	return cookie.substr( keyindex , valend - keyindex );
}

static void update_cookies(LwqqCookies *cookies, const std::string & httpheader,
							std::string key)
{
	std::string value = get_cookie( httpheader, key );

	if( value.empty() )
		return ;

	if( key ==  "RK" )
	{
		cookies->RK =  value ;
	}else if( key ==  "ptvfsession" )
	{
		cookies->ptvfsession = value ;
	}
	else if( ( key == "ptcz" ) )
	{
		cookies->ptcz = value ;
	}
	else if( ( key == "skey" ) )
	{
		cookies->skey = value ;
	}
	else if( ( key == "ptwebqq" ) )
	{
		cookies->ptwebqq = value ;
	}
	else if( ( key == "ptuserinfo" ) )
	{
		cookies->ptuserinfo = value ;
	}
	else if( ( key == "uin" ) )
	{
		cookies->uin = value ;
	}
	else if( ( key == "ptisp" ) )
	{
		cookies->ptisp = value ;
	}
	else if( ( key == "pt2gguin" ) )
	{
		cookies->pt2gguin = value ;
	}
	else if( ( key == "pt4_token" ) )
	{
		cookies->pt4_token = value;
	}
	else if( ( key == "pt4_token" ) )
	{
		cookies->pt4_token =  value;
	}
	else if( ( key == "verifysession" ) )
	{
		cookies->verifysession = value ;
	}
	else if( ( key == "superkey" ) )
	{
		cookies->superkey = value ;
	}
	else if( ( key == "superuin" ) )
	{
		cookies->superuin = value ;
	}
	else if( ( key == "rv2" ) )
	{
		cookies->rv2 = value ;
	}	else
	{
		std::cerr <<  "warring: No this cookie: " <<  key <<  std::endl;
	}
}

static void save_cookie( LwqqCookies * cookies, const std::string & httpheader )
{
	update_cookies( cookies, httpheader, "ptcz" );
	update_cookies( cookies, httpheader, "skey" );
	update_cookies( cookies, httpheader, "ptwebqq" );
	update_cookies( cookies, httpheader, "ptuserinfo" );
	update_cookies( cookies, httpheader, "uin" );
	update_cookies( cookies, httpheader, "ptisp" );
	update_cookies( cookies, httpheader, "pt2gguin" );
	update_cookies( cookies, httpheader, "pt4_token" );
	update_cookies( cookies, httpheader, "ptui_loginuin" );
	update_cookies( cookies, httpheader, "rv2" );
	update_cookies( cookies, httpheader, "RK" );
	update_cookies( cookies, httpheader, "superkey" );
	update_cookies( cookies, httpheader, "superuin" );
	cookies->update();
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
class SYMBOL_HIDDEN corologin : boost::asio::coroutine {
public:
	corologin( boost::shared_ptr<qqimpl::WebQQ> webqq )
		: m_webqq( webqq ) {
		read_streamptr stream;
		( *this )( boost::system::error_code(), 0 );
	}

	// 在这里实现　QQ 的登录.
	void operator()( const boost::system::error_code& ec, std::size_t bytes_transfered ) {
		//　登录步骤.
		reenter( this ) {
			stream = boost::make_shared<avhttp::http_stream>( boost::ref(m_webqq->get_ioservice()));
			buffer = boost::make_shared<boost::asio::streambuf>();

			std::cout << "Get webqq version from " <<  LWQQ_URL_VERSION <<  std::endl;
			// 首先获得版本.
			yield avhttp::misc::async_read_body( *stream, LWQQ_URL_VERSION, * buffer,  *this );

			m_webqq->m_version = parse_version( *buffer );

			// 接着获得验证码.
			stream = boost::make_shared<avhttp::http_stream>(boost::ref(m_webqq->get_ioservice()));

			m_webqq->m_clientid.clear();
			m_webqq->m_cookies.clear();
			m_webqq->m_groups.clear();
			m_webqq->m_psessionid.clear();
			m_webqq->m_vfwebqq.clear();
			m_webqq->m_status = LWQQ_STATUS_OFFLINE;
			//获取验证码.

			stream->request_options(
				avhttp::request_opts()
				( avhttp::http_options::cookie, boost::str( boost::format( "chkuin=%s" ) % m_webqq->m_qqnum ) )
				( avhttp::http_options::connection, "close" )
			);
			buffer = boost::make_shared<boost::asio::streambuf>();

			yield avhttp::misc::async_read_body( *stream,
										/*url*/ boost::str( boost::format( "%s%s?uin=%s&appid=%s" ) % LWQQ_URL_CHECK_HOST % VCCHECKPATH % m_webqq->m_qqnum % APPID ),
										*buffer,
										*this );

			// 解析验证码，然后带验证码登录.
			parse_verify_code(*buffer );
		}
	}

	void parse_verify_code(boost::asio::streambuf& buffer )
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
		const char * response = boost::asio::buffer_cast<const char * >( buffer.data() );
		char *s;
		char *c = ( char* )strstr( response, "ptui_checkVC" );
		c = ( char* )strchr( response, '\'' );
		c++;

		if( *c == '0' ) {
			/* We got the verify code. */

			/* Parse uin first */
			m_webqq->m_verifycode.uin = parse_verify_uin( response );

			if( m_webqq->m_verifycode.uin.empty() ) {
				m_webqq->sigerror( 1, 0 );
				return;
			}

			s = c;
			c = strstr( s, "'" );
			s = c + 1;
			c = strstr( s, "'" );
			s = c + 1;
			c = strstr( s, "'" );
			*c = '\0';

			/* We need get the ptvfsession from the header "Set-Cookie" */
			update_cookies( &( m_webqq->m_cookies ), stream->response_options().header_string(), "ptvfsession" );
			m_webqq->m_cookies.update();

			m_webqq->login_withvc( s );
		} else if( *c == '1' ) {
			/* We need get the verify image. */

			/* Parse uin first */
			m_webqq->m_verifycode.uin = parse_verify_uin( response );
			s = c;
			c = strstr( s, "'" );
			s = c + 1;
			c = strstr( s, "'" );
			s = c + 1;
			c = strstr( s, "'" );
			*c = '\0';

			//TODO, get verify image, and call signeedvc
			m_webqq->get_verify_image( s );
		}
	}
private:
	boost::shared_ptr<qqimpl::WebQQ> m_webqq;
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

		avhttp::misc::async_read_body( *stream, url, *buffer, *this );
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
			m_webqq->get_ioservice().post( boost::bind( &WebQQ::login, m_webqq->shared_from_this() ) );
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

		std::cout << console_out_str(response) << std::endl;
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
				std::cerr <<  "login success!" << std::endl;
				break;

			case 1:
				std::cerr << "Server busy! Please try again" <<  std::endl;

				status = LWQQ_STATUS_OFFLINE;
				break;
			case 2:
				std::cerr << "Out of date QQ number" <<  std::endl;

				status = LWQQ_STATUS_OFFLINE;
				break;


			case 3:
				std::cerr << "Wrong QQ password" <<  std::endl;
				status = LWQQ_STATUS_OFFLINE;
				exit(1);
				break;

			case 4:
				std::cerr << "!!!!!!!!!! Wrong verify code !!!!!!!!" <<  std::endl;
				status = LWQQ_STATUS_OFFLINE;
				break;

			case 5:
   				std::cerr << "!!!!!!!!!! Verify failed !!!!!!!!" <<  std::endl;
				status = LWQQ_STATUS_OFFLINE;
				break;


			case 6:
				std::cerr << "!!!!!!!!!! You may need to try login again !!!!!!!!" <<  std::endl;
				status = LWQQ_STATUS_OFFLINE;
				break;

			case 7:
				std::cerr << "!!!!!!!!!! Wrong input !!!!!!!!" <<  std::endl;
				status = LWQQ_STATUS_OFFLINE;
				break;


			case 8:
				std::cerr << "!!!!!!!!!! Too many logins on this IP. Please try again !!!!!!!!" <<  std::endl;
				status = LWQQ_STATUS_OFFLINE;
				break;


			default:
				std::cerr << "!!!!!!!!!! Unknow error!!!!!!!!" <<  std::endl;
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
