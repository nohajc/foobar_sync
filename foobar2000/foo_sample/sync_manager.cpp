#include "stdafx.h"

#include "void_async.h"
#include "string_helpers.h"
#include "sync_manager.h"
#include "sync_playlist.h"

#include "dllmain.h"
#include "hook_framework.h"
#include "function_hooks.h"

#include <libtorrent/entry.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/time.hpp>
#include <libtorrent/alert.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/create_torrent.hpp>

#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/URI.h>
#include <Poco/StreamCopier.h>
#include <Poco/Net/NetException.h>

#include <boost/filesystem.hpp>

#include <iostream>
#include <memory>
#include <thread>
#include <unordered_map>
#include <string>
#include <vector>
#include <deque>
#include <algorithm>
#include <iterator>


void sync_manager::on_init() {
	console::print("Starting libtorrent session...");
	torrent_session = std::make_unique<libtorrent::session>();
	console::printf("Sync manager initialized, addr %x.", (intptr_t)this);

	// TODO: Initialize torrent client - could it be done on demand only?
	using namespace libtorrent;

	session & s = *torrent_session;
	error_code ec;
	
	s.set_alert_mask(alert::status_notification | alert::error_notification);

	s.listen_on(std::make_pair(6881, 6889), ec);
	if (ec) {
		console::print(ec.message().c_str());
		return;
	}

	s.start_upnp();
	s.start_natpmp();

	app_running = true;
	alert_handler_thread = std::thread(&sync_manager::alert_handler, this);

	sio_ini = std::make_unique<sio_client_initializer>();

	update_window_callback = nullptr;
	sync_room_event_handlers_set = false;
	setup_sync_room_event_handlers();

	hook_filesystem_functions();
}

void sync_manager::hook_filesystem_functions() {
	fn_hook::builder fh_builder;

	assert(fn_hook::get_instance() == nullptr);
	fh_builder
		.hook_fn_with(CreateFileW, MyCreateFileW)
		.hook_fn_with(FindFirstFileW, MyFindFirstFileW)
		.hook_fn_with(FindNextFileW, MyFindNextFileW)
		.hook_fn_with(FindClose, MyFindClose)
		.build(g_dll_handle);
	assert(fn_hook::get_instance() != nullptr);
}

void sync_manager::on_quit() {
	// This creates memory leak at exit.
	// It is a workaround for heap corruption error
	// which occurs during sio::client destruction.
	sio_ini.release();
	app_running = false;
	alert_handler_thread.join();
	torrent_session.reset();
}

libtorrent::session & sync_manager::get_torrent_session() {
	return *torrent_session;
}

decltype(sync_manager::pl) & sync_manager::get_playlist_map() {
	return pl;
}

void sync_manager::alert_handler() {
	using namespace libtorrent;

	std::deque<alert*> alerts;

	while (app_running) {
		while (!torrent_session->wait_for_alert(milliseconds(500))) {
			//console::print("Waiting for event...");
			if (!app_running) {
				return;
			}
		}

		torrent_session->pop_alerts(&alerts);

		for (auto a : alerts) {
			console::printf("Alert received: %s", a->message().c_str());

			switch (a->type()) {
				case read_piece_alert::alert_type: {
					auto rpa = alert_cast<read_piece_alert>(a);
					sha1_hash ih = rpa->handle.get_torrent_info().info_hash();
					auto & our_pl = pl[ih];

					sync_playlist::read_piece_task task;

					{ // Synchronized access to read_request multimap
						std::lock_guard<std::mutex> guard(our_pl->read_request_mutex);

						// Find a request for this torrent piece
						auto it = our_pl->read_request.find(rpa->piece);
						assert(it != our_pl->read_request.end());
						// Remove promise from the multimap, move it to this local scope
						task = std::move(it->second);
						our_pl->read_request.erase(it);
					}

					// Provide the data to an associated future object
					task(*rpa); // After the value is set, task can be deleted
								//promise.set_value(*rpa); // After the value is set, promise can be deleted
					break;
				}
				default:;
			}

			delete a;
		}

		alerts.clear();
	}
}

void sync_manager::add_torrent_from_url(const char * url) {
	using namespace Poco;
	using namespace Net;

	console::print("Adding torrent from url...");
	
	try {
		URI uri(url);

		HTTPClientSession cs(uri.getHost(), uri.getPort());
		std::string path(uri.getPathAndQuery());

		if (path.empty()) {
			path = "/";
		}

		HTTPRequest req(HTTPRequest::HTTP_GET, path, HTTPMessage::HTTP_1_1);
		HTTPResponse resp;

		cs.sendRequest(req);
		std::istream & resp_body = cs.receiveResponse(resp);

		if (resp.getStatus() == HTTPResponse::HTTP_NOT_FOUND) {
			// Error 404
			//throw exception_io_not_found();
			// TODO: proper error handling

			console::print("Torrent not found.");
			return;
		}
		assert(resp.getStatus() == HTTPResponse::HTTP_OK);

		std::string data;
		std::streamsize copied = StreamCopier::copyToString(resp_body, data);

		console::printf("Content-Length: %u", resp.getContentLength());
		console::printf("Data length: %u", copied);

		add_torrent_from_data(&data[0], copied);
	}
	catch (SyntaxException e) {
		console::print(e.message().c_str());
	}
	catch (NetException e) {
		console::print(e.message().c_str());
	}
}

