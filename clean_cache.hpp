#pragma once

#include <boost/asio.hpp>
#include <boost/avloop.hpp>
#include <boost/filesystem.hpp>
#include <boost/async_dir_walk.hpp>

namespace detail{

class clean_cache{
	boost::asio::io_service &io_service;
public:
	clean_cache(boost::asio::io_service &_io_service):io_service(_io_service)
	{
		// 空闲时执行.
		avloop_idle_post(io_service, *this);
	}

	void operator()()
	{

	}
};

void clean_cache_dir_walk_handler( boost::asio::io_service & io_service, const boost::filesystem::path & item, boost::function<void( const boost::system::error_code& )> handler )
{
	using namespace boost::system;

	//boost::function<void(const boost::system::error_code&)> h = io_service.wrap(handler);
	if( boost::filesystem::is_directory( item ) )
	{
		handler( error_code() );
	}
	else
	{
		// 执行 glob 匹配,  寻找 cache_*
		// 删除最后访问时间超过一天的文件.


	}


	// 根据 path 是不是

}

}

void clean_cache(boost::asio::io_service &io_service)
{
	boost::async_dir_walk(io_service, boost::filesystem::path("."),
		boost::bind(detail::clean_cache_dir_walk_handler, boost::ref(io_service), _1, _2)
	);
}
