

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
 */

#pragma once
#include <avhttp/url.hpp>
#include <avhttp/http_stream.hpp>

namespace cookie{

/*
 * 表示一个 cookie 对象,  可以获取用户 cookie: 的字符串,  也可以继续操纵其存储的 cookie
 */
class cookie{
public:
	// 返回 cookie: 所应该使用的字符串.
	std::string operator()()
	{
	}
};

/* 管理和存储 cookie, WebQQ 最重要的任务就是保护这么一个对象*/
class cookie_store
{
public:
	// 以 url 对象为参数调用就可以获得这个请求应该带上的 cookie
	//
	cookie cookie(const avhttp::url & url)
	{
		// 遍历 cookie store, 找到符合 PATH 和 domain 要求的 cookie
		// 然后构建 cookie 对象.


	}

	// 调用以设置 cookie, 这个是其中一个重载, 用于从 http_stream 获取 set-cookie 头
	void set_cookie(const avhttp::http_stream & stream)
	{

	}

	// 以 set-cookie: 行的字符串设置 cookie
	void set_cookie(std::string domain, std::string set_cookie)
	{
	}

	// 详细参数直接设置一个 cookie,  通常不要使用这个函数!
	void set_cookie(std::string name, std::string value, std::string expiry, std::string path, std::string domain)
	{

	}

};

}