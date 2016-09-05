#include "stdafx.h"
#include "string_helpers.h"

#include <urlmon.h>

#include <sstream>
#include <iomanip>
#include <vector>

std::string wstringToString(std::wstring string_in) {
	size_t len = WideCharToMultiByte(CP_UTF8, 0, &string_in[0], string_in.size(), NULL, 0, NULL, NULL);

	std::string string_s(len, 0);
	WideCharToMultiByte(CP_UTF8, 0, &string_in[0], string_in.size(), &string_s[0], len, NULL, NULL);

	return string_s;
}

std::wstring stringToWstring(std::string string_in) {
	size_t len = MultiByteToWideChar(CP_UTF8, 0, &string_in[0], string_in.size(), NULL, 0);

	std::wstring wstring_s(len, 0);
	MultiByteToWideChar(CP_UTF8, 0, &string_in[0], string_in.size(), &wstring_s[0], len);

	return wstring_s;
}

std::string uintToHexString(unsigned i, int width) {
	std::stringstream ss;
	ss << std::setfill('0') << std::setw(width) << std::hex << i;

	return ss.str();
}

unsigned hexStringToUInt(const char * hs, int width) {
	return strtoul(std::string(hs, hs + width).c_str(), NULL, 16);
}

std::string sha1ToHexString(std::string bin_str) {
	std::string hex;

	for (char c : bin_str) {
		hex += uintToHexString((unsigned char)c, 2);
	}

	return hex;
}

std::string hexStringToSha1(std::string hex_str) {
	std::string bin;
	unsigned i = 0;

	for (i = 0; i < hex_str.size(); i += 2) {
		bin.push_back(hexStringToUInt(&hex_str[i], 2));
	}

	return bin;
}

void split(const std::string &s, char delim, std::vector<std::string> &elems) {
	std::stringstream ss;
	ss.str(s);
	std::string item;
	while (getline(ss, item, delim)) {
		elems.push_back(item);
	}
}


std::vector<std::string> split(const std::string &s, char delim) {
	std::vector<std::string> elems;
	split(s, delim, elems);
	return elems;
}

std::string MimeTypeFromString(const std::string& str) {
	std::string ext = str;

	if (ext[0] != '.') {
		ext = "." + ext;
	}

	LPWSTR pwzMimeOut = NULL;
	HRESULT hr = FindMimeFromData(NULL, stringToWstring(ext).c_str(), NULL, 0,
		NULL, FMFD_URLASFILENAME, &pwzMimeOut, 0x0);
	if (SUCCEEDED(hr)) {
		std::wstring strResult(pwzMimeOut);
		CoTaskMemFree(pwzMimeOut);
		return wstringToString(strResult);
	}

	return "";
}
