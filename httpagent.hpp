
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
#include <boost/function.hpp>
#include <boost/asio.hpp>
#include <avhttp.hpp>

#include "boost/coro/coro.hpp"
#include "boost/coro/yield.hpp"


typedef boost::shared_ptr<avhttp::http_stream> read_streamptr;


class SYMBOL_HIDDEN async_http_download : boost::coro::coroutine {
public:
	typedef void result_type;
public:
	template<class httpstreamhandler>
	async_http_download( read_streamptr _stream, const avhttp::url & url, httpstreamhandler _handler )
		: handler( _handler ), stream( _stream ), sb( new boost::asio::streambuf() ) , readed( 0 ) {
		stream->async_open( url, *this );
	}

	void operator()( const boost::system::error_code& ec , std::size_t length = 0 ) {
		reenter( this ) {
			if( !ec ) {
				content_length = stream->response_options().find( avhttp::http_options::content_length );

				while( !ec ) {
					_yield stream->async_read_some( sb->prepare( 4096 ), *this );
					sb->commit( length );
					readed += length;

					if( !content_length.empty() &&  readed == boost::lexical_cast<std::size_t>( content_length ) ) {
						handler( boost::system::error_code( boost::asio::error::eof ), stream, *sb );
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
	boost::function<void ( const boost::system::error_code& ec, read_streamptr stream,  boost::asio::streambuf & ) > handler;
	boost::shared_ptr<boost::asio::streambuf> sb;
};
