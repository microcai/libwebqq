
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
#include <boost/foreach.hpp>
#include <boost/function.hpp>
#include <boost/asio.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
namespace js = boost::property_tree::json_parser;

#include <boost/property_tree/json_parser.hpp>
namespace pt = boost::property_tree;
#include <boost/json_create_escapes_utf8.hpp>

#include <boost/format.hpp>

#include <avhttp.hpp>
#include <avhttp/async_read_body.hpp>

#include <boost/hash.hpp>

#include "boost/timedcall.hpp"
#include "boost/urlencode.hpp"
#include "boost/stringencodings.hpp"

#include "webqq_impl.hpp"
#include "constant.hpp"
#include "lwqq_status.hpp"

namespace webqq{
namespace qqimpl{
namespace detail{

template<class Handler>
class update_group_qqnumber_op : boost::asio::coroutine
{
	void do_fetch()
	{
		m_stream = boost::make_shared<avhttp::http_stream>(boost::ref(m_webqq->get_ioservice()));
		m_buffer = boost::make_shared<boost::asio::streambuf>();

		std::string url = boost::str(
				boost::format("%s/api/get_friend_uin2?tuin=%s&verifysession=&type=1&code=&vfwebqq=%s&t=%ld")
				% "http://s.web2.qq.com"
				% m_this_group->code
				% m_webqq->m_vfwebqq
				% std::time( NULL )
			);

		m_webqq->m_cookie_mgr.get_cookie(url, *m_stream);

		m_stream->request_options(
			avhttp::request_opts()
			( avhttp::http_options::referer, LWQQ_URL_REFERER_QUN_DETAIL )
			( avhttp::http_options::connection, "close" )
		);

		avhttp::async_read_body( *m_stream, url, *m_buffer, *this);
	}

public:
	update_group_qqnumber_op(boost::shared_ptr<WebQQ> webqq,
		boost::shared_ptr<qqGroup> group, Handler handler)
		: m_webqq(webqq)
		, m_this_group(group)
		, m_handler(handler)
	{
		do_fetch();
	}

	void operator()(boost::system::error_code ec, std::size_t bytes_transfered)
	{
		pt::ptree	jsonobj;
		std::istream jsondata( m_buffer.get() );

		/**
		* Here, we got a json object like this:
		* {"retcode":0,"result":{"uiuin":"","account":615050000,"uin":954663841}}
		*
		*/
		//处理!
		try {
			pt::json_parser::read_json( jsondata, jsonobj );

			//TODO, group members
			if( jsonobj.get<int>( "retcode" ) == 0 ) {
				m_this_group->qqnum = jsonobj.get<std::string>( "result.account" );
				BOOST_LOG_TRIVIAL(debug) <<  "qq number of group "
					<<  utf8_to_local_encode(m_this_group->name) << " is " <<  m_this_group->qqnum;

				m_webqq->siggroupnumber(m_this_group);

				m_handler(boost::system::error_code());

				return ;
			}else{
				BOOST_LOG_TRIVIAL(error) << literal_to_localstr("获取群的QQ号码失败");
				pt::json_parser::write_json(std::cerr, jsonobj);
			}
		} catch( const pt::ptree_error & jserr ) {
		}

		// 返回错误
		m_handler(error::make_error_code(error::failed_to_fetch_group_qqnumber));
	}

private:
	boost::shared_ptr<qqimpl::WebQQ> m_webqq;
	boost::shared_ptr<qqGroup> m_this_group;
	Handler m_handler;

	read_streamptr m_stream;
	boost::shared_ptr<boost::asio::streambuf> m_buffer;
};

template<class Handler>
update_group_qqnumber_op<Handler>
make_update_group_qqnumber_op(boost::shared_ptr<WebQQ> webqq, boost::shared_ptr<qqGroup> group, Handler handler)
{
	return update_group_qqnumber_op<Handler>(webqq, group, handler);
}

} // namespace detail


template<class Handler>
void async_update_group_qqnumber(boost::shared_ptr<WebQQ> webqq, boost::shared_ptr<qqGroup> group, Handler handler)
{
	detail::make_update_group_qqnumber_op(webqq, group, handler);
}

} // namespace qqimpl
} // namespace webqq
