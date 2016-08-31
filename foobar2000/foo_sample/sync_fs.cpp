#include "stdafx.h"

static const char syncfs_prefix[] = "sync://";
static const unsigned syncfs_prefix_len = 7;

class sync_fs : public filesystem {
public:
	FB2K_MAKE_SERVICE_INTERFACE(sync_fs, filesystem);
};

class sync_fs_impl : public sync_fs {
public:
	bool is_our_path(const char * path) {
		if (strncmp(path, syncfs_prefix, syncfs_prefix_len) == 0) {
			console::printf("Opened sync path %s.", path);
			return true;
		}
		return false;
	}

	void open(service_ptr_t<file> & p_out, const char * path, t_open_mode mode, abort_callback & p_abort) {

	}

	bool supports_content_types() {
		return true;
	}

	bool get_canonical_path(const char * path, pfc::string_base & out) {
		return false;
	}

	bool get_display_path(const char * path, pfc::string_base & out) {
		return false;
	}
	void remove(const char * path, abort_callback & p_abort) {}
	void move(const char * src, const char * dst, abort_callback & p_abort) {}
	bool is_remote(const char * src) {
		return false;
	}
	bool relative_path_create(const char * file_path, const char * playlist_path, pfc::string_base & out) {
		return false;
	}
	bool relative_path_parse(const char * relative_path, const char * playlist_path, pfc::string_base & out) {
		return false;
	}

	void create_directory(const char * path, abort_callback &) {}
	void list_directory(const char * p_path, directory_callback & p_out, abort_callback & p_abort) {}
	void get_stats(const char * p_path, t_filestats & p_stats, bool & p_is_writeable, abort_callback & p_abort) {}
};

const GUID sync_fs::class_guid = { 0xaef1a5b2, 0xf4d2, 0x4afa,{ 0xaa, 0xf6, 0xf1, 0xec, 0x6f, 0x3a, 0x71, 0xe7b } };

template<typename T>
class fs_factory_t : public service_factory_single_t<T> {};

static fs_factory_t<sync_fs_impl> g_syncfs_factory;