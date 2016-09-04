#pragma once

#include "stdafx.h"

#include "sync_playlist.h"

#include <libtorrent/entry.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/session.hpp>

#include <memory>
#include <thread>
#include <unordered_map>


class sync_file : public file_readonly {
	int ref_count;
	pfc::string8 mime_type;
	t_filesize file_size;
	t_filesize file_offset;

	/*URI uri;
	HTTPClientSession http_session;*/
	std::string path;

public:
	sync_file(const char * url);

	int service_release();

	int service_add_ref();

	t_size read(void * p_buffer, t_size p_bytes, abort_callback & p_abort);

	t_filesize get_size(abort_callback & p_abort);

	t_filesize get_position(abort_callback & p_abort);

	void seek(t_filesize p_position, abort_callback & p_abort);

	bool can_seek();

	bool get_content_type(pfc::string_base & p_out);

	bool is_remote();

	void reopen(abort_callback & p_abort);
};

class sync_fs : public filesystem {
public:
	FB2K_MAKE_SERVICE_INTERFACE(sync_fs, filesystem);
};

class sync_fs_impl : public sync_fs {
	/*libtorrent::session torrent_session;
	std::thread alert_handler_thread;
	std::map<libtorrent::sha1_hash, std::unique_ptr<sync_playlist>> pl;*/

public:
	sync_fs_impl();
	~sync_fs_impl();

	bool is_our_path(const char * path);

	void open(service_ptr_t<file> & p_out, const char * path, t_open_mode mode, abort_callback & p_abort);

	bool supports_content_types();

	bool get_canonical_path(const char * path, pfc::string_base & out);

	bool get_display_path(const char * path, pfc::string_base & out);
	void remove(const char * path, abort_callback & p_abort);
	void move(const char * src, const char * dst, abort_callback & p_abort);

	bool is_remote(const char * src);

	bool relative_path_create(const char * file_path, const char * playlist_path, pfc::string_base & out);

	bool relative_path_parse(const char * relative_path, const char * playlist_path, pfc::string_base & out);

	void create_directory(const char * path, abort_callback &);
	void list_directory(const char * p_path, directory_callback & p_out, abort_callback & p_abort);
	void get_stats(const char * p_path, t_filestats & p_stats, bool & p_is_writeable, abort_callback & p_abort);
};

template<typename T>
class fs_factory_t : public service_factory_single_t<T> {};
