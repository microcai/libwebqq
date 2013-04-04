/*
 * Copyright (C) 2012  微蔡 <microcai@fedoraproject.org>
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

#ifndef WEBQQ_IMPL_H
#define WEBQQ_IMPL_H

#if defined _WIN32 || defined __CYGWIN__
#define SYMBOL_HIDDEN
#else
#if __GNUC__ >= 4
#define SYMBOL_HIDDEN  __attribute__ ((visibility ("hidden")))
#else
#define SYMBOL_HIDDEN
#endif
#endif


#include <string>
#include <map>
#include <queue>
#include <boost/tuple/tuple.hpp>
#include <boost/circular_buffer.hpp>
#include <boost/asio.hpp>
#include <boost/signal.hpp>
#include <boost/concept_check.hpp>
#include <boost/system/system_error.hpp>
#include <boost/property_tree/ptree.hpp>
namespace pt = boost::property_tree;

#include <avhttp.hpp>

#include "webqq.h"

#include "httpagent.hpp"

#if !defined(_MSC_VER)
#pragma GCC visibility push(hidden)
#endif

namespace qq {

typedef enum LwqqMsgType {
	LWQQ_MT_BUDDY_MSG = 0,
	LWQQ_MT_GROUP_MSG,
	LWQQ_MT_DISCU_MSG,
	LWQQ_MT_SESS_MSG, //group whisper message
	LWQQ_MT_STATUS_CHANGE,
	LWQQ_MT_KICK_MESSAGE,
	LWQQ_MT_SYSTEM,
	LWQQ_MT_BLIST_CHANGE,
	LWQQ_MT_SYS_G_MSG,
	LWQQ_MT_OFFFILE,
	LWQQ_MT_FILETRANS,
	LWQQ_MT_FILE_MSG,
	LWQQ_MT_NOTIFY_OFFFILE,
	LWQQ_MT_INPUT_NOTIFY,
	LWQQ_MT_UNKNOWN,
} LwqqMsgType;

typedef enum {
	LWQQ_MC_OK = 0,
	LWQQ_MC_TOO_FAST = 108,             //< send message too fast
	LWQQ_MC_LOST_CONN = 121
} LwqqMsgRetcode;

typedef struct LwqqVerifyCode {
	std::string img;
	std::string uin;
	std::string data;
	size_t size;
} LwqqVerifyCode;

typedef struct LwqqCookies {
	std::string ptvfsession;          /**< ptvfsession */
	std::string ptcz;
	std::string skey;
	std::string ptwebqq;
	std::string ptuserinfo;
	std::string uin;
	std::string ptisp;
	std::string pt2gguin;
	std::string verifysession;
	std::string lwcookies;
	void clear() {
		ptvfsession.clear();          /**< ptvfsession */
		ptcz.clear();
		skey.clear();
		ptwebqq.clear();
		ptuserinfo.clear();
		uin.clear();
		ptisp.clear();
		pt2gguin.clear();
		verifysession.clear();
		lwcookies.clear();
	}
} LwqqCookies;

typedef std::map<std::string, qqGroup>	grouplist;

namespace detail {
class corologin;
class corologin_vc;
}

class SYMBOL_HIDDEN WebQQ {
public:
	WebQQ( boost::asio::io_service & asioservice, std::string qqnum, std::string passwd, LWQQ_STATUS status = LWQQ_STATUS_ONLINE );

	// not need to call this the first time, but you might need this if you became offline.
	void login();
	// login with vc, call this if you got signeedvc signal.
	// in signeedvc signal, you can retreve images from server.
	void login_withvc( std::string vccode );

	typedef boost::function<void ( const boost::system::error_code& ec )> send_group_message_cb;
	void send_group_message( std::string group, std::string msg, send_group_message_cb donecb );
	void send_group_message( qqGroup &  group, std::string msg, send_group_message_cb donecb );
	void update_group_list();
	void update_group_qqmember( qqGroup& group );
	void update_group_member( qqGroup &  group );

	qqGroup * get_Group_by_gid( std::string gid );
	qqGroup * get_Group_by_qq( std::string qq );
	boost::asio::io_service	&get_ioservice() {
		return m_io_service;
	};

public:// signals
	// 登录成功激发.
	boost::signal< void ()> siglogin;
	// 验证码, 需要自行下载url中的图片，然后调用 login_withvc.
	boost::signal< void ( const boost::asio::const_buffer & )> signeedvc;
	// 断线的时候激发.
	boost::signal< void ()> sigoffline;

	// 发生错误的时候激发, 返回 false 停止登录，停止发送，等等操作。true则重试.
	boost::signal< bool ( int stage, int why )> sigerror;

	// 有群消息的时候激发.
	boost::signal< void ( const std::string group, const std::string who, const std::vector<qqMsg> & )> siggroupmessage;

private:
	void init_face_map();

	void update_group_member_qq( qqGroup & group );

	void get_verify_image( std::string vcimgid );
	void cb_get_verify_image( const boost::system::error_code& ec, read_streamptr stream, boost::asio::streambuf& );

	void do_poll_one_msg( std::string cookie );
	void cb_poll_msg( const boost::system::error_code& ec, read_streamptr stream, boost::asio::streambuf& buf, std::string cookie );

	void process_msg( const pt::wptree & jstree );
	void process_group_message( const pt::wptree & jstree );
	void cb_group_list( const boost::system::error_code& ec, read_streamptr stream, boost::asio::streambuf& );
	void cb_group_member( const boost::system::error_code& ec, read_streamptr stream, boost::asio::streambuf&, qqGroup & );
	void cb_group_qqnumber( const boost::system::error_code& ec, read_streamptr stream, boost::asio::streambuf&, qqGroup & );

	void send_group_message_internal( std::string group, std::string msg, send_group_message_cb donecb );
	void cb_send_msg( const boost::system::error_code& ec, read_streamptr stream, boost::asio::streambuf&, boost::function<void ( const boost::system::error_code& ec )> donecb );

public:
	std::string	m_vfwebqq;
	LwqqCookies m_cookies;

private:
	boost::asio::io_service & m_io_service;

	std::string m_qqnum, m_passwd;
	LWQQ_STATUS m_status;

	std::string	m_version;
	std::string m_clientid, m_psessionid;
	long m_msg_id;     // update on every message.

	LwqqVerifyCode m_verifycode;

	grouplist	m_groups;

	bool		m_group_msg_insending;
	boost::circular_buffer<boost::tuple<std::string, std::string, send_group_message_cb> >	m_msg_queue;
	std::map<int, int> facemap;

	friend class ::webqq;
	friend class detail::corologin;
	friend class detail::corologin_vc;
};

};

#if !defined(_MSC_VER)
#pragma GCC visibility pop
#endif


#endif // WEBQQ_H
