
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
#include <boost/function.hpp>
#include <boost/asio.hpp>
#include <avhttp.hpp>

#include "boost/coro/coro.hpp"
#include "boost/coro/yield.hpp"

#include "httpagent.hpp"

#include "webqq_impl.h"

#include "constant.hpp"

namespace qq {
namespace detail {

static std::string parse_version(boost::asio::streambuf& buffer)
{
	const char* response = boost::asio::buffer_cast<const char*> ( buffer.data() );

	if ( strstr ( response, "ptuiV" ) )
	{
		char* s, *t;
		s = ( char* ) strchr ( response, '(' );
		t = ( char* ) strchr ( response, ')' );

		if ( !s || !t ) {
			return "";
		}

		s++;
		char v[t - s + 1];
		memset ( v, 0, t - s + 1 );
		strncpy ( v, s, t - s );
		
		std::cout << "Get webqq version: " <<  v <<  std::endl;
		return v;
	}	
}

// ptui_checkVC('0','!IJG, ptui_checkVC('0','!IJG', '\x00\x00\x00\x00\x54\xb3\x3c\x53');
static std::string parse_verify_uin(const char *str)
{
    const char *start;
    const char *end;

    start = strchr(str, '\\');
    if (!start)
        return "";

    end = strchr(start,'\'');
    if (!end)
        return "";

    return std::string(start, end - start);
}


static std::string get_cookie(const std::string & cookie, std::string key)
{
 	std::string searchkey = key + "=";
 	std::string::size_type keyindex = cookie.find(searchkey);
 	if (keyindex == std::string::npos)
 		return "";
 	keyindex +=searchkey.length();
 	std::string::size_type valend = cookie.find("; ", keyindex);
 	return cookie.substr(keyindex , valend-keyindex);
}

static void update_cookies(LwqqCookies *cookies, const std::string & httpheader,
                           std::string key, int update_cache)
{
	std::string value = get_cookie(httpheader, key);
    if (value.empty())
        return ;
    
#define FREE_AND_STRDUP(a, b)    a = b
    
    if (key ==  "ptvfsession") {
        FREE_AND_STRDUP(cookies->ptvfsession, value);
    } else if ((key== "ptcz")) {
        FREE_AND_STRDUP(cookies->ptcz, value);
    } else if ((key== "skey")) {
        FREE_AND_STRDUP(cookies->skey, value);
    } else if ((key== "ptwebqq")) {
        FREE_AND_STRDUP(cookies->ptwebqq, value);
    } else if ((key== "ptuserinfo")) {
        FREE_AND_STRDUP(cookies->ptuserinfo, value);
    } else if ((key== "uin")) {
        FREE_AND_STRDUP(cookies->uin, value);
    } else if ((key== "ptisp")) {
        FREE_AND_STRDUP(cookies->ptisp, value);
    } else if ((key== "pt2gguin")) {
        FREE_AND_STRDUP(cookies->pt2gguin, value);
    } else if ((key== "verifysession")) {
        FREE_AND_STRDUP(cookies->verifysession, value);
    } else {
        lwqq_log(LOG_WARNING, "No this cookie: %s\n", key.c_str());
    }
#undef FREE_AND_STRDUP

    if (update_cache) {
		
		cookies->lwcookies.clear();

        if (cookies->ptvfsession.length()) {
            cookies->lwcookies += "ptvfsession="+cookies->ptvfsession+"; ";
        }
        if (cookies->ptcz.length()) {
			cookies->lwcookies += "ptcz="+cookies->ptcz+"; ";
        }
        if (cookies->skey.length()) {
            cookies->lwcookies += "skey="+cookies->skey+"; ";
        }
        if (cookies->ptwebqq.length()) {
            cookies->lwcookies += "ptwebqq="+cookies->ptwebqq+"; ";
        }
        if (cookies->ptuserinfo.length()) {
			cookies->lwcookies += "ptuserinfo="+cookies->ptuserinfo+"; ";
        }
        if (cookies->uin.length()) {
            cookies->lwcookies += "uin="+cookies->uin+"; ";
        }
        if (cookies->ptisp.length()) {
            cookies->lwcookies += "ptisp="+cookies->ptisp+"; ";
        }
        if (cookies->pt2gguin.length()) {
			cookies->lwcookies += "pt2gguin="+cookies->pt2gguin+"; ";
        }
        if (cookies->verifysession.length()) {
			cookies->lwcookies += "verifysession="+cookies->verifysession+"; ";
        }
    }
}

static void save_cookie(LwqqCookies * cookies, const std::string & httpheader)
{
	update_cookies(cookies, httpheader, "ptcz", 0);
    update_cookies(cookies, httpheader, "skey",  0);
    update_cookies(cookies, httpheader, "ptwebqq", 0);
    update_cookies(cookies, httpheader, "ptuserinfo", 0);
    update_cookies(cookies, httpheader, "uin", 0);
    update_cookies(cookies, httpheader, "ptisp", 0);
    update_cookies(cookies, httpheader, "pt2gguin", 1);
}

// qq 登录办法
class corologin : boost::coro::coroutine {
public:
	corologin(qq::WebQQ & webqq )
		:m_webqq(webqq)
	{
		read_streamptr stream;//(new avhttp::http_stream(m_webqq.get_ioservice()));
		boost::asio::streambuf buf;
		(*this)(boost::system::error_code(), stream, buf);
	}

