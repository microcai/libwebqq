
#include "buddy_mgr.hpp"

namespace webqq {
namespace qqimpl {

void buddy_mgr::update_group_list(std::string gid, std::string name, std::string code)
{
	using namespace soci;

	std::string qqnum, owner;

	soci::indicator qqnum_indicator, owner_indicator;

	transaction trans(m_sql);

	m_sql << "select qqnum, owner from groups where group_code= :group_code"
		  , into(qqnum, qqnum_indicator), into(owner, owner_indicator), use(code);

	if (qqnum_indicator == i_null)
		qqnum = "";

	if (owner_indicator == i_null)
		owner = "";

	m_sql << "delete from groups where gid = :gid or group_code = :group_code "
		  , use(gid), use(code);

	m_sql << "insert into groups "
		  "(gid, group_code, name, qqnum, owner, generate_time)"
		  " values "
		  "(:gid, :group_code, :name, :qqnum, :owner, datetime('now') )"
		  , use(gid), use(code), use(name), use(qqnum), use(owner);

	trans.commit();
}

} // nsmespace qqimpl
} // namespace webqq
