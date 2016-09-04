#include "stdafx.h"
#include "string_helpers.h"

#include <sstream>
#include <iomanip>

std::string wstringToString(std::wstring string_in) {
	size_t len = WideCharToMultiByte(CP_UTF8, 0, &string_in[0], string_in.size(), NULL, 0, NULL, NULL);

	std::string string_s(len, 0);
	WideCharToMultiByte(CP_UTF8, 0, &string_in[0], string_in.size(), &string_s[0], len, NULL, NULL);

	return string_s;
}

std::string uintToHexString(unsigned i, int width) {
	std::stringstream ss;
	ss << std::setfill('0') << std::setw(width) << std::hex << i;

	return ss.str();
}

std::string sha1ToHexString(std::string bin_str) {
	std::string hex;

	for (char c : bin_str) {
		hex += uintToHexString((unsigned char)c, 2);
	}

	return hex;
}