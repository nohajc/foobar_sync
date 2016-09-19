#pragma once

#include "stdafx.h"

HANDLE WINAPI MyCreateFileW(
	LPCWSTR filename,
	DWORD access,
	DWORD sharing,
	LPSECURITY_ATTRIBUTES sa,
	DWORD creation,
	DWORD attributes,
	HANDLE hTemplate
);