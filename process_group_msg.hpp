
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
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/smart_ptr/make_shared_array.hpp>
namespace js = boost::property_tree::json_parser;
#include <boost/make_shared.hpp>

#include <boost/avloop.hpp>

#include <avhttp.hpp>

#include "boost/timedcall.hpp"
#include "boost/coro/coro.hpp"
#include "boost/coro/yield.hpp"

#include "httpagent.hpp"

#include "webqq_impl.h"

#include "constant.hpp"
#include "md5.hpp"
#include "url.hpp"
#include "boost/consolestr.hpp"

#include "utf8.hpp"

namespace qqimpl
{
namespace detail
{

struct process_group_message_op : boost::coro::coroutine
{
	typedef void result_type;

	process_group_message_op(WebQQ & _webqqclient, const boost::property_tree::wptree& jstree )
		: m_jstree(boost::make_shared<boost::property_tree::wptree>(jstree) ), webqqclient(_webqqclient)
	{
		avloop_idle_post(webqqclient.get_ioservice(), boost::bind(*this, boost::system::error_code(), std::string("") ) );
	}

	void operator()(boost::system::error_code ec, std::string url)
	{
		std::string group_code = wide_utf8( m_jstree->get<std::wstring>( L"value.from_uin" ) );
		std::string who = wide_utf8( m_jstree->get<std::wstring>( L"value.send_uin" ) );

		//parse content

		namespace pt = boost::property_tree;

		pt::wptree::value_type * content;

		m_iterator = m_jstree->get_child( L"value.content" ).begin();
		m_iterator_end = m_jstree->get_child( L"value.content" ).end();



		reenter(this)
		{
			for( ; m_iterator != m_iterator_end; m_iterator ++ )
			{
				content = &(*m_iterator);


				if( content->second.count( L"" ) )
				{
					if( content->second.begin()->second.data() == L"font" )
					{
						msg.type = qqMsg::LWQQ_MSG_FONT;
						msg.font = wide_utf8( content->second.rbegin()->second.get<std::wstring> ( L"name" ) );
						messagecontent.push_back( msg );
					}
					else if( content->second.begin()->second.data() == L"face" )
					{
						msg.type = qqMsg::LWQQ_MSG_FACE;
						int wface = boost::lexical_cast<int>( content->second.rbegin()->second.data() );
						msg.face = webqqclient.facemap[wface];
						messagecontent.push_back( msg );
					}
					else if( content->second.begin()->second.data() == L"cface" )
					{
						msg.type = qqMsg::LWQQ_MSG_CFACE;
						msg.cface.uin = who;
						msg.cface.gid = webqqclient.get_Group_by_gid( group_code )->code;

						msg.cface.file_id = wide_utf8( content->second.rbegin()->second.get<std::wstring> ( L"file_id" ) );
						msg.cface.name = wide_utf8( content->second.rbegin()->second.get<std::wstring> ( L"name" ) );
						msg.cface.vfwebqq = webqqclient.m_vfwebqq;
						msg.cface.key = wide_utf8( content->second.rbegin()->second.get<std::wstring> ( L"key" ) );
						msg.cface.server = wide_utf8( content->second.rbegin()->second.get<std::wstring> ( L"server" ) );
						msg.cface.cookie = webqqclient.m_cookies.lwcookies;

						_yield webqq::async_cface_url_final(webqqclient.get_ioservice(), msg.cface, *this );
						msg.cface.gchatpicurl = url;

						messagecontent.push_back( msg );
					}
				}
				else
				{
					//聊天字符串就在这里.
					msg.type = qqMsg::LWQQ_MSG_TEXT;
					msg.text = wide_utf8( content->second.data() );
					messagecontent.push_back( msg );
				}
			}

			webqqclient.siggroupmessage( group_code, who, messagecontent );
		}

	}

private:
	WebQQ & webqqclient;
	boost::shared_ptr<boost::property_tree::wptree> m_jstree;
	boost::property_tree::basic_ptree< std::wstring, std::wstring >::iterator	m_iterator, m_iterator_end;
	std::vector<qqMsg>	messagecontent;
	qqMsg msg;

};

}
}