void sync_manager::add_torrent_from_data(const char * data, std::streamsize size) {
	using namespace libtorrent;

	error_code ec;
	torrent_info * ti = new torrent_info(&data[0], size, ec);

	if (ec)
	{
		console::printf("Error loading torrent: %s\n", ec.message().c_str());
		return;
	}
	else {
		console::print("Torrent file appears to be valid.");
	}

	add_torrent(ti);
}

void sync_manager::add_torrent(libtorrent::torrent_info * ti) {
	using namespace libtorrent;
	using namespace boost::filesystem;

	// Playlist related foobar API does not work outside of the main thread
	fb2k::inMainThread([this, ti]() {
		add_torrent_params p;
		error_code ec;
		session & s = *torrent_session;

		p.ti = ti;

		PWSTR wstr_profile_path;
		HRESULT hr = SHGetKnownFolderPath(FOLDERID_Profile, 0, NULL, &wstr_profile_path);
		assert(SUCCEEDED(hr));

		std::string profile_path = wstringToString(wstr_profile_path);
		CoTaskMemFree(wstr_profile_path);

		path save_path = profile_path / "Foobar torrents";

		if (!exists(save_path)) try {
			create_directory(save_path);
		}
		catch (filesystem_error e) {
			console::print(e.code().message().c_str());
		}

		p.save_path = save_path.generic_string();
		console::print(save_path.generic_string().c_str());

		torrent_handle h = s.add_torrent(p, ec);
		if (ec) {
			console::printf("Error adding torrent to session: %s", ec.message().c_str());
			return;
		}

		pl[ti->info_hash()] = std::make_unique<sync_playlist>(h);
	});
}

void sync_manager::seed_torrent(libtorrent::torrent_info * ti, const std::string & path) {
	using namespace libtorrent;
	using namespace boost::filesystem;

	add_torrent_params p;
	error_code ec;
	session & s = *torrent_session;

	p.ti = ti;
	p.save_path = path;

	torrent_handle h = s.add_torrent(p, ec);
	if (ec) {
		console::printf("Error adding torrent to session: %s", ec.message().c_str());
		return;
	}
}

void sync_manager::share_playlist_as_torrent_async(pfc::list_t<metadb_handle_ptr> items) {
	void_async(&sync_manager::share_playlist_as_torrent, this, items);
}

void sync_manager::share_playlist_as_torrent(pfc::list_t<metadb_handle_ptr> items) {
	using namespace libtorrent;
	namespace bfs = boost::filesystem;

	if (items.get_count() > 0) {
		//session & s = *torrent_session;

		file_storage fs;
		bfs::path temp_path = bfs::temp_directory_path();
		bfs::path torrent_path = temp_path / bfs::unique_path();

		if (!exists(torrent_path)) try {
			create_directory(torrent_path);
		}
		catch (bfs::filesystem_error e) {
			//console::print(e.code().message().c_str());
			console::print(e.what());
			return;
		}

		auto torrent_path_wstr = torrent_path.wstring() + TEXT("\\*");
		auto virt_dir_optional = virtual_dirs.emplace(std::make_pair(torrent_path_wstr, std::vector<std::wstring>()));
		assert(virt_dir_optional.second);
		auto virt_dir_it = virt_dir_optional.first;
		auto & virt_dir_file_list = virt_dir_it->second;

		console::print(torrent_path.string().c_str());
		console::print(temp_path.string().c_str());

		for (t_size i = 0; i < items.get_count(); i++) {
			std::string fpath_str = items[i]->get_path();
			if (fpath_str.substr(0, 7) == "file://") {
				fpath_str = fpath_str.substr(7);
			}
			bfs::path fpath = fpath_str;
			//console::print((torrent_path / fpath.filename()).string().c_str());
			std::wstring virt_path = (torrent_path / fpath.filename()).wstring();
			std::wstring target = fpath.wstring();

			virtual_paths.emplace(std::make_pair(virt_path, target));
			virt_dir_file_list.push_back(fpath.filename().wstring());

			/*if (!CreateSymbolicLink(symlink.c_str(), target.c_str(), 0)) {
				PrintLastError();
				return;
			}*/
			/*try {
				bfs::create_symlink(fpath, torrent_path / fpath.filename());
			}
			catch (bfs::filesystem_error e) {
				console::print("CREATE SYMLINK FAILED");
				console::print(e.what());
				return;
			}*/
		}

		add_files(fs, torrent_path.string(), [](const std::string & p) {
			console::printf("Adding file %s to torrent.", p.c_str());
			return true;
		});

		// Make sure the optimize flag is off,
		// we want to retain the original order of files.
		create_torrent t(fs, 0, -1, 0);
		t.add_tracker(tracker_url);

		set_piece_hashes(t, temp_path.string());

		std::vector<char> buf;
		bencode(std::back_inserter(buf), t.generate());

		console::printf("Created torrent, size: %d", buf.size());

		error_code ec;
		torrent_info * ti = new torrent_info(&buf[0], buf.size(), ec);

		if (ec)
		{
			console::printf("Error loading torrent: %s\n", ec.message().c_str());
			return;
		}
		else {
			console::print("Torrent file appears to be valid.");
		}

		seed_torrent(ti, temp_path.string());
		share_torrent_with_room(buf);
	}
}

