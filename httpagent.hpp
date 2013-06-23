
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

#ifndef __HTTP_AGENT_HPP__
#define __HTTP_AGENT_HPP__

#include <boost/function.hpp>
#include <boost/asio.hpp>
#include <avhttp.hpp>

#include <boost/asio/yield.hpp>

#ifndef SYMBOL_HIDDEN

#if defined _WIN32 || defined __CYGWIN__
#define SYMBOL_HIDDEN
#else
#if __GNUC__ >= 4
#define SYMBOL_HIDDEN  __attribute__ ((visibility ("hidden")))
#else
#define SYMBOL_HIDDEN
#endif
#endif

#endif // SYMBOL_HIDDEN

typedef boost::shared_ptr<avhttp::http_stream> read_streamptr;

namespace detail{

template<class httpstreamhandler>
class SYMBOL_HIDDEN async_http_download_op : boost::asio::coroutine {
public:
	typedef void result_type;
public:
	async_http_download_op( read_streamptr _stream, const avhttp::url & url, httpstreamhandler _handler )
		: handler( _handler ), stream( _stream ), sb( new boost::asio::streambuf() ) , readed( 0 )
	{
		stream->async_open( url, *this );
	}

	void operator()( const boost::system::error_code& ec , std::size_t length = 0 ) {
		reenter( this ) {
			if( !ec ) {
				content_length = stream->response_options().find( avhttp::http_options::content_length );

				while( !ec ) {
					yield stream->async_read_some( sb->prepare( 4096 ), *this );
					sb->commit( length );
					readed += length;

					if( !content_length.empty() &&  readed == boost::lexical_cast<std::size_t>( content_length ) ) {
						handler( boost::asio::error::make_error_code( boost::asio::error::eof ), stream, *sb );
						return ;
					}
				}
			}

			handler( ec, stream, *sb );
		}
	}

private:
	std::size_t	readed;
	std::string content_length;
	read_streamptr stream;
	httpstreamhandler handler;
	boost::shared_ptr<boost::asio::streambuf> sb;
};

}

template<class httpstreamhandler>
void async_http_download(read_streamptr _stream, const avhttp::url & url, httpstreamhandler _handler)
{
	detail::async_http_download_op<boost::function<void ( const boost::system::error_code& ec, read_streamptr stream,  boost::asio::streambuf & ) > >(_stream, url, _handler);
}

#endif // __HTTP_AGENT_HPP__
