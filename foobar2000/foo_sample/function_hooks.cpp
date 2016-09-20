#include "stdafx.h"
#include "function_hooks.h"
#include "hook_framework.h"
#include "sync_manager.h"

#include <vector>
#include <unordered_map>
#include <string>
#include <memory>

HANDLE WINAPI MyCreateFileW(
	LPCWSTR filename,
	DWORD access,
	DWORD sharing,
	LPSECURITY_ATTRIBUTES sa,
	DWORD creation,
	DWORD attributes,
	HANDLE hTemplate
) {
	fn_hook * h = fn_hook::get_instance();
	assert(h != nullptr);
	sync_manager & m = sync_manager::get_instance();
	auto & vp = m.get_virtual_paths();

	auto CreateFileW_Orig = h->get_orig_fn(MyCreateFileW);

	std::wstring our_filename = filename;
	if (our_filename.substr(0, 4) == TEXT("\\\\?\\")) {
		our_filename = our_filename.substr(4);
	}

	auto vp_entry_it = vp.find(our_filename);
	if (vp_entry_it != vp.end()) {
		// Virtual file path, translate it:
		return CreateFileW_Orig(vp_entry_it->second.c_str(), access, sharing, sa, creation, attributes, hTemplate);
	}

	return CreateFileW_Orig(filename, access, sharing, sa, creation, attributes, hTemplate);
}

struct SearchHandle {
	std::vector<std::wstring> & file_list;
	std::vector<std::wstring>::iterator file_it;

	SearchHandle(std::vector<std::wstring> & fl) : file_list(fl) {
		file_it = file_list.begin();
	}

	std::vector<std::wstring>::iterator next() {
		auto ret = file_it;
		if (ret != file_list.end()) file_it++;
		return ret;
	}

	std::vector<std::wstring>::iterator end() {
		return file_list.end();
	}
};

std::unordered_map<int, std::unique_ptr<SearchHandle>> g_search_handles;
int g_search_handle_cnt = (int)INVALID_HANDLE_VALUE;

HANDLE WINAPI MyFindFirstFileW(
	_In_  LPCWSTR            lpFileName,
	_Out_ LPWIN32_FIND_DATAW lpFindFileData
) {
	fn_hook * h = fn_hook::get_instance();
	assert(h != nullptr);
	sync_manager & m = sync_manager::get_instance();
	auto & vd = m.get_virtual_dirs();

	auto FindFirstFileW_Orig = h->get_orig_fn(MyFindFirstFileW);

	auto vd_entry_it = vd.find(lpFileName);
	if (vd_entry_it != vd.end()) {
		// Virtual directory path, we must create fake search handle
		auto & file_list = vd_entry_it->second;
		// Create new SearchHandle and store it into a global table
		auto s_handle_it_optional = g_search_handles.emplace(--g_search_handle_cnt, std::make_unique<SearchHandle>(file_list));
		assert(s_handle_it_optional.second);
		auto s_handle_it = s_handle_it_optional.first;

		int s_handle_idx = s_handle_it->first;
		SearchHandle & s_handle = *s_handle_it->second;
		// libtorrent only cares about cFileName, we don't have to fill the rest
		auto file_it = s_handle.next();
		if (file_it != s_handle.end()) {
			wcscpy_s(lpFindFileData->cFileName, file_it->c_str());
			return new int(s_handle_idx);
		}

		SetLastError(ERROR_FILE_NOT_FOUND);
		return INVALID_HANDLE_VALUE;
	}

	return FindFirstFileW_Orig(lpFileName, lpFindFileData);
}

BOOL WINAPI MyFindNextFileW(
	_In_  HANDLE             hFindFile,
	_Out_ LPWIN32_FIND_DATAW lpFindFileData
) {
	fn_hook * h = fn_hook::get_instance();
	assert(h != nullptr);

	auto FindNextFileW_Orig = h->get_orig_fn(MyFindNextFileW);

	int s_handle_idx = *(int*)hFindFile;
	if (s_handle_idx < 0) {
		// This is our fake handle
		SearchHandle & s_handle = *g_search_handles[s_handle_idx];

		auto file_it = s_handle.next();
		if (file_it != s_handle.end()) {
			wcscpy_s(lpFindFileData->cFileName, file_it->c_str());
			return TRUE;
		}

		SetLastError(ERROR_NO_MORE_FILES);
		return FALSE;
	}

	return FindNextFileW_Orig(hFindFile, lpFindFileData);
}

BOOL WINAPI MyFindClose(
	_Inout_ HANDLE hFindFile
) {
	fn_hook * h = fn_hook::get_instance();
	assert(h != nullptr);

	auto FindClose_Orig = h->get_orig_fn(MyFindClose);

	int s_handle_idx = *(int*)hFindFile;
	if (s_handle_idx < 0) {
		// This is our fake handle
		g_search_handles.erase(s_handle_idx);
		delete hFindFile;
		return TRUE;
	}

	return FindClose_Orig(hFindFile);
}