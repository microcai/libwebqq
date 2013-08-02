
#pragma once

#include <string>
#include <boost/log/trivial.hpp>

#include "webqq_impl.hpp"

namespace webqq {
namespace qqimpl {

typedef struct LwqqCookies {
	std::string ptvfsession;          /**< ptvfsession */
	std::string ptcz;
	std::string skey, p_skey;
	std::string ptwebqq;
	std::string ptuserinfo;
	std::string uin;
	std::string ptisp;
	std::string pt2gguin;
	std::string pt4_token;
	std::string verifysession;
	std::string lwcookies;
	std::string rv2;
	std::string uikey;

	std::string RK;
	std::string superkey, superuin;

	void clear() {
		ptvfsession.clear();          /**< ptvfsession */
		ptcz.clear();
		skey.clear();
		ptwebqq.clear();
		ptuserinfo.clear();
		uin.clear();
		ptisp.clear();
		pt2gguin.clear();
		pt4_token.clear();
		verifysession.clear();
		lwcookies.clear();
		rv2.clear();
		superkey.clear();
		superuin.clear();
	}
	void update(){
		this->lwcookies.clear();

		if( this->RK.length() ) {
			this->lwcookies += "RK=" + this->RK + "; ";
		}

		if( this->ptvfsession.length() ) {
			this->lwcookies += "ptvfsession=" + this->ptvfsession + "; ";
		}

		if( this->ptcz.length() ) {
			this->lwcookies += "ptcz=" + this->ptcz + "; ";
		}

		if( this->skey.length() ) {
			this->lwcookies += "skey=" + this->skey + "; ";
		}

		if( this->p_skey.length() ) {
			this->lwcookies += "p_skey=" + this->p_skey + "; ";
		}

		if( this->ptwebqq.length() ) {
			this->lwcookies += "ptwebqq=" + this->ptwebqq + "; ";
		}

		if( this->ptuserinfo.length() ) {
			this->lwcookies += "ptuserinfo=" + this->ptuserinfo + "; ";
		}

		if( this->uin.length() ) {
			this->lwcookies += "uin=" + this->uin + "; " + "p_uin=" + this->uin + "; ";
			this->lwcookies += "ptui_loginuin=" + this->uin.substr(1) + "; ";
			this->lwcookies += "p_uin=" + this->uin + "; ";
		}

		if( this->ptisp.length() ) {
			this->lwcookies += "ptisp=" + this->ptisp + "; ";
		}

		if( this->pt2gguin.length() ) {
			this->lwcookies += "pt2gguin=" + this->pt2gguin + "; ";
		}
		if( this->pt4_token.length() ) {
			this->lwcookies += "pt4_token=" + this->pt4_token + "; ";
		}else{
			this->lwcookies += "pt4_token=; p_skey=; ";
		}

		if( this->verifysession.length() ) {
			this->lwcookies += "verifysession=" + this->verifysession + "; ";
		}
		if( this->rv2.length() ) {
			this->lwcookies += "rv2=" + this->rv2 + "; ";
		}
		if( this->superkey.length() ) {
			this->lwcookies += "superkey=" + this->superkey + "; ";
		}
 		if( this->superuin.length() ) {
			this->lwcookies += "superuin=" + this->superuin + "; ";
		}
		if (this->uikey.length())
			this->lwcookies += "uikey=" + this->uikey + "; ";
	}
} LwqqCookies;


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
	}
	else if ( key == "uikey")
	{
		cookies->uikey = value;
	}
	else
	{
		BOOST_LOG_TRIVIAL(warning) <<  " No this cookie: " <<  key;
	}
}

// static void save_cookie( LwqqCookies * cookies, const std::string & httpheader )
// {
// 	update_cookies( cookies, httpheader, "ptcz" );
// 	update_cookies( cookies, httpheader, "skey" );
// 	update_cookies( cookies, httpheader, "p_skey" );
// 	update_cookies( cookies, httpheader, "ptwebqq" );
// 	update_cookies( cookies, httpheader, "ptuserinfo" );
// 	update_cookies( cookies, httpheader, "uin" );
// 	update_cookies( cookies, httpheader, "ptisp" );
// 	update_cookies( cookies, httpheader, "pt2gguin" );
// 	update_cookies( cookies, httpheader, "pt4_token" );
// 	update_cookies( cookies, httpheader, "rv2" );
// 	update_cookies( cookies, httpheader, "RK" );
// 	update_cookies( cookies, httpheader, "superkey" );
// 	update_cookies( cookies, httpheader, "superuin" );
// 	update_cookies( cookies, httpheader, "uikey" );
//
// 	//ptui_loginuin=2019517659; pt2gguin=o2019517659;
// 	cookies->update();
// }

}
}
}
