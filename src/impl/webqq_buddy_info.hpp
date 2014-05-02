
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
#include <boost/array.hpp>
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
#include "webqq_group_qqnumber.hpp"
#include "webqq_hash.hpp"

namespace webqq {
namespace qqimpl {
namespace detail {

template<class Handler>
class update_buddy_list_op : boost::asio::coroutine
{
    static std::string hashP(std::string uin,std::string ptwebqq)
	{
		return hash_func_P(uin, ptwebqq);
	}

	std::string create_post_data(std::string vfwebqq)
	{
		std::string m = boost::str(
			boost::format( "{\"hash\":\"%s\", \"vfwebqq\":\"%s\"}" )
			% hashP(m_webqq->m_myself_uin,
				m_webqq->m_cookie_mgr.get_cookie(WEBQQ_S_HOST "/api/get_user_friends2")["ptwebqq"]
			)
			% vfwebqq
		);
		return std::string("r=") + avhttp::detail::escape_string(m);
	}

	void do_fetch()
	{
		m_stream = boost::make_shared<avhttp::http_stream>(boost::ref(m_webqq->get_ioservice()));
		m_buffer = boost::make_shared<boost::asio::streambuf>();

		BOOST_LOG_TRIVIAL(debug) << "getting buddy list";

		/* Create post data: {"h":"hello","vfwebqq":"4354j53h45j34"} */
		std::string postdata = create_post_data(m_webqq->m_vfwebqq);
		std::string url =  WEBQQ_S_HOST "/api/get_user_friends2";

		m_webqq->m_cookie_mgr.get_cookie(url, *m_stream);

		m_stream->request_options(
			avhttp::request_opts()
			(avhttp::http_options::request_method, "POST")
			(avhttp::http_options::referer, WEBQQ_S_REF_URL)
			(avhttp::http_options::content_type, "application/x-www-form-urlencoded; charset=UTF-8")
			(avhttp::http_options::request_body, postdata)
			(avhttp::http_options::content_length, boost::lexical_cast<std::string>(postdata.length()))
			(avhttp::http_options::connection, "close")
		);

		avhttp::async_read_body(*m_stream, url, *m_buffer, *this);
	}

public:
	update_buddy_list_op(const boost::shared_ptr<WebQQ>& webqq, Handler handler)
		: m_webqq(webqq)
		, m_handler(handler)
	{
		do_fetch();
	}

	/**
	* Here, we got a json object like this:
	* {"retcode":0,"result":{
	* "friends":[{"flag":0,"uin":1907104721,"categories":0},i...
	* "marknames":[{"uin":276408653,"markname":""},...
	* "categories":[{"index":1,"sort":1,"name":""},...
	* "vipinfo":[{"vip_level":1,"u":1907104721,"is_vip":1},i...
	* "info":[{"face":294,"flag":8389126,"nick":"","uin":1907104721},
	*
	*/
	void operator()(boost::system::error_code ec, std::size_t bytes_transfered)
	{
 		pt::ptree	jsonobj;
 		std::istream jsondata(m_buffer.get());
 		bool errored = false;

		try{
			js::read_json(jsondata, jsonobj);

			if(jsonobj.get<int>("retcode") == 0)
			{
				grouplist newlist;
				bool replace_list = false;

				BOOST_FOREACH(pt::ptree::value_type result, jsonobj.get_child("result.friends"))
				{
					std::string uin = result.second.get<std::string>("uin");
					int flag = result.second.get<int>("flag");
					int categories = result.second.get<int>("categories");
					m_webqq->m_buddy_mgr.new_buddy(uin, flag, categories);
				}

				try {BOOST_FOREACH(pt::ptree::value_type result, jsonobj.get_child("result.marknames"))
				{
					std::string uin = result.second.get<std::string>("uin");
					std::string markname = result.second.get<std::string>("markname");
					m_webqq->m_buddy_mgr.buddy_update_markname(uin, markname);
				}}catch (const pt::ptree_error&){}

				try {BOOST_FOREACH(pt::ptree::value_type result, jsonobj.get_child("result.categories"))
				{
					int index =  result.second.get<int>("index");
					int sort =  result.second.get<int>("sort");
					std::string name =  result.second.get<std::string>("name");

					m_webqq->m_buddy_mgr.new_catgory(index, sort, name);
				}}catch (const pt::ptree_error&){}

				try {BOOST_FOREACH(pt::ptree::value_type result, jsonobj.get_child("result.info"))
				{
					std::string uin = result.second.get<std::string>("uin");
					std::string nick = result.second.get<std::string>("nick");

					m_webqq->m_buddy_mgr.buddy_update_nick(uin, nick);
				}}catch (const pt::ptree_error&){}

			}
		}catch (const pt::ptree_error &){errored = true;}

		if (errored){
			m_handler(error::make_error_code(error::failed_to_fetch_buddy_list));
			return;
		}

		// and then update_group_detail
		m_handler(boost::system::error_code());
	}

private:
	boost::shared_ptr<qqimpl::WebQQ> m_webqq;
	Handler m_handler;

	read_streamptr m_stream;
	boost::shared_ptr<boost::asio::streambuf> m_buffer;
};

template<class Handler>
update_buddy_list_op<Handler>
make_update_buddy_list_op(const boost::shared_ptr<WebQQ> & _webqq, Handler handler)
{
	return update_buddy_list_op<Handler>(_webqq, handler);
}

} // namespace detail

template<class Handler>
void async_update_buddy_list(const boost::shared_ptr<WebQQ>& _webqq, Handler handler)
{
	detail::make_update_buddy_list_op(_webqq, handler);
}

} // namespace qqimpl
} // namespace webqq
