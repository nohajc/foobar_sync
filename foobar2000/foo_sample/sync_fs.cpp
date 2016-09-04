#include "stdafx.h"

#include "sync_fs.h"

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

static const char syncfs_prefix[] = "sync://";
static const unsigned syncfs_prefix_len = 7;

using namespace Poco;
using namespace Net;


sync_file::sync_file(const char * url) /*: uri(url), http_session(uri.getHost(), uri.getPort()), path(uri.getPathAndQuery())*/ {
	ref_count = 0;
	file_offset = 0;

	if (path.empty()) {
		path = "/";
	}

	/*HTTPRequest req(HTTPRequest::HTTP_HEAD, path, HTTPMessage::HTTP_1_1);
	HTTPResponse resp;

	http_session.sendRequest(req);
	std::istream & resp_body = http_session.receiveResponse(resp);

	if (resp.getStatus() == HTTPResponse::HTTP_NOT_FOUND) {
		// Error 404
		throw exception_io_not_found();
	}
	assert(resp.getStatus() == HTTPResponse::HTTP_OK);

	mime_type = resp.get("Content-Type").c_str();
	std::stringstream ss(resp.get("Content-Length"));
	ss >> file_size;

	console::print(mime_type);
	console::printf("%d", file_size);

	try {
		console::print(resp.get("Accept-Ranges").c_str());
	}
	catch (NotFoundException) {
		console::print("Byte ranges not supported!");
	}*/
}

int sync_file::service_release() {
	return --ref_count;
}

int sync_file::service_add_ref() {
	return ++ref_count;
}

t_size sync_file::read(void * p_buffer, t_size p_bytes, abort_callback & p_abort) {
	return 0; // TODO: implement
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
	return true;
}

void sync_file::reopen(abort_callback & p_abort) {
	seek(0, p_abort);
}


sync_fs_impl::sync_fs_impl() {
	using namespace libtorrent;

	//session & s = torrent_session;		
	//error_code ec;
	// TODO: Initialize torrent client
	/*s.listen_on(std::make_pair(6881, 6889), ec);
	if (ec) {
		console::print(ec.message().c_str());
		return;
	}

	s.start_upnp();
	s.start_natpmp();

	alert_handler_thread = std::thread(&sync_fs_impl::alert_handler, this);*/
}

sync_fs_impl::~sync_fs_impl() {
	// TODO: terminate alert_handler thread
}

/*libtorrent::session & sync_fs_impl::get_torrent_session() {
	return torrent_session;
}*/

decltype(sync_fs_impl::pl) & sync_fs_impl::get_playlist_map() {
	return pl;
}

void sync_fs_impl::alert_handler() {
	using namespace libtorrent;

	while (true) {
		/*while (!torrent_session.wait_for_alert(libtorrent::time_duration(1000)));
		auto a = torrent_session.pop_alert();*/
	}
}

bool sync_fs_impl::is_our_path(const char * path) {
	if (strncmp(path, syncfs_prefix, syncfs_prefix_len) == 0) {
		console::printf("Got sync path %s.", path);
		return true;
	}
	return false;
}

void sync_fs_impl::open(service_ptr_t<file> & p_out, const char * path, t_open_mode mode, abort_callback & p_abort) {
	console::print("CALLED OUR OPEN");

	pfc::string8 new_path = "http://";
	new_path += (path + syncfs_prefix_len);
	//p_out = service_ptr_t<sync_file>(new sync_file(new_path));

	filesystem::g_open(p_out, new_path, mode, p_abort);

	service_ptr_t<filesystem> fs = filesystem::g_get_interface(new_path);

	if (p_out->can_seek()) {
		console::print("CAN SEEK :)");
	}
	else {
		console::print("CANNOT SEEK!");
	}
	if (fs->supports_content_types()) {
		console::print("SUPPORTS MIME TYPES :)");
		pfc::string8 mime;
		p_out->get_content_type(mime);
		console::print(mime);
	}
	else {
		console::print("DOES NOT SUPPORT MIME TYPES!");
	}
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
void sync_fs_impl::get_stats(const char * p_path, t_filestats & p_stats, bool & p_is_writeable, abort_callback & p_abort) {}

const GUID sync_fs::class_guid = { 0xaef1a5b2, 0xf4d2, 0x4afa, { 0xaa, 0xf6, 0xf1, 0xec, 0x6f, 0x3a, 0x71, 0xe7b } };

static fs_factory_t<sync_fs_impl> g_syncfs_factory;