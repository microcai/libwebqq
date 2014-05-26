
#pragma once

#include <boost/function.hpp>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <boost/property_tree/json_parser.hpp>
namespace js = boost::property_tree::json_parser;

#include <avhttp.hpp>
#include <avhttp/async_read_body.hpp>
#include <avhttp/default_storage.hpp>

#include "webqq_impl.hpp"

#include "constant.hpp"
#include "utf8.hpp"

namespace webqq{
namespace qqimpl{
namespace detail{

inline long lwqq_time()
{
    struct timeval tv;
   // gettimeofday(&tv, NULL);
    long ret;
    ret = tv.tv_sec*1000+tv.tv_usec/1000;
    return ret;
}

/*
 * 本协程上传文件,
 */
template<class Handler>
class upload_offline_file_op : boost::asio::coroutine
{
public:
	upload_offline_file_op(const boost::shared_ptr<WebQQ> & _webqq,
		const std::string & filename,
		const std::string & from_uin,
		const std::string & to_uin,
		Handler handler)
		: m_webqq(_webqq)
		, m_handler(handler)
		, m_filename(filename)
		, m_file(boost::make_shared<avhttp::default_storge>())
	{
		boost::system::error_code ec;

		m_file->open(m_filename, ec);
		if (ec)
		{
			std::cerr << "Error: " << ec.message() << std::endl;
			failed_invode_handler(ec);
		}

		std::string url = boost::str(
			boost::format("http://weboffline.ftn.qq.com/ftn_access/upload_offline_file?time=%ld")
			% lwqq_time()
		);

		m_uploader = boost::make_shared<avhttp::file_upload>(boost::ref(m_webqq->get_ioservice()), true);

		m_uploader->request_option(
			avhttp::request_opts()
				(avhttp::http_options::referer, "http://web2.qq.com/")
				("Origin", "http://web2.qq.com/")
				("Cache-Control", "max-age=0")
		);

		avhttp::cookies uploadcookie = m_webqq->m_cookie_mgr.get_cookie(url);

		m_uploader->get_http_stream().http_cookies(uploadcookie);

		avhttp::file_upload::form_args args;

		args["callback"] = "parent.EQQ.Model.ChatMsg.callbackSendOffFile";
		args["locallangid"] = "2052";
		args["clientversion"] = "1409";
		args["uin"] = from_uin;
		args["skey"] = uploadcookie["skey"];

		args["appid"] = "1002101";
		args["peeruin"] = to_uin;
		args["vfwebqq"] = m_webqq->m_vfwebqq;

		args["fileid"] = boost::str(boost::format("%s_%ld") % to_uin % std::time(NULL));

		args["senderviplevel"] = "0";
		args["reciverviplevel"] = "0";

		m_uploader->async_open(url, filename, "file", args, *this);
	}

	void operator()(boost::system::error_code ec, std::size_t bytes_transfered = 0)
	{
		BOOST_ASIO_CORO_REENTER(this)
		{
			if (ec)
			{
				return failed_invode_handler(ec);
			}

			m_buffer = boost::make_shared< boost::array<char, 1024> >();

			// 开始写入文件.
			while ((readed = m_file->read(m_buffer->c_array(), 1024)) > 0)
			{
				BOOST_ASIO_CORO_YIELD boost::asio::async_write(
					*m_uploader,
					boost::asio::buffer(*m_buffer, readed),
					boost::asio::transfer_exactly(readed),
					*this
				);
				if (ec)
				{
					failed_invode_handler(ec);
					return;
				}
			}

			m_buffer.reset();

			BOOST_ASIO_CORO_YIELD m_uploader->async_write_tail(*this);

			// 读取 body
			m_result = boost::make_shared<boost::asio::streambuf>();

			BOOST_ASIO_CORO_YIELD boost::asio::async_read(
				m_uploader->get_http_stream(),
				*m_result,
				avhttp::transfer_response_body(m_uploader->get_http_stream().content_length()),
				*this
			);

			// 获取 filename 和 filepath,  将在下一步发送过程中使用!
			try{
 				pt::ptree result;

 				std::istream tx_reseult(m_result.get());

				js::read_json(tx_reseult, result);

				if( result.get<int>("retcode") != 0)
				{
					// upload failed
					return failed_invode_handler(error::make_error_code(error::upload_offline_file_failed));
				}

				std::string filename, filepath;

				filename = result.get<std::string>("filename");
				filepath = result.get<std::string>("filepath");

				m_handler(ec, filename, filepath);
 			}catch (...)
			{
				failed_invode_handler(error::make_error_code(error::upload_offline_file_failed));
			}
		}

	}

private:
	void failed_invode_handler(const boost::system::error_code & ec)
	{
		m_webqq->get_ioservice().post(
			boost::bind<void>(m_handler, ec, std::string(), std::string())
		);
	}

private:
	boost::shared_ptr<WebQQ> m_webqq;
	boost::shared_ptr<avhttp::file_upload> m_uploader;
	boost::shared_ptr<avhttp::default_storge> m_file;
	std::string m_filename;
	Handler m_handler;


