#pragma once

#include <cstdint>
#include <unordered_map>
#include <boost/bimap.hpp>

class fn_hook {
	typedef std::unordered_map<uintptr_t, uintptr_t> ptr_map_t;
	typedef boost::bimap<uintptr_t, uintptr_t> ptr_bimap_t;
	ptr_bimap_t hook_table;

	uintptr_t pIATStart, pIATEnd;

	void patchIAT(LPVOID IATAddr, LPVOID newfnc);
	fn_hook(const ptr_map_t & ht, HINSTANCE module_handle);

	// fn_hook is a singleton
	static fn_hook * create_instance(const ptr_map_t & ht, HINSTANCE module_handle);
	static bool instance_created;
public:
	static fn_hook * get_instance();

	class builder {
		ptr_map_t hook_table;
	public:
		template<class F>
		builder & hook_fn_with(F * fn_orig, F * fn_hook) {
			uintptr_t fn_orig_ptr = (uintptr_t)fn_orig;
			uintptr_t fn_hook_ptr = (uintptr_t)fn_hook;
			hook_table[fn_orig_ptr] = fn_hook_ptr;

			return *this;
		}

		fn_hook & build(HINSTANCE module_handle = nullptr) {
			return *create_instance(hook_table, module_handle);
		}
	};

	fn_hook(const fn_hook &) = delete;
	void operator=(const fn_hook &) = delete;

	~fn_hook();

	template<class F>
	F * get_orig_fn(F * fn_hook) {
		uintptr_t fn_hook_ptr = (uintptr_t)fn_hook;
		auto htable_entry_it = hook_table.right.find(fn_hook_ptr);
		if (htable_entry_it != hook_table.right.end()) {
			return (F*)htable_entry_it->second;
		}
		return nullptr;
	}
};