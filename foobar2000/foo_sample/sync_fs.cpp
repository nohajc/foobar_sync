#include "stdafx.h"

#include "sync_fs.h"
#include "string_helpers.h"
#include "sync_manager.h"

#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/URI.h>

#include <boost/asio/impl/src.hpp>

#include <libtorrent/entry.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/session.hpp>

#include <string>
#include <iostream>
#include <sstream>
#include <thread>

//static const char syncfs_prefix[] = "sync://";
//static const unsigned syncfs_prefix_len = 7;

using namespace Poco;
using namespace Net;


sync_file::sync_file(const char * url) {
	ref_count = 0;
	file_offset = 0;
	std::string ext;
	time_t mtime;

	pl = file_stats(url, file_size, file_idx, ext, mtime);

	mime_type = MimeTypeFromString(ext).c_str();
}

sync_playlist * sync_file::file_stats(const char * path, t_filesize & file_size, int & file_idx, std::string & ext, time_t & mtime) {
	auto path_tokens = split(path + sync_fs::prefix_len, '/');
	// Find torrent playlist by infohash
	auto & pl_map = sync_manager::get_instance().get_playlist_map();
	libtorrent::sha1_hash infohash(hexStringToSha1(path_tokens[0]));

	auto it = pl_map.find(infohash);
	if (it == pl_map.end()) {
		console::print("HASH NOT FOUND!");
		return nullptr; // TODO: throw exception
	}

	sync_playlist * pl = it->second.get();

	// Set file index (index of file in the torrent)
	std::stringstream ss;
	ss << path_tokens[1];
	ss >> file_idx;

	auto & tf = pl->get_info().file_at(file_idx);
	file_size = tf.size;
	mtime = tf.mtime;
	ext = split(path_tokens.back(), '.').back();

	return pl;
}

int sync_file::service_release() {
	int ret = --ref_count;
	if (ret == 0) {
		delete this;
	}

	return ret;
}

int sync_file::service_add_ref() {
	return ++ref_count;
}

t_size sync_file::read(void * p_buffer, t_size p_bytes, abort_callback & p_abort) {
	console::printf("Foobar wants to read %u bytes.", p_bytes);

	t_size bytes_read = pl->read_file(file_idx, p_buffer, file_offset, p_bytes, p_abort);
	file_offset += bytes_read;
	return bytes_read;
}

t_filesize sync_file::get_size(abort_callback & p_abort) {
	return file_size;
}

t_filesize sync_file::get_position(abort_callback & p_abort) {
	return file_offset;
}

void sync_file::seek(t_filesize p_position, abort_callback & p_abort) {
	if (p_position > file_size) {
		throw exception_io_seek_out_of_range();
	}
	file_offset = p_position;
}

bool sync_file::can_seek() {
	return true;
}

bool sync_file::get_content_type(pfc::string_base & p_out) {
	p_out = mime_type;
	return true;
}

bool sync_file::is_remote() {
	return false;
}

void sync_file::reopen(abort_callback & p_abort) {
	seek(0, p_abort);
}

const char sync_fs::prefix[] = "sync://";
const unsigned sync_fs::prefix_len = 7;

//sync_fs_impl::sync_fs_impl() {}
//sync_fs_impl::~sync_fs_impl() {}


bool sync_fs_impl::is_our_path(const char * path) {
	if (strncmp(path, sync_fs::prefix, sync_fs::prefix_len) == 0) {
		console::printf("Got sync path %s.", path);
		return true;
	}
	return false;
}

void sync_fs_impl::open(service_ptr_t<file> & p_out, const char * path, t_open_mode mode, abort_callback & p_abort) {
	console::print("CALLED OUR OPEN");
		
	p_out = service_ptr_t<sync_file>(new sync_file(path));
}

bool sync_fs_impl::supports_content_types() {
	return true;
}

bool sync_fs_impl::get_canonical_path(const char * path, pfc::string_base & out) {
	out = path;
	return true;
}

bool sync_fs_impl::get_display_path(const char * path, pfc::string_base & out) {
	out = path;
	return true;
}
void sync_fs_impl::remove(const char * path, abort_callback & p_abort) {}
void sync_fs_impl::move(const char * src, const char * dst, abort_callback & p_abort) {}

bool sync_fs_impl::is_remote(const char * src) {
	return false;
}
bool sync_fs_impl::relative_path_create(const char * file_path, const char * playlist_path, pfc::string_base & out) {
	return false;
}
bool sync_fs_impl::relative_path_parse(const char * relative_path, const char * playlist_path, pfc::string_base & out) {
	return false;
}

void sync_fs_impl::create_directory(const char * path, abort_callback &) {}
void sync_fs_impl::list_directory(const char * p_path, directory_callback & p_out, abort_callback & p_abort) {}

void sync_fs_impl::get_stats(const char * p_path, t_filestats & p_stats, bool & p_is_writeable, abort_callback & p_abort) {
	int file_idx;
	std::string ext;
	time_t mtime;

	sync_file::file_stats(p_path, p_stats.m_size, file_idx, ext, mtime);
	p_is_writeable = false;
	p_stats.m_timestamp = mtime;
}

const GUID sync_fs::class_guid = { 0xaef1a5b2, 0xf4d2, 0x4afa, { 0xaa, 0xf6, 0xf1, 0xec, 0x6f, 0x3a, 0x71, 0xe7b } };

static fs_factory_t<sync_fs_impl> g_syncfs_factory;