
/*
 * Copyright (C) 2013 - 2013  微蔡 <microcai@fedoraproject.org>
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


#if LWQQ_ENABLE_SSL
//ssl switcher
#define SSL_(ssl,normal) ssl
#else
#define SSL_(ssl,normal) normal
#endif

#define H_ SSL_("https://","http://")
//normal ssl switcher
#define S_(normal) SSL_("ssl.",normal)
//standard http header+ssl switcher
#define H_S_ H_ S_("")

#define WEBQQ_PROXY SSL_("cfproxy.html?v=20110331002&callback=1","proxy.html?v=20110331002&callback=1")

#define WEBQQ_LOGIN_UI_HOST H_ "ui.ptlogin2.qq.com"
#define WEBQQ_CHECK_HOST    H_ S_( "check." ) "ptlogin2.qq.com"
#define WEBQQ_LOGIN_HOST    H_S_ "ptlogin2.qq.com"
#define WEBQQ_CAPTCHA_HOST  H_S_ "captcha.qq.com"
#define WEBQQ_D_HOST        H_" d.web2.qq.com"
#define WEBQQ_S_HOST        "http://s.web2.qq.com"

#define WEBQQ_D_REF_URL     WEBQQ_D_HOST "/" WEBQQ_PROXY
#define WEBQQ_S_REF_URL     WEBQQ_S_HOST "/proxy.html?v=201103311002&callback=1"
#define WEBQQ_LOGIN_REF_URL WEBQQ_LOGIN_HOST "/proxy.html"
#define WEBQQ_VERSION_URL   WEBQQ_LOGIN_UI_HOST "/cgi-bin/ver"

#define WEBQQ_LOGIN_LONG_REF_URL(buf) (snprintf(buf,sizeof(buf),\
            WEBQQ_LOGIN_UI_HOST "/cgi-bin/login?daid=164&target=self&style=5&mibao_css=m_webqq&appid=501004106&enable_qlogin=0&no_verifyimg=1&s_url=http%%3A%%2F%%2Fw.qq.com%%2Floginproxy.html&f_url=loginerroralert&strong_login=1&login_stat=%d&t=%lu",lc->stat,LTIME),buf)



/* URL for webqq login */
#define LWQQ_URL_LOGIN_HOST "https://ssl.ptlogin2.qq.com"
#define LWQQ_URL_CHECK_HOST "https://ssl.ptlogin2.qq.com"
#define LWQQ_URL_CHECK_LOGIN_SIG_HOST "https://ui.ptlogin2.qq.com/cgi-bin/login"
#define LWQQ_URL_VERIFY_IMG "http://captcha.qq.com/getimage?aid=%s&uin=%s"
#define VCCHECKPATH "/check"
#define APPID "501004106"
#define LWQQ_URL_SET_STATUS "http://d.web2.qq.com/channel/login2"

#define LWQQ_URL_POLL_MESSAGE "http://d.web2.qq.com/channel/poll2"
#define LWQQ_URL_SEND_OFFFILE "http://d.web2.qq.com/channel/send_offfile2"

/* URL for get webqq version */
#define LWQQ_URL_VERSION "http://ui.ptlogin2.qq.com/cgi-bin/ver"

#define LWQQ_URL_REFERER_QUN_DETAIL "http://s.web2.qq.com/proxy.html?v=201304220930&id=3"
#define LWQQ_URL_REFERER_DISCU_DETAIL "http://d.web2.qq.com/proxy.html?v=201304220930&id=2"

#define LWQQ_URL_SEND_QUN_MSG "http://d.web2.qq.com/channel/send_qun_msg2"
