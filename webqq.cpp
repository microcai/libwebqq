/*
 * Copyright (C) 2012  微蔡 <microcai@fedoraproject.org>
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

#include "webqq.h"
#include "webqq_impl.h"

webqq::webqq( boost::asio::io_service& asioservice, std::string qqnum, std::string passwd, LWQQ_STATUS status )
	: impl( new qq::WebQQ( asioservice, qqnum, passwd, status ) )
{
}

void webqq::on_verify_code( boost::function< void ( const boost::asio::const_buffer & ) >  cb )
{
	impl->signeedvc.connect( cb );
}


void webqq::on_group_msg( boost::function< void( const std::string, const std::string, const std::vector<qqMsg>& )> cb )
{
	this->impl->siggroupmessage.connect( cb );
}

void webqq::update_group_member( qqGroup& group )
{
	impl->update_group_member( group );
}

qqGroup * webqq::get_Group_by_gid( std::string gid )
{
	return impl->get_Group_by_gid( gid );
}

qqGroup* webqq::get_Group_by_qq( std::string qq )
{
	return impl->get_Group_by_qq( qq );
}

void webqq::login()
{
	impl->login();
}

void webqq::login_withvc( std::string vccode )
{
	impl->login_withvc( vccode );
}


void webqq::send_group_message( std::string group, std::string msg, boost::function<void ( const boost::system::error_code& ec )> donecb )
{
	impl->send_group_message( group, msg, donecb );
}

void webqq::send_group_message( qqGroup& group, std::string msg, boost::function<void ( const boost::system::error_code& ec )> donecb )
{
	impl->send_group_message( group, msg, donecb );
}

boost::asio::io_service& webqq::get_ioservice()
{
	return impl->get_ioservice();
}

bool webqq::is_online()
{
	return impl->m_status == LWQQ_STATUS_ONLINE;
}

