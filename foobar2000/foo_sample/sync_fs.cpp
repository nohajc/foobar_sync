#include "stdafx.h"
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/URI.h>
#include <string>
#include <iostream>
#include <sstream>

static const char syncfs_prefix[] = "sync://";
static const unsigned syncfs_prefix_len = 7;

using namespace Poco;
using namespace Net;

class sync_file : public file_readonly {
	int ref_count;
	pfc::string8 mime_type;
	t_filesize file_size;
	t_filesize file_offset;

	URI uri;
	HTTPClientSession session;
	std::string path;

public:
	sync_file(const char * url) : uri(url), session(uri.getHost(), uri.getPort()), path(uri.getPathAndQuery()) {
		ref_count = 0;
		file_offset = 0;

		if (path.empty()) {
			path = "/";
		}

		HTTPRequest req(HTTPRequest::HTTP_HEAD, path, HTTPMessage::HTTP_1_1);
		HTTPResponse resp;

		session.sendRequest(req);
		std::istream & resp_body = session.receiveResponse(resp);

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
	}

	int service_release() {
		return --ref_count;
	}

	int service_add_ref() {
		return ++ref_count;
	}

	t_size read(void * p_buffer, t_size p_bytes, abort_callback & p_abort) {
		return 0; // TODO: implement
	}

	t_filesize get_size(abort_callback & p_abort) {
		return file_size;
	}

	t_filesize get_position(abort_callback & p_abort) {
		return file_offset;
	}

	void seek(t_filesize p_position, abort_callback & p_abort) {
		file_offset = p_position;
	}

	bool can_seek() {
		return true;
	}

	bool get_content_type(pfc::string_base & p_out) {
		p_out = mime_type;
		return true;
	}

	bool is_remote() {
		return true;
	}

	void reopen(abort_callback & p_abort) {
		seek(0, p_abort);
	}
};

class sync_fs : public filesystem {
public:
	FB2K_MAKE_SERVICE_INTERFACE(sync_fs, filesystem);
};

class sync_fs_impl : public sync_fs {
public:
	bool is_our_path(const char * path) {
		if (strncmp(path, syncfs_prefix, syncfs_prefix_len) == 0) {
			console::printf("Got sync path %s.", path);
			return true;
		}
		return false;
	}

	void open(service_ptr_t<file> & p_out, const char * path, t_open_mode mode, abort_callback & p_abort) {
		console::print("CALLED OUR OPEN");

		pfc::string8 new_path = "http://";
		new_path += (path + syncfs_prefix_len);
		p_out = service_ptr_t<sync_file>(new sync_file(new_path));

		/*filesystem::g_open(p_out, new_path, mode, p_abort);

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
		}*/
	}

	bool supports_content_types() {
		return true;
	}

	bool get_canonical_path(const char * path, pfc::string_base & out) {
		out = path;
		return true;
	}

	bool get_display_path(const char * path, pfc::string_base & out) {
		out = path;
		return true;
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

const GUID sync_fs::class_guid = { 0xaef1a5b2, 0xf4d2, 0x4afa, { 0xaa, 0xf6, 0xf1, 0xec, 0x6f, 0x3a, 0x71, 0xe7b } };

template<typename T>
class fs_factory_t : public service_factory_single_t<T> {};

static fs_factory_t<sync_fs_impl> g_syncfs_factory;