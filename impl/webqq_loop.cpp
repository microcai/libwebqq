
#include <boost/log/trivial.hpp>

#include <boost/avloop.hpp>
#include <boost/timedcall.hpp>

#include "webqq_impl.hpp"
#include "webqq_check_login.hpp"
#include "webqq_login.hpp"
#include "webqq_poll_message.hpp"
#include "webqq_loop.hpp"
#include "webqq_group_member.hpp"

namespace webqq {
namespace qqimpl {
namespace detail {

/*
 * 这是 webqq 的一个内部循环，也是最重要的一个循环
 */
class webqq_main_loop_op : boost::asio::coroutine
{
public:
	webqq_main_loop_op(boost::asio::io_service & io_service, boost::shared_ptr<WebQQ> _webqq)
		: m_io_service(io_service)
		, m_webqq(_webqq)
		, m_counted_network_error(0)
	{
		// 读取 一些 cookie
		cookie::cookie webqqcookie =
			m_webqq->m_cookie_mgr.get_cookie("http://psession.qq.com/"); //.get_value("ptwebqq");

		// load 缓存的 一些信息
		m_webqq->m_vfwebqq = webqqcookie.get_value("vfwebqq");
		m_webqq->m_psessionid = webqqcookie.get_value("psessionid");
		m_webqq->m_clientid = webqqcookie.get_value("clientid");

		firs_start = m_webqq->m_clientid.empty();

		m_webqq->m_status = firs_start==0? LWQQ_STATUS_ONLINE:LWQQ_STATUS_OFFLINE;

		avloop_idle_post(
			m_io_service,
			boost::asio::detail::bind_handler(
				*this,
				boost::system::error_code(),
				std::string()
			)
		);
	}

