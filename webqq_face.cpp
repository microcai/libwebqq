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

#include "webqq_impl.h"

void qq::WebQQ::init_face_map()
{
#define MAP(X,Y)	do{facemap.insert(std::make_pair((X),(Y)));}while(0)

	MAP(0,14);
	MAP(14,0);
	MAP(50,15);
	MAP(51,16);
	MAP(96,17);
	MAP(53,18);
	MAP(54,19);
	MAP(73,20);
	MAP(74,21);
	MAP(75,22);
	MAP(76,23);
	MAP(77,24);
	MAP(78,25);
	MAP(56,26);
	MAP(55,27);
	MAP(57, 28);
	MAP(58, 29);
	MAP(79, 30);
	MAP(80, 31);
	MAP(81, 32);
	MAP(83, 33);
	MAP(82, 34);
	MAP(84, 35);
	MAP(85, 36);
	MAP(86, 37);
	MAP(87, 38);
	MAP(88, 39);
	MAP(97, 40);
	MAP(98, 41);
	MAP(99, 42);
	MAP(100, 43);
	MAP(101, 44);
	MAP(102, 45);
	MAP(103, 46);
	MAP(104, 47);
	MAP(105, 48);
	MAP(106, 49);
	MAP(108, 50);
	MAP(107, 51);
	MAP(109, 52);
	MAP(110, 53);
	MAP(111, 54);
	MAP(112, 55);
	MAP(32, 56);
	MAP(113, 57);
	MAP(114, 58);
	MAP(115, 59);
	MAP(63, 60);
	MAP(64, 61);
	MAP(59, 62);
	MAP(33, 63);
	MAP(34, 64);
	MAP(116, 65);
	MAP(36, 66);
	MAP(37, 67);
	MAP(38, 68);
	MAP(91, 69);
	MAP(92, 70);
	MAP(93, 71);
	MAP(29, 72);
	MAP(117, 73);
	MAP(72, 74);
	MAP(45, 75);
	MAP(42, 76);
	MAP(39, 77);
	MAP(62, 78);
	MAP(46, 79);
	MAP(47, 80);
	MAP(71, 81);
	MAP(95, 82);
	MAP(118, 83);
	MAP(119, 84);
	MAP(120, 85);
	MAP(121, 86);
	MAP(122, 87);
	MAP(123, 88);
	MAP(124, 89);
	MAP(27, 90);
	MAP(21, 91);
	MAP(23, 92);
	MAP(25, 93);
	MAP(26, 94);
	MAP(125, 95);
	MAP(126, 96);
	MAP(127, 97);
	MAP(128, 98);
	MAP(129, 99);
	MAP(130, 100);
	MAP(131, 101);
	MAP(132, 102);
	MAP(133, 103);
	MAP(134, 104);

	for(int i=0;i<=134;i++)
		facemap.insert(std::make_pair(i,i));

}

