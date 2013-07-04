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

#include <ctype.h>
#include <string>
#include <algorithm>

#include "../urlencode.hpp"
template<class BaseIterator>
struct url_encode_iterator
{

	BaseIterator m_baseposition;

	// 存放 %32 这样的 url 编码.
	typename BaseIterator::value_type	cur_char_buf[3];
	std::size_t							cur_char_pos;

private:
	/* Converts an integer value to its hex character*/
	static char to_hex(char code)
	{
		static char hex[] = "0123456789ABCDEF";
		return hex[code & 15];
	}

	template<class CharTtpe>
	static bool need_encode(const CharTtpe c)
	{
		return ! (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~');
	}

	bool need_encode() const
	{
		reference c = *m_baseposition;
		return need_encode(c);
	}

	void fill_current()
	{
		cur_char_pos = 0;
		std::fill_n(cur_char_buf, sizeof cur_char_buf, 0);
		//判断是否需要编码.
		if(need_encode())
		{
			typename BaseIterator::reference c = *m_baseposition;
			// 将编码后的内容放到这里.
			cur_char_buf[0] = '%';
			cur_char_buf[1] = to_hex(c >> 4);
			cur_char_buf[2] = to_hex(c & 15);
		}
	}

	void base_inc()
	{
		++m_baseposition;

		fill_current();
	}

	void inc()
	{
		// 移动指针到下一个字母.
		if(need_encode())
		{
			cur_char_pos ++;

			if(cur_char_pos  > 2 || cur_char_buf[cur_char_pos] == '\0')
			{
				// 读到头了, 移动下一个 m_baseposition
				base_inc();
			}
		}
		else
		{
			base_inc();
		}
	}
public:
	typedef typename BaseIterator::iterator_category iterator_category;
	typedef typename BaseIterator::value_type        value_type;
	typedef typename BaseIterator::difference_type   difference_type;
	typedef typename BaseIterator::pointer           pointer;
	typedef typename BaseIterator::reference         reference;
public:

	url_encode_iterator(BaseIterator base_iter)
		: cur_char_pos(0), m_baseposition(base_iter)
	{
		fill_current();
	}

	void operator ++()
	{
		inc();
	}

	void operator ++(int)
	{
		inc();
	}

	reference operator* () const
	{
		// 取出 当前字符
		if(need_encode())
			return cur_char_buf[cur_char_pos];

		return * m_baseposition;
	}

	bool operator == (const url_encode_iterator & rhs) const
	{
		return m_baseposition == rhs.m_baseposition && cur_char_pos == rhs.cur_char_pos;
	}

	bool operator != (const url_encode_iterator & rhs) const
	{
		return m_baseposition != rhs.m_baseposition || cur_char_pos != rhs.cur_char_pos;
	}
private:
	struct need_encode_func{

	template<class CharType>
	bool operator()(const CharType c)
	{
		return need_encode(c);
	}

	};
public:
	difference_type operator - (const url_encode_iterator & rhs) const
	{
		// 遍历到底, 然后计算有多少 need_encode , need_encode 的数量 * 2 + 字符串长度即可.
		return (m_baseposition - rhs.m_baseposition) + 2*  std::count_if(rhs.m_baseposition, m_baseposition , need_encode_func() );
	}
};

std::string url_encode(const std::string str)
{
	return std::string(url_encode_iterator<std::string::const_iterator>(str.begin()),
		url_encode_iterator<std::string::const_iterator>(str.end()) );
}



// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