	void operator()(boost::system::error_code ec, std::string str)
	{
		if (ec == boost::system::errc::operation_canceled)
			return;

		BOOST_ASIO_CORO_REENTER(this)
		{
		if (firs_start==0)
		{
			BOOST_LOG_TRIVIAL(info) << "libwebqq: use cached cookie to avoid login...";
		}

		for (;m_webqq->m_status!= LWQQ_STATUS_QUITTING;){

			m_counted_network_error = 0;

	  		// 首先进入 message 循环! 做到无登录享用!
			while (m_webqq->m_status == LWQQ_STATUS_ONLINE)
			{
				// TODO, 每 12个小时刷新群列表.

				// 获取一次消息。
				BOOST_ASIO_CORO_YIELD async_poll_message(m_webqq,
					boost::bind<void>(*this, _1, std::string())
				);

				if (firs_start==0 && !ec)
				{
					BOOST_LOG_TRIVIAL(info)
						<< "libwebqq: GOOD NEWS! The cached cookies accepted by TX!";
				}

				// 判断消息处理结果

				if (ec == error::poll_failed_need_login || ec == error::poll_failed_user_kicked_off)
				{
					// 重新登录
					m_webqq->m_status = LWQQ_STATUS_OFFLINE;

					// 延时 60s  第一次的话不延时,  立即登录.
					BOOST_ASIO_CORO_YIELD boost::delayedcallsec(m_io_service, 60 * firs_start,
						boost::asio::detail::bind_handler(*this, ec, str)
					);

					if (firs_start==0)
					{
						BOOST_LOG_TRIVIAL(info)
							<< "libwebqq: failed with last cookies, doing full login";
					}
				}
				else if ( ec == error::poll_failed_network_error )
				{
					// 网络错误计数 +1 遇到连续的多次错误就要重新登录.
					m_counted_network_error ++;

					if (m_counted_network_error >= 3)
					{
						m_webqq->m_status = LWQQ_STATUS_OFFLINE;

						BOOST_LOG_TRIVIAL(info)
							<< "libwebqq: too many network errors, try relogin later...";
					}
					// 等待等待就好了，等待 12s
					BOOST_ASIO_CORO_YIELD boost::delayedcallsec(m_io_service, 12,
						boost::asio::detail::bind_handler(*this, ec, str)
					);
				}
				else if (ec == error::poll_failed_need_refresh)
				{
					// 重新刷新列表.

					// 就目前来说, 重新登录是最快的实现办法
					// TODO 使用更好的办法.
					m_webqq->m_status = LWQQ_STATUS_OFFLINE;

					BOOST_LOG_TRIVIAL(info)
						<< "libwebqq: group uin changed, try relogin...";

					// 等待等待就好了，等待 15s
					BOOST_ASIO_CORO_YIELD boost::delayedcallsec(m_io_service, 12,
						boost::asio::detail::bind_handler(*this, ec, str)
					);
				}

				if (!ec)
				{
					// 没出错的话, 把数字减1, 恩.
					if (m_counted_network_error>0)
						m_counted_network_error --;
				}
				firs_start = 1;
			}

			do {
				// try check_login
				BOOST_ASIO_CORO_YIELD async_check_login(m_webqq, *this);

				if (ec)
				{
					if (ec == error::login_check_need_vc)
					{
						m_webqq->m_signeedvc(str);
						ec = boost::system::error_code();
					}
					else
					{
						if (ec)
						{
							BOOST_LOG_TRIVIAL(error) << "发生错误: " <<  ec.message() <<  " 重试中...";
							BOOST_ASIO_CORO_YIELD boost::delayedcallsec(
								m_io_service, 300, boost::asio::detail::bind_handler(*this, ec, str));
						}
					}
				}
				else
				{
					m_webqq->m_vc_queue.push(str);
				}

			}while (ec);

			// then retrive vc, can be pushed by check_login or login_withvc
			BOOST_ASIO_CORO_YIELD m_webqq->m_vc_queue.async_pop(*this);

			BOOST_LOG_TRIVIAL(info) << "vc code is \"" << str << "\"" ;
			// 回调会进入 async_pop 的下一行
			BOOST_ASIO_CORO_YIELD async_login(m_webqq, str, boost::bind<void>(*this, _1, str));

			// 完成登录了. 检查登录结果
			if (ec)
			{
				if (ec == error::login_failed_wrong_vc)
				{
					m_webqq->m_sigbadvc();
				}

				// 查找问题， 报告问题啊！
				if (ec == error::login_failed_wrong_passwd)
				{
					// 密码问题,  直接退出了.
					BOOST_LOG_TRIVIAL(error) << ec.message();
					BOOST_LOG_TRIVIAL(error) << "停止登录, 请修改密码重启 avbot";
					m_webqq->m_status = LWQQ_STATUS_QUITTING;
					return;
				}
				if (ec == error::login_failed_blocked_account)
				{
					BOOST_LOG_TRIVIAL(error) << "300s 后重试...";
					// 帐号冻结, 多等些时间, 嘻嘻
					BOOST_ASIO_CORO_YIELD boost::delayedcallsec(
						m_io_service, 300, boost::asio::detail::bind_handler(*this, ec, str));
				}

				BOOST_LOG_TRIVIAL(error) << "30s 后重试...";

				BOOST_ASIO_CORO_YIELD boost::delayedcallsec(
						m_io_service, 30, boost::asio::detail::bind_handler(*this,ec, str));
			}

			// 掉线了，自动重新进入循环， for (;;) 嘛
		}}
	}

private:
	boost::asio::io_service & m_io_service;
	boost::shared_ptr<WebQQ> m_webqq;
	int firs_start;

	int m_counted_network_error;
};


webqq_main_loop_op make_webqq_main_loop_op(boost::asio::io_service& io_service, boost::shared_ptr<qqimpl::WebQQ> _webqq)
{
	return webqq_main_loop_op(io_service, _webqq);
}

class group_refresh_loop_op : boost::asio::coroutine
{
public:
	group_refresh_loop_op(boost::shared_ptr<WebQQ> _webqq)
		:m_webqq(_webqq)
	{
		m_webqq->m_group_refresh_queue.async_pop(
			boost::bind<void>(*this, _1, _2)
		);

		m_last_sync = boost::posix_time::from_time_t(0);
	}

