
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

namespace webqq {
namespace qqimpl {
namespace detail {

template<class S, class E>
struct jsStruct{
	S s;
	E e;

	jsStruct()
	  : s(0), e(0)
	{}

	jsStruct(S _s, E _e)
	  : s(_s), e(_e)
	{}
};

/*
 * _ = function (e, t) {
	this.s = e || 0, this.e = t || 0
}
 */
template<class E, class T>
jsStruct<int, int> hash_func__(E e, T t)
{
	return jsStruct<int, int>(e, t);
}

/*
 * D = function (e) {
	var t = [];
	for (var n = 0; n < e.length; ++n) t.push(e.charCodeAt(n));
	return t
}
 */
template<typename E>
std::vector<typename E::value_type> hash_func_D(E e)
{
	std::vector<typename E::value_type> t;

	for (int n = 0; n < e.length(); ++n)
		t.push_back(e[n]);
	return t;
}

/*
 * H = function (e) {
	var t = ["0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "A", "B", "C", "D", "E", "F"],
		n = "";
	for (var r = 0; r < e.length; r++) n += t[e[r] >> 4 & 15], n += t[e[r] & 15];
	return n
}
 */
template<typename E>
std::string hash_func_H(E e)
{
	std::string t = "0123456789ABCDEF";

	std::string n = "";

	for (int r = 0; r < e.size(); r++)
	{
		n += t[e[r] >> 4 & 15];
		n += t[e[r] & 15];
	}

	return n;
}

/*
 * P = function (e, t) {
	e += "";
	var n = [];
	n[0] = e >> 24 & 255, n[1] = e >> 16 & 255, n[2] = e >> 8 & 255, n[3] = e & 255;
	var r = D(t),
		i = [];
	i.push(new _(0, r.length - 1));
	while (i.length > 0) {
		var s = i.pop();
		if (s.s >= s.e || s.s < 0 || s.e >= r.length) continue;
		if (s.s + 1 == s.e) {
			if (r[s.s] > r[s.e]) {
				var o = r[s.s];
				r[s.s] = r[s.e], r[s.e] = o
			}
		} else {
			var u = s.s,
				a = s.e,
				f = r[s.s];
			while (s.s < s.e) {
				while (s.s < s.e && r[s.e] >= f) s.e--, n[0] = n[0] + 3 & 255;
				s.s < s.e && (r[s.s] = r[s.e], s.s++, n[1] = n[1] * 13 + 43 & 255);
				while (s.s < s.e && r[s.s] <= f) s.s++, n[2] = n[2] - 3 & 255;
				s.s < s.e && (r[s.e] = r[s.s], s.e--, n[3] = (n[0] ^ n[1] ^ n[2] ^ n[3] + 1) & 255)
			}
			r[s.s] = f, i.push(new _(u, s.s - 1)), i.push(new _(s.s + 1, a))
		}
	}
	return H(n)
}

 */

template<class E, class T>
E hash_func_P(E e ,  T t)
{
	e += "";

	boost::uint64_t int_e = boost::lexical_cast<boost::uint64_t>(e);

	std::vector<typename E::value_type> n(4);

	n[0] = int_e >> 24 & 255;
	n[1] = int_e>> 16 & 255;
	n[2] = int_e >> 8 & 255;
	n[3] = int_e & 255;

	std::vector<typename T::value_type> r = hash_func_D(t);

	std::vector<jsStruct<int, int> > i;

	i.push_back(hash_func__(0, r.size() - 1 ));

	while ( i.size() > 0)
	{
		jsStruct<int, int> s = i.back();
		i.pop_back();

		if (s.s >= s.e || s.s < 0 || s.e >= r.size() )
			continue;

		if (s.s + 1 == s.e)
		{
			if (r[s.s] > r[s.e])
			{
				typename T::value_type o = r[s.s];
				r[s.s] = r[s.e];
				r[s.e] = o;
			}
		}
		else
		{
			std::size_t u = s.s;
			std::size_t a = s.e;
			std::size_t f = r[s.s];

			while (s.s < s.e)
			{
				while (s.s < s.e && r[s.e] >= f)
				{
					s.e--;
					n[0] = n[0] + 3 & 255;
				}

				if ( s.s < s.e) {
					r[s.s] = r[s.e];
					s.s++;
					n[1] = n[1] * 13 + 43 & 255;
				}

				while (s.s < s.e && r[s.s] <= f)
				{
					s.s++;
					n[2] = n[2] - 3 & 255;
				}


				if(s.s < s.e)
				{
					r[s.e] = r[s.s];
					s.e--;
					n[3] = (n[0] ^ n[1] ^ n[2] ^ n[3] + 1) & 255;
				}
			}

			r[s.s] = f;
			i.push_back(hash_func__(u, s.s - 1));
			i.push_back(hash_func__(s.s + 1, a));
		}
	}

	return hash_func_H(n);
}

} // namespace detail
} // namespace qqimpl
} // namespace webqq
