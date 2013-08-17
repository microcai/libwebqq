
/*
 * buddy manager - manager/update/cache the group/buddies information
 *
 * copyright 2013 microcai
 * copyright 2012 - 2013 avplayer.org
 */

#pragma once

#include <boost/asio.hpp>

#include <soci-sqlite3.h>
#include <boost-optional.h>
#include <boost-tuple.h>
#include <boost-gregorian-date.h>
#include <soci.h>

namespace webqq {
namespace qqimpl {
namespace detail {

} // nsmespace detail

class buddy_mgr
{
public:
	buddy_mgr(std::string dbname  = ":memory:")
	{
		sqlite_api::sqlite3_enable_shared_cache(1);
		m_sql.open(soci::sqlite3, dbname);
		db_initialize();
	}

	void update_group_list(std::string gid, std::string name, std::string code)
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

private:

	void db_initialize()
	{
		soci::transaction trans(m_sql);
		// 初始化存储数据库
		m_sql << ""
			"create table if not exists groups ( "
				"`gid` TEXT not null,"
				"`group_code` TEXT not null,"
				"`name` TEXT not null, "
				"`qqnum` TEXT, "
				"`owner` TEXT, "
				// last time that this group information retrived from TX
				// libwebqq will remove outdated one
				"`generate_time` TEXT not null"
			");";

		m_sql << ""
			"create table if not exists group_buddies ( "
				"`gid` TEXT not null,"
				"`uid` TEXT not null,"
				"`nick` TEXT,"
				"`card` TEXT,"
				"`mflag` INTEGER,"
				"`qqnum` TEXT"
			");";

		trans.commit();
	}

private:
	soci::session	m_sql;

};


} // nsmespace qqimpl
} // namespace webqq