	// pop call back
	void operator()(boost::system::error_code ec, WebQQ::group_refresh_queue_type v)
	{
	 	if (ec == boost::system::errc::operation_canceled)
			return;
		std::string gid;
		// 检查最后一次同步时间.
		boost::posix_time::ptime curtime = boost::posix_time::from_time_t(std::time(NULL));

		BOOST_ASIO_CORO_REENTER(this)
		{for (;m_webqq->m_status!= LWQQ_STATUS_QUITTING;){

			// good, 现在可以更新了.

			// 先检查 type
			gid = v.get<1>();


			if (gid.empty()) // 更新全部.
			{
				// 需要最少休眠 30min 才能再次发起一次，否则会被 TX 拉黑
				if (boost::posix_time::time_duration(curtime - m_last_sync).minutes() <= 30)
				{
					BOOST_ASIO_CORO_YIELD boost::delayedcallsec(
						m_webqq->get_ioservice(),
						1800 - boost::posix_time::time_duration(curtime - m_last_sync).seconds(),
						boost::asio::detail::bind_handler(*this, ec, v)
					);
				}

				BOOST_ASIO_CORO_YIELD update_group_list(
					m_webqq,
					boost::bind<void>(*this, _1, v)
				);

				if (ec)
				{
					// 重来！，how？ 当然是 push 咯！
					m_webqq->m_group_refresh_queue.push(v);
				}
				else
				{
// 					// 检查是否刷新了群 GID, 如果是，就要刷新群列表！
// 					if ( ! m_webqq->m_groups.empty() &&
// 						! m_webqq->m_groups.begin()->second->memberlist.empty())
// 					{
// 						// 刷新群列表！
// 						// 接着是刷新群成员列表.
// 						for (iter = m_webqq->m_groups.begin(); iter != m_webqq->m_groups.end(); ++iter)
// 						{
// 							BOOST_ASIO_CORO_YIELD
// 								m_webqq->update_group_member(iter->second , boost::bind<void>(*this, _1, v));
//
// 							using namespace boost::asio::detail;
// 							BOOST_ASIO_CORO_YIELD
// 								boost::delayedcallms(m_webqq->get_ioservice(), 530, bind_handler(*this, ec, v));
// 						}
// 					}
				}
			}
			else
			{
				// 更新特定的 group 即可！
				BOOST_ASIO_CORO_YIELD qqimpl::update_group_member(
					m_webqq,
					m_webqq->m_buddy_mgr.get_group_by_gid(gid),
					boost::bind<void>(*this, _1, v)
				);

				if (ec){
					// 应该是群GID都变了，重新刷新
					m_webqq->m_group_refresh_queue.push(
						boost::make_tuple(
							webqq::webqq_handler_t(),
							std::string()
						)
					);
				}

			}

			// 更新完毕
			if (v.get<0>())
			{
				v.get<0>()(ec);
			}

			m_last_sync = boost::posix_time::from_time_t(std::time(NULL));

			// 继续获取，嘻嘻.
			BOOST_ASIO_CORO_YIELD m_webqq->m_group_refresh_queue.async_pop(
				boost::bind<void>(*this, _1, _2)
			);
		}}
	}

private:
	boost::posix_time::ptime m_last_sync;
	boost::shared_ptr<WebQQ> m_webqq;

	grouplist::iterator iter;
};

static group_refresh_loop_op make_group_refresh_loop_op(boost::shared_ptr<WebQQ> _webqq)
{
	return group_refresh_loop_op(_webqq);
}

} // namespace detail

void webqq_loop_start(boost::asio::io_service& io_service, boost::shared_ptr<qqimpl::WebQQ> _webqq)
{
	// 开启第一个 loop
	detail::make_webqq_main_loop_op(io_service, _webqq);
	// 开启第二个 loop
	detail::make_group_refresh_loop_op(_webqq);
}

} // namespace qqimpl
} // namespace webqq