	boost::shared_ptr<
		boost::array<char, 1024>
	> m_buffer;

	boost::shared_ptr<boost::asio::streambuf> m_result;

	std::streamsize readed;
};

template<class Handler>
upload_offline_file_op<Handler>
make_upload_offline_file_op(const boost::shared_ptr<WebQQ> & _webqq, const std::string & filename,
	const std::string & from_uin, const std::string & to_uin, Handler handler)
{
	return upload_offline_file_op<Handler>(_webqq, filename, from_uin, to_uin, handler);
}

template<class Handler>
void async_upload_offline_file(const boost::shared_ptr<WebQQ> & _webqq, const std::string & filename,
	const std::string & from_uin, const std::string & to_uin, Handler handler)
{
	make_upload_offline_file_op(_webqq, filename, from_uin, to_uin, handler);
}

/*
 * 上传的文件再放入列队.
 */
template<class Handler>
class send_offline_file_op : boost::asio::coroutine
{
public:
	send_offline_file_op(const boost::shared_ptr<WebQQ>& _webqq,
		const std::string& _to_uin, const std::string& filename,
		Handler handler)
		: m_webqq(_webqq)
		, m_handler(handler)
		, to_uin(_to_uin)
	{
		async_upload_offline_file(m_webqq, filename, m_webqq->m_myself_uin, to_uin, *this);
	}

	void operator()(boost::system::error_code ec, std::string filename, std::string filepath)
	{
		if (ec)
		{
			return m_handler(ec);
		}

		std::string post_data = boost::str(
			boost::format("r={\"to\":\"%s\",\"file_path\":\"%s\","
				"\"filename\":\"%s\",\"to_uin\":\"%s\","
				"\"clientid\":\"%s\",\"psessionid\":\"%s\"}"
			)
			% to_uin %  filepath % filename % to_uin
			% m_webqq->m_clientid % m_webqq->m_psessionid
		);

		m_stream = boost::make_shared<avhttp::http_stream>(boost::ref(m_webqq->get_ioservice()));
		m_buffer = boost::make_shared<boost::asio::streambuf>();


		m_stream->request_options(
			avhttp::request_opts()
				(avhttp::http_options::referer, WEBQQ_D_REF_URL)
		);

		std::string url = WEBQQ_D_HOST "/channel/send_offfile2";

		m_webqq->m_cookie_mgr.get_cookie(url, *m_stream);

		avhttp::async_read_body(*m_stream, url, *m_buffer, *this);
	}

	void operator()(boost::system::error_code ec, std::size_t bytes_transfered)
	{
		m_handler(ec);
	}

private:
	boost::shared_ptr<WebQQ> m_webqq;
	boost::shared_ptr<avhttp::http_stream> m_stream;
	boost::shared_ptr<boost::asio::streambuf> m_buffer;
	Handler m_handler;

	std::string to_uin;
};

template<class Handler>
send_offline_file_op<Handler>
make_send_offline_file_op(const boost::shared_ptr<WebQQ> & _webqq,
		const std::string &to_uin, const std::string &filename, Handler handler)
{
	return send_offline_file_op<Handler>(_webqq, to_uin, filename, handler);
}

} // namespace detail

template<class Handler>
void async_send_offline_file(const boost::shared_ptr<WebQQ> & _webqq,
		const std::string &to_uin, const std::string &filename, Handler handler)
{
	detail::make_send_offline_file_op(_webqq, to_uin, filename, handler);
}

} // namespace qqimpl
} // namespace webqq
