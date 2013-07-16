/*
 * Copyright (C) 2013  微蔡 <microcai@fedoraproject.org>
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

#include <boost/assign.hpp>

#include "webqq_impl.hpp"

void webqq::qqimpl::WebQQ::init_face_map()
{
	boost::assign::insert( facemap )
	( 0, 14 )
	( 14, 0 )
	( 50, 15 )
	( 51, 16 )
	( 96, 17 )
	( 53, 18 )
	( 54, 19 )
	( 73, 20 )
	( 74, 21 )
	( 75, 22 )
	( 76, 23 )
	( 77, 24 )
	( 78, 25 )
	( 56, 26 )
	( 55, 27 )
	( 57, 28 )
	( 58, 29 )
	( 79, 30 )
	( 80, 31 )
	( 81, 32 )
	( 83, 33 )
	( 82, 34 )
	( 84, 35 )
	( 85, 36 )
	( 86, 37 )
	( 87, 38 )
	( 88, 39 )
	( 97, 40 )
	( 98, 41 )
	( 99, 42 )
	( 100, 43 )
	( 101, 44 )
	( 102, 45 )
	( 103, 46 )
	( 104, 47 )
	( 105, 48 )
	( 106, 49 )
	( 108, 50 )
	( 107, 51 )
	( 109, 52 )
	( 110, 53 )
	( 111, 54 )
	( 112, 55 )
	( 32, 56 )
	( 113, 57 )
	( 114, 58 )
	( 115, 59 )
	( 63, 60 )
	( 64, 61 )
	( 59, 62 )
	( 33, 63 )
	( 34, 64 )
	( 116, 65 )
	( 36, 66 )
	( 37, 67 )
	( 38, 68 )
	( 91, 69 )
	( 92, 70 )
	( 93, 71 )
	( 29, 72 )
	( 117, 73 )
	( 72, 74 )
	( 45, 75 )
	( 42, 76 )
	( 39, 77 )
	( 62, 78 )
	( 46, 79 )
	( 47, 80 )
	( 71, 81 )
	( 95, 82 )
	( 118, 83 )
	( 119, 84 )
	( 120, 85 )
	( 121, 86 )
	( 122, 87 )
	( 123, 88 )
	( 124, 89 )
	( 27, 90 )
	( 21, 91 )
	( 23, 92 )
	( 25, 93 )
	( 26, 94 )
	( 125, 95 )
	( 126, 96 )
	( 127, 97 )
	( 128, 98 )
	( 129, 99 )
	( 130, 100 )
	( 131, 101 )
	( 132, 102 )
	( 133, 103 )
	( 134, 104 )
	;

	for( int i = 0; i <= 134; i++ )
		facemap.insert( std::make_pair( i, i ) );
}

