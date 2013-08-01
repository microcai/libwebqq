
#pragma once

#include <string>

#include "webqq_impl.hpp"

namespace webqq {
namespace qqimpl {
namespace detail {

static std::string get_cookie( const std::string & cookie, std::string key )
{
	std::string searchkey = key + "=";
	std::string::size_type keyindex = cookie.find( searchkey );

	if( keyindex == std::string::npos )
		return "";

	keyindex += searchkey.length();
	std::string::size_type valend = cookie.find( "; ", keyindex );
	return cookie.substr( keyindex , valend - keyindex );
}

static void update_cookies(LwqqCookies *cookies, const std::string & httpheader,
							std::string key)
{
	std::string value = get_cookie(httpheader, key);

	if( value.empty() )
		return ;

	if( key ==  "RK" )
	{
		cookies->RK =  value ;
	}else if( key ==  "ptvfsession" )
	{
		cookies->ptvfsession = value ;
	}
	else if( ( key == "ptcz" ) )
	{
		cookies->ptcz = value ;
	}
	else if( ( key == "skey" ) )
	{
		cookies->skey = value ;
	}else if ( key == "p_skey")
	{
		cookies->p_skey = value ;
	}
	else if( ( key == "ptwebqq" ) )
	{
		cookies->ptwebqq = value ;
	}
	else if( ( key == "ptuserinfo" ) )
	{
		cookies->ptuserinfo = value ;
	}
	else if( ( key == "uin" ) )
	{
		cookies->uin = value ;
	}
	else if( ( key == "ptisp" ) )
	{
		cookies->ptisp = value ;
	}
	else if( ( key == "pt2gguin" ) )
	{
		cookies->pt2gguin = value ;
	}
	else if( ( key == "pt4_token" ) )
	{
		cookies->pt4_token = value;
	}
	else if( ( key == "pt4_token" ) )
	{
		cookies->pt4_token =  value;
	}
	else if( ( key == "verifysession" ) )
	{
		cookies->verifysession = value ;
	}
	else if( ( key == "superkey" ) )
	{
		cookies->superkey = value ;
	}
	else if( ( key == "superuin" ) )
	{
		cookies->superuin = value ;
	}
	else if( ( key == "rv2" ) )
	{
		cookies->rv2 = value ;
	}	else
	{
		BOOST_LOG_TRIVIAL(warning) <<  " No this cookie: " <<  key;
	}
}

static void save_cookie( LwqqCookies * cookies, const std::string & httpheader )
{
	update_cookies( cookies, httpheader, "ptcz" );
	update_cookies( cookies, httpheader, "skey" );
	update_cookies( cookies, httpheader, "p_skey" );
	update_cookies( cookies, httpheader, "ptwebqq" );
	update_cookies( cookies, httpheader, "ptuserinfo" );
	update_cookies( cookies, httpheader, "uin" );
	update_cookies( cookies, httpheader, "ptisp" );
	update_cookies( cookies, httpheader, "pt2gguin" );
	update_cookies( cookies, httpheader, "pt4_token" );
	update_cookies( cookies, httpheader, "ptui_loginuin" );
	update_cookies( cookies, httpheader, "rv2" );
	update_cookies( cookies, httpheader, "RK" );
	update_cookies( cookies, httpheader, "superkey" );
	update_cookies( cookies, httpheader, "superuin" );
	cookies->update();
}

}
}
}
