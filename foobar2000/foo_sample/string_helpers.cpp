#include "stdafx.h"
#include "string_helpers.h"

std::string wstringToString(std::wstring string_in) {
	size_t len = WideCharToMultiByte(CP_UTF8, 0, &string_in[0], string_in.size(), NULL, 0, NULL, NULL);

	std::string string_s(len, 0);
	WideCharToMultiByte(CP_UTF8, 0, &string_in[0], string_in.size(), &string_s[0], len, NULL, NULL);

	return string_s;
}