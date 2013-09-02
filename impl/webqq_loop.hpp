
#include <boost/asio.hpp>
#include "../webqq.hpp"

namespace webqq{
namespace qqimpl{

void webqq_loop_start(boost::asio::io_service& io_service, boost::shared_ptr<qqimpl::WebQQ> _webqq);

} // namespace qqimpl
} // namespace webqq