void sync_manager::share_torrent_with_room(std::vector<char>& data) {
	auto h = get_sio_client();
	if (!h) {
		return;
	}
	auto & socket = h->socket();

	if (sync_room_joined != "") {
		sio::message::list msg_list;
		msg_list.push(sync_room_joined);
		msg_list.push(std::make_shared<std::string>(&data[0], data.size()));

		socket->emit("share_torrent", msg_list);
	}
}

decltype(sync_manager::virtual_paths) & sync_manager::get_virtual_paths() {
	return virtual_paths;
}

decltype(sync_manager::virtual_dirs) & sync_manager::get_virtual_dirs() {
	return virtual_dirs;
}

sio::client * sync_manager::get_sio_client() {
	auto cl = sio_ini->get_client();
	if (cl == nullptr) {
		sio_ini->connect(sync_srv_url);
		cl = sio_ini->get_client();

		if (cl && !sync_room_event_handlers_set) {
			setup_sync_room_event_handlers();
		}
	}
	
	return cl;
}

void sync_manager::setup_sync_room_event_handlers() {
	auto h = get_sio_client();
	if (!h) {
		return;
	}
	auto & socket = h->socket();

	socket->on("add_torrent", [this](sio::event & e) {
		auto & msgs = e.get_messages();
		assert(msgs.size() == 2);

		auto & data_msg = msgs[0];
		assert(data_msg->flag_binary);
		auto & data_ptr = data_msg->get_binary();
		auto data_raw = &(*data_ptr)[0];

		// TODO: We need to receive and store the id of seeder
		// so that we can later ask him for pieces via websocket.
		auto & id_msg = msgs[1];
		assert(id_msg->flag_string);

		add_torrent_from_data(data_raw, data_ptr->size());
	});

	socket->on("room_list", [this](sio::event & e) {
		auto & msg = e.get_message();
		assert(msg->flag_object);
		auto & obj = msg->get_map();

		{
			std::lock_guard<std::mutex> guard(sync_room_list_mutex);
			sync_room_list.clear();
		}

		console::print("GOT ROOM LIST:");
		for (auto & it : obj) {
			const std::string & room_name = it.first;
			if (room_name.substr(0, 2) != "/#") {
				console::print(it.first.c_str());
				{
					std::lock_guard<std::mutex> guard(sync_room_list_mutex);
					sync_room_list.insert(room_name);
				}
			}
		}

		if (update_window_callback) {
			update_window_callback();
		}
	});

	sync_room_event_handlers_set = true;
}

void sync_manager::create_sync_room(const std::string & name) {
	auto h = get_sio_client();
	if (!h) return;
	auto & socket = h->socket();

	socket->emit("create_room", { name });
	sync_room_joined = name;
}

void sync_manager::join_sync_room(const std::string & name) {
	auto h = get_sio_client();
	if (!h) return;
	auto & socket = h->socket();

	socket->emit("join_room", { name });
	sync_room_joined = name;
}

std::set<std::string>& sync_manager::get_sync_room_list() {
	return sync_room_list;
}

bool sync_manager::sync_room_is_joined(const std::string & name) {
	return sync_room_joined == name;
}

void sync_manager::register_update_window_callback(const std::function<void()> & fn) {
	update_window_callback = fn;
}

static initquit_factory_t<sync_manager> g_sync_manager;

sync_manager & sync_manager::get_instance() {
	sync_manager & inst = g_sync_manager.get_static_instance();
	console::printf("Static instance: %x", (intptr_t)&inst);
	return inst;
}

std::string sync_manager::sync_srv_url = "http://webdev.fit.cvut.cz:4200"; // TODO: make the URL configurable
std::string sync_manager::tracker_url = "udp://tracker.opentrackr.org:1337";
//const GUID sync_manager::class_guid = { 0x34afb0fb, 0x48ca, 0x4336,{ 0x8e, 0x09, 0x9e, 0x05, 0x2b, 0xe2, 0x4c, 0xefd } };