	// 在这里实现　QQ 的登录.
	void operator()(const boost::system::error_code& ec, read_streamptr stream, boost::asio::streambuf & buf)
	{
		//　登录步骤.
		reenter(this)
		{
			stream.reset(new avhttp::http_stream(m_webqq.get_ioservice()));
			lwqq_log(LOG_DEBUG, "Get webqq version from %s\n", LWQQ_URL_VERSION);
			// 首先获得版本.
			coyield async_http_download(stream, LWQQ_URL_VERSION, *this);

			m_webqq.m_version = parse_version(buf);
			
			// 接着获得验证码.
			stream.reset(new avhttp::http_stream(m_webqq.get_ioservice()));
			
			m_webqq.m_clientid.clear();
			m_webqq.m_cookies.clear();
			m_webqq.m_groups.clear();
			m_webqq.m_psessionid.clear();
			m_webqq.m_vfwebqq.clear();
			m_webqq.m_status = LWQQ_STATUS_OFFLINE;
			//获取验证码.

			stream->request_options(
				avhttp::request_opts()
					(avhttp::http_options::cookie, boost::str(boost::format("chkuin=%s") % m_webqq.m_qqnum))
					(avhttp::http_options::connection, "close")
			);

			coyield async_http_download(stream,
				/*url*/ boost::str(boost::format("%s%s?uin=%s&appid=%s") % LWQQ_URL_CHECK_HOST % VCCHECKPATH % m_webqq.m_qqnum % APPID),
									*this);

			// 解析验证码，然后带验证码登录.
			parse_verify_code(ec, stream , buf);
		}
	}

	void parse_verify_code(const boost::system::error_code& ec, read_streamptr stream, boost::asio::streambuf& buffer)
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
		const char * response = boost::asio::buffer_cast<const char * >(buffer.data());
		char *s;
		char *c = (char*)strstr(response, "ptui_checkVC");
		c = (char*)strchr(response, '\'');
		c++;
		if (*c == '0') {
			/* We got the verify code. */
			
			/* Parse uin first */
			m_webqq.m_verifycode.uin = parse_verify_uin(response);
			if (m_webqq.m_verifycode.uin.empty())
			{
				m_webqq.sigerror(1, 0);
				return;
			}

			s = c;
			c = strstr(s, "'");
			s = c + 1;
			c = strstr(s, "'");
			s = c + 1;
			c = strstr(s, "'");
			*c = '\0';

			/* We need get the ptvfsession from the header "Set-Cookie" */
			update_cookies(&(m_webqq.m_cookies), stream->response_options().header_string(), "ptvfsession", 1);
			lwqq_log(LOG_NOTICE, "Verify code: %s\n", s);

			m_webqq.login_withvc(s);
		} else if (*c == '1') {
			/* We need get the verify image. */

			/* Parse uin first */
			m_webqq.m_verifycode.uin = parse_verify_uin(response);
			s = c;
			c = strstr(s, "'");
			s = c + 1;
			c = strstr(s, "'");
			s = c + 1;
			c = strstr(s, "'");
			*c = '\0';

			// ptui_checkVC('1','7ea19f6d3d2794eb4184c9ae860babf3b9c61441520c6df0', '\x00\x00\x00\x00\x04\x7e\x73\xb2');

			lwqq_log(LOG_NOTICE, "We need verify code image: %s\n", s);

			//TODO, get verify image, and call signeedvc
			m_webqq.get_verify_image(s);
		}
	}
private:
	qq::WebQQ & m_webqq;
};

}
}
