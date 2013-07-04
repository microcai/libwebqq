/**
 * @file   url.h
 * @author mathslinux <riegamaths@gmail.com>
 * @date   Thu May 24 23:01:23 2012
 *
 * @brief  The Encode and Decode helper is based on
 * code where i download from http://www.geekhideout.com/urlcode.shtml
 *
 *
 */

#ifndef ___LWQQ_URL_H___
#define ___LWQQ_URL_H___

/**
 * NB: be sure to free() the returned string after use
 *
 * @param str
 *
 * @return A url-encoded version of str
 */
std::string url_encode( const char *str );
std::string url_encode( const std::string str );

#endif  /* ___LWQQ_URL_H___ */
