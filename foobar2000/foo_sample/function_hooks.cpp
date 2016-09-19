#include "stdafx.h"
#include "function_hooks.h"
#include "hook_framework.h"

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

	console::print("MyCreateFileW called!");

	auto CreateFileW_Orig = h->get_orig_fn(MyCreateFileW);
	return CreateFileW_Orig(filename, access, sharing, sa, creation, attributes, hTemplate);
}