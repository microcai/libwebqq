
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

#ifndef __AVHTTP_MISC_HTTP_READBODY_HPP__
#define __AVHTTP_MISC_HTTP_READBODY_HPP__

#include <boost/asio.hpp>
#include <avhttp.hpp>

namespace avhttp {
namespace misc {
namespace detail {

// match condition!
struct read_all_t
{
  read_all_t(std::string content_length_str)
		: has_content_length(false)
	{

		if(!content_length_str.empty())
		{
			// 转化为 content_length
			has_content_length = true;
			// cast 失败肯定是严重的 bug
			m_content_length = boost::lexical_cast<std::size_t>(content_length_str);
		}
	}

	template <typename Error>
	std::size_t operator()(const Error& err, std::size_t bytes_transferred)
	{
		if(err)
			return 0;

		if(m_content_length > 0 )
		{
			// 读取到 content_length 是吧.
			return m_content_length - bytes_transferred;
		}
		else
		{
			return 4096;
		}
	}

	bool has_content_length;
	boost::int64_t m_content_length;
};

inline read_all_t read_all(std::string content_length)
{
	return read_all_t(content_length);
}

template <typename AsyncReadStream, typename MutableBufferSequence, typename Handler>
class read_body_op : boost::asio::coroutine
{
public:
	read_body_op(AsyncReadStream &stream, const avhttp::url &url,
		MutableBufferSequence &buffers, BOOST_ASIO_MOVE_ARG(Handler) handler)
		: m_stream(stream)
		, m_buffers(buffers)
		, m_handler(BOOST_ASIO_MOVE_CAST(Handler)(handler))
	{
		m_stream.async_open(url, *this);
	}

#if defined(BOOST_ASIO_HAS_MOVE)
    read_body_op(const read_body_op &other)
      : m_stream(other.m_stream)
	  , m_buffers(other.m_buffers)
	  , m_handler(other.m_handler)
    {
    }

    read_body_op(read_body_op &&other)
      : m_stream(other.m_stream)
	  , m_buffers(other.m_buffers)
	  , m_handler(BOOST_ASIO_MOVE_CAST(Handler)(other.m_handler))
    {
    }
#endif // defined(BOOST_ASIO_HAS_MOVE)

	void operator()(const boost::system::error_code &ec, std::size_t bytes_transferred = 0)
	{
		BOOST_ASIO_CORO_REENTER(this)
		{
			if(!ec)
			{
				BOOST_ASIO_CORO_YIELD boost::asio::async_read( m_stream, m_buffers,
						read_all(m_stream.response_options().find(avhttp::http_options::content_length)), *this);
			}
			else
			{
				m_handler(ec, bytes_transferred);
				return;
			}

			if(ec == boost::asio::error::eof && m_stream.content_length() == 0)
			{
				m_handler(boost::system::error_code(), bytes_transferred);
			}
			else
			{
				m_handler(ec, bytes_transferred);
			}
		}
	}

private:
	AsyncReadStream &m_stream;
	MutableBufferSequence &m_buffers;
	Handler m_handler;
};

template <typename AsyncReadStream, typename MutableBufferSequence, typename Handler>
read_body_op<AsyncReadStream, MutableBufferSequence, Handler>
	make_read_body_op(AsyncReadStream &stream,
	const avhttp::url &url, MutableBufferSequence &buffers, Handler handler)
{
	return read_body_op<AsyncReadStream, MutableBufferSequence, Handler>(
		stream, url, buffers, handler);
}

} // namespace detail


///用于http_stream异步访问url.
// 这个函数用于http_stream异步访问指定的url, 并通过handler回调通知用户, 数据将
// 保存在用户事先提供的buffers中.
// @注意:
//  1. 该函数停止回调条件为直到读取到eof或遇到错误, 错误信息通过error_code传回.
//  2. 在完成整个过程中, 应该保持 stream 和 buffers的生命期.
// @param stream 一个http_stream对象.
// @param url 指定的url.
// @param buffers 一个或多个用于读取数据的缓冲区
// 这个类型必须满足MutableBufferSequence, MutableBufferSequence的定义.
// 具体可参考boost.asio文档中相应的描述.
// @param handler在读取操作完成或出现错误时, 将被回调, 它满足以下条件:
// @begin code
//  void handler(
//    const boost::system::error_code &ec,	// 用于返回操作状态.
//    std::size_t bytes_transferred			// 返回读取的数据字节数.
//  );
// @end code
// @begin example
//  void handler(const boost::system::error_code &ec, std::size_t bytes_transferred)
//  {
//      // 处理异步回调.
//  }
//  ...
//  avhttp::http_stream h(io);
//  async_read_body(h, "http://www.boost.org/LICENSE_1_0.txt",
//      boost::asio::buffer(data, 1024), handler);
//  io.run();
//  ...
// @end example
template<typename AsyncReadStream, typename MutableBufferSequence, typename Handler>
void async_read_body(AsyncReadStream &stream,
	const avhttp::url &url, MutableBufferSequence &buffers, Handler handler)
{
	detail::make_read_body_op(stream, url, buffers, handler);
}

} // namespace misc
} // namespace avhttp

#endif // __AVHTTP_MISC_HTTP_READBODY_HPP__
