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

HANDLE WINAPI MyFindFirstFileW(
	_In_  LPCWSTR            lpFileName,
	_Out_ LPWIN32_FIND_DATAW lpFindFileData
);

BOOL WINAPI MyFindNextFileW(
	_In_  HANDLE             hFindFile,
	_Out_ LPWIN32_FIND_DATAW lpFindFileData
);

BOOL WINAPI MyFindClose(
	_Inout_ HANDLE hFindFile
);