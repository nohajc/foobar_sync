#pragma once

#include "stdafx.h"
#include <string>
#include <vector>

std::string wstringToString(std::wstring string_in);
std::wstring stringToWstring(std::string string_in);
std::string sha1ToHexString(std::string bin_str);
std::string hexStringToSha1(std::string hex_str);
std::vector<std::string> split(const std::string &s, char delim);
std::string MimeTypeFromString(const std::string& str);