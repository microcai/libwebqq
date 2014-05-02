

/*
 * Copyright (C) 2013  微蔡 <microcai@fedoraproject.org>
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

/*
 * cookie manager
 *
 *
 * this is an simple cookie manager class that designed to work with WebQQ in mind.
 *
 * What is a cookie?  A cookie is simply an k/v value with a little bit extra
 * information such as domain and expiries time.
 *
 * With this in mind, I developed an cookie manager for use with avhttp. The cookie
 * is stored individuly, when you access the domain, then the cookie manager collect
 * all cookies needed to access the URI.
 *
 */

#pragma once

#include <string>
#include <ctime>
#include <algorithm>

#include <boost/system/error_code.hpp>
#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>

#include <boost/format.hpp>
#include <boost/foreach.hpp>
#include <boost/regex.hpp>
#include "boost/date_time/gregorian/gregorian.hpp"
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/date.hpp>
#include <boost/lexical_cast.hpp>

#include <soci-sqlite3.h>
#include <boost-optional.h>
#include <boost-tuple.h>
#include <boost-gregorian-date.h>
#include <soci.h>

#include <avhttp/url.hpp>
#include <avhttp/http_stream.hpp>

namespace cookie {

/* 管理和存储 cookie, WebQQ 最重要的任务就是保护这么一个对象*/
class cookie_store : boost::noncopyable
{
	soci::session db;

	void check_db_initialized()
	{
		soci::transaction trans(db);
		db <<
			"create table if not exists cookies ("
				"`domain` TEXT not null,"
				"`path` TEXT not null default \"/\", "
				"`name` TEXT not null, "
				"`value` TEXT not null default \"\", "
				"`expiration` TEXT not null default \"session\""
			");";
		trans.commit();
	}

	void save_cookie(const avhttp::cookies & cookies, std::string domain)
	{
		std::vector< boost::tuple<std::string, std::string, std::string> > inserted;

		BOOST_FOREACH(avhttp::cookies::http_cookie cookie, cookies)
		{
			bool shall_delete = cookie.value.empty();

			if (cookie.domain.empty())
				cookie.domain = domain;
			if (cookie.path.empty())
				cookie.path = "/";

			if ( !shall_delete && cookie.expires.is_not_a_date_time() )
			{
				// 根据　expires 确定是否为删除操作
				shall_delete = cookie.expires < boost::posix_time::second_clock::local_time();
			}

			if (shall_delete)
			{
				// 检查是否在　inserted 有了！　如果有了，则删除操作是不可以的。
				if (
					std::find(
						inserted.begin(), inserted.end(),
						boost::make_tuple(cookie.domain, cookie.path, cookie.name)
					) == inserted.end()
				)
				{
					// 可删！
					delete_cookie(cookie.domain, cookie.path, cookie.name);
				}
			}else{
				// 更新到数据库！
				save_cookie(cookie.domain, cookie.path, cookie.name, cookie.value, to_str(cookie.expires));
				inserted.push_back(boost::make_tuple(cookie.domain, cookie.path, cookie.name));
			}
		}
	}

public:
	// drop session cookies with the domain name,
	// if "" then drop session cookies of all domains
	void drop_session(const std::string & domain = std::string())
	{
		// TODO check for % and \" \' charactor
		//db << "delete from cookies";

 		std::string sqlstmt =
 			boost::str(boost::format("delete from cookies where expiration = \"session\" and domain like \"%%%s\"") % domain);
		db <<  sqlstmt;
	}

 	cookie_store(const std::string & dbpath = std::string(":memory:"))
	{
		db.open(soci::sqlite3, dbpath);
		check_db_initialized();
	}

	static std::string build_domain_conditions(std::string domain)
	{
		bool prepend_gama = false;
		std::stringstream condition;

		condition << "domain in (";
		do{
			if (prepend_gama)
				condition <<  " ,  " ;
			condition <<  "\"" << domain << "\" , \"." <<  domain <<  "\" ";
			prepend_gama = true;
			if (domain.find_first_of('.')!= std::string::npos)
			{
				domain = domain.substr(domain.find_first_of('.')+1);
			}
		}while (domain.find_first_of('.') != std::string::npos );
		condition << ")";
		return condition.str();
	}

	static std::string build_path_conditions(std::string path)
	{
		bool prepend_gama = false;
		std::stringstream condition;

		// 砍掉 ?
		std::size_t pos = path.find_first_of('?');
		if (pos != std::string::npos){
			path = path.substr(0, pos);
		}

		condition << "path in ( \"/\"";
		do{
			condition <<  ", \"" << path << "\"";
			if (path.find_last_of('/')!= std::string::npos)
			{
				path = path.substr(0, path.find_last_of('/'));
			}
		}while (path.find_last_of('/') != std::string::npos );
		condition << ")";
		return condition.str();
	}

	// 以 url 对象为参数调用就可以获得这个请求应该带上的 cookie
	//
	avhttp::cookies get_cookie(const avhttp::url & url)
	{
		// 遍历 cookie store, 找到符合 PATH 和 domain 要求的 cookie
		// 然后构建 cookie 对象.

		std::vector<std::string> names(100);
		std::vector<std::string> values(100);

		std::vector<soci::indicator> inds_names;
		std::vector<soci::indicator> inds_values;

		std::string sql = boost::str(
			boost::format("select name, value from cookies where %s and %s")
				% build_domain_conditions(url.host())
				% build_path_conditions(url.path())
		);

		db << sql , soci::into(names, inds_names), soci::into(values, inds_values) ;

		avhttp::cookies cookie;

		for (int i = 0; i < names.size() ; i++)
			cookie(names[i], values[i]);

		return cookie;
	}

	// 直接设置 cookie
	void get_cookie(const avhttp::url & url, avhttp::http_stream & avstream)
	{
		avstream.http_cookies(get_cookie(url));
	}

	// 直接设置 cookie
	void get_cookie(avhttp::http_stream & avstream)
	{
		get_cookie(avstream.final_url(), avstream);
	}

	// 调用以设置 cookie, 这个是其中一个重载, 用于从 http_stream 获取 set-cookie 头
	void save_cookie(const avhttp::http_stream & stream)
	{
		save_cookie(stream.http_cookies(), avhttp::url(stream.final_url()).host());
	}

	// 详细参数直接设置一个 cookie
	void save_cookie(std::string domain, std::string path, std::string name, std::string value, std::string expiration)
	{
		using namespace soci;
		transaction transac(db);

		db<< "delete from cookies where domain = :domain and path = :path and name = :name"
			, use(domain), use(path), use(name);

		db << "insert into cookies (domain, path, name, value, expiration) values ( :name  , :value  , :domain, :path , :expiration)"
			, use(domain), use(path), use(name), use(value), use(expiration) ;

		transac.commit();
	}

	void delete_cookie(std::string domain, std::string path, std::string name)
	{
		using namespace soci;
		db<< "delete from cookies where domain = :domain and path = :path and name = :name"
		  , use(domain), use(path), use(name);
	}
protected:
	std::string to_str(const boost::posix_time::ptime & expires) const
	{
		if (expires.is_not_a_date_time())
			return "session";

		std::stringstream ss;

		boost::posix_time::time_input_facet* rfc850_date =
			new boost::posix_time::time_input_facet("%A, %d-%b-%y %H:%M:%S GMT");
		ss.imbue(std::locale(ss.getloc(), rfc850_date));

		ss <<  expires;

		return ss.str();
	}
};

} // namespace cookie
