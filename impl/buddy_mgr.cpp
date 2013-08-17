
#include "buddy_mgr.hpp"
#include <boost/smart_ptr/make_shared_object.hpp>

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

bool buddy_mgr::group_has_qqnum(std::string code)
{
	using namespace soci;

	std::string qqnum;
	soci::indicator qqnum_indicator;

	m_sql << "select qqnum from groups where group_code = :group_code"
		, into(qqnum, qqnum_indicator), use(code);

	if (qqnum_indicator == soci::i_ok)
	{
		return  !qqnum.empty();
	}

	return false;
}

void buddy_mgr::map_group_qqnum(std::string code, std::string qqnum)
{
	using namespace soci;

	transaction trans(m_sql);

	m_sql << "update groups set qqnum = :qqnum where group_code = :group_code "
		, use(qqnum), use(code);

	trans.commit();
}



qqGroup_ptr buddy_mgr::get_group_by_gid(std::string gid)
{
	qqGroup group;

	m_sql << "select gid, group_code, name, qqnum, owner from groups where gid = :gid"
	  , soci::into(group.gid)
	  , soci::into(group.code)
	  , soci::into(group.name)
	  , soci::into(group.qqnum)
	  , soci::into(group.owner)
	  , soci::use(gid);

	return boost::make_shared<qqGroup>(group);
}

qqGroup_ptr buddy_mgr::get_group_by_qq(std::string qqnum)
{
	qqGroup group;

	m_sql << "select gid, group_code, name, qqnum, owner from groups where qqnum = :qqnum"
	  , soci::into(group.gid)
	  , soci::into(group.code)
	  , soci::into(group.name)
	  , soci::into(group.qqnum)
	  , soci::into(group.owner)
	  , soci::use(qqnum);

	return boost::make_shared<qqGroup>(group);
}

void buddy_mgr::set_group_owner(std::string gid, std::string owner)
{
	using namespace soci;

	transaction trans(m_sql);

	m_sql << "update groups set owner = :owner where gid = :gid "
		, use(owner), use(gid);

	trans.commit();
}

} // nsmespace qqimpl
} // namespace webqq
