#include "stdafx.h"
#include "hook_framework.h"

fn_hook::fn_hook(const ptr_map_t & ht, HINSTANCE module_handle) {
	hook_table.left.insert(ht.begin(), ht.end());
	// Hook IAT
	HANDLE hself = nullptr;
	if (module_handle == nullptr) {
		hself = GetModuleHandle(nullptr);
	}
	else {
		hself = module_handle;
	}
	PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)hself;
	PIMAGE_NT_HEADERS32 pNTHeaders = (PIMAGE_NT_HEADERS32)(pDosHeader->e_lfanew + (uintptr_t)hself);

	pIATStart = (uintptr_t)hself + pNTHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].VirtualAddress;
	pIATEnd = (uintptr_t)pIATStart + pNTHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].Size;

	for (uintptr_t pIAT = pIATStart; pIAT < pIATEnd; pIAT += sizeof(uintptr_t)) {
		uintptr_t pfn = *(uintptr_t*)pIAT;
		auto htable_entry_it = hook_table.left.find(pfn);
		if (htable_entry_it != hook_table.left.end()) {
			patchIAT((LPVOID)pIAT, (LPVOID)htable_entry_it->second);
		}
	}
}

fn_hook * fn_hook::create_instance(const ptr_map_t & ht, HINSTANCE module_handle) {
	static fn_hook hook_obj(ht, module_handle);
	instance_created = true;
	return &hook_obj;
}

fn_hook * fn_hook::get_instance() {
	ptr_map_t * nil = nullptr;
	if (!instance_created) {
		return nullptr; // get_instance() should be called after fn_hook::builder::build()
	}
	// Invalid reference *nil should not matter.
	// Static hook_obj is already created and we just need to retrieve it
	return create_instance(*nil, nullptr);
}

bool fn_hook::instance_created = false;

fn_hook::~fn_hook() {
	for (uintptr_t pIAT = pIATStart; pIAT < pIATEnd; pIAT += sizeof(uintptr_t)) {
		uintptr_t pfn = *(uintptr_t*)pIAT;
		auto htable_entry_it = hook_table.right.find(pfn);
		if (htable_entry_it != hook_table.right.end()) {
			patchIAT((LPVOID)pIAT, (LPVOID)htable_entry_it->second);
		}
	}
}

void fn_hook::patchIAT(LPVOID IATAddr, LPVOID newfnc) {
	SYSTEM_INFO sysinfo;
	DWORD oldProtect;

	GetSystemInfo(&sysinfo);
	VirtualProtect(IATAddr, sysinfo.dwPageSize, PAGE_READWRITE, &oldProtect);
		
	*(LPVOID*)IATAddr = newfnc;

	VirtualProtect(IATAddr, sysinfo.dwPageSize, oldProtect, &oldProtect);
}