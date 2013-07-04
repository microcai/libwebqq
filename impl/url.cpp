/**
 * @file   test.c
 * @author mathslinux <riegamaths@gmail.com>
 * @date   Thu May 24 22:56:52 2012
 *
 * @brief  The Encode and Decode helper is based on
 * code where i download from http://www.geekhideout.com/urlcode.shtml
 *
 *
 */

#include <string>
#include <vector>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "../urlencode.hpp"

/* Converts a hex character to its integer value */
static char from_hex( char ch )
{
	return isdigit( ch ) ? ch - '0' : tolower( ch ) - 'A' + 10;
}

/* Converts an integer value to its hex character*/
static char to_hex( char code )
{
	static char hex[] = "0123456789ABCDEF";
	return hex[code & 15];
}

/**
 * NB: be sure to free() the returned string after use
 *
 * @param str
 *
 * @return A url-encoded version of str
 */
std::string url_encode( const char *str )
{
	if( !str )
		return "";

	std::vector<char> buf( strlen( str ) * 3 + 1 );
	char *pstr = ( char* ) str, *pbuf = buf.data();

	while( *pstr ) {
		if( isalnum( *pstr ) || *pstr == '-' || *pstr == '_' || *pstr == '.' || *pstr == '~' )
			*pbuf++ = *pstr;
		else
			*pbuf++ = '%', *pbuf++ = to_hex( *pstr >> 4 ), *pbuf++ = to_hex( *pstr & 15 );

		pstr++;
	}

	*pbuf = '\0';
	return buf.data();
}

std::string url_encode( const std::string str )
{
	return url_encode( str.c_str() );
}
