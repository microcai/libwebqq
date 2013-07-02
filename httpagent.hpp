
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

namespace avhttp
{
namespace misc
{

namespace detail
{

// match condition!
struct avhttp_async_read_body_condition
{
	avhttp_async_read_body_condition(boost::int64_t content_length)
		:m_content_length(false)
	{
	}

	std::size_t operator()(boost::system::error_code ec, std::size_t bytes_transferred)
	{
		if(ec)
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

	boost::int64_t m_content_length;
};

template<class avHttpStream, class MutableBufferSequence, class Handler>
class async_read_body_op : boost::asio::coroutine
{
public:
	async_read_body_op(avHttpStream &stream, const avhttp::url & url,
						MutableBufferSequence &buffers, Handler handler)
		: m_handler(handler), m_stream(stream), m_buffers(buffers)
	{
		m_stream.async_open(url, *this);
	}

	void operator()(const boost::system::error_code& ec , std::size_t length = 0)
	{
		BOOST_ASIO_CORO_REENTER(this)
		{
			if(!ec)
			{
				BOOST_ASIO_CORO_YIELD boost::asio::async_read(m_stream,
															  m_buffers,
															  avhttp_async_read_body_condition(m_stream.content_length()),
															  *this);
			}else{
				m_handler(ec, length);
				return;
			}

			if(ec == boost::asio::error::eof && m_stream.content_length()==0)
			{
				m_handler(boost::system::error_code(), length);
			}
			else
			{
				m_handler(ec, length);
			}
		}
	}

private:
	avHttpStream & m_stream;
	Handler m_handler;
	MutableBufferSequence &m_buffers;
};

template<class avHttpStream, class MutableBufferSequence, class Handler>
async_read_body_op<avHttpStream, MutableBufferSequence, Handler>
	make_async_read_body_op(avHttpStream & stream, const avhttp::url & url,
							MutableBufferSequence &buffers, Handler _handler)
{
	return async_read_body_op<avHttpStream, MutableBufferSequence, Handler>
				(stream, url, buffers, _handler);
}

} // namespace detail


/**
 * 用法, 先设置好 http_stream 的 options ,  然后调用
 *   avhttp::misc::async_read_body(stream, url, buffer, handler);
 * 注意,  stream 和 buffer 的生命周期要保持到调用handler.
 *
 * handler 的签名同 boost::asio::async_read
 *
 */
template<class avHttpStream, class MutableBufferSequence, class Handler>
void async_read_body(avHttpStream & stream, const avhttp::url & url, MutableBufferSequence &buffers, Handler _handler)
{
	detail::make_async_read_body_op(stream, url, buffers, _handler);
}

} // namespace misc
} // namespace avhttp

#endif // __AVHTTP_MISC_HTTP_READBODY_HPP__
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
