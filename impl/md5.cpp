
#include <string.h>
#include <stdio.h>
#include <boost/hash.hpp>
#include "md5.hpp"

std::string lutil_md5_data(const std::string & data)
{
	boost::hashes::md5::digest_type md5sum ;
	md5sum = boost::hashes::compute_digest<boost::hashes::md5>(data);
	return md5sum.str();
}

std::string lutil_md5_digest(const std::string & data)
{
	boost::hashes::md5::digest_type md5sum ;
	md5sum = boost::hashes::compute_digest<boost::hashes::md5>(data);
	return std::string(reinterpret_cast<const char*>(md5sum.c_array()), md5sum.static_size);
}
