#ifndef __NAMES__
#define __NAMES__
#include <algorithm>
#include <string>

inline bool CaseInsensitiveComparator(const std::string &lhs, const std::string &rhs)
{
	return std::lexicographical_compare(lhs.cbegin(),
										lhs.cend(),
										rhs.cbegin(),
										rhs.cend(),
										[](char lhs, char rhs) -> bool { return std::toupper(lhs) < std::toupper(rhs); });
}

#endif