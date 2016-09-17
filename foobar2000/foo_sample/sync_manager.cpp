#include "stdafx.h"

#include "string_helpers.h"
#include "sync_manager.h"
#include "sync_playlist.h"

#include <libtorrent/entry.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/time.hpp>
#include <libtorrent/alert.hpp>
#include <libtorrent/alert_types.hpp>

#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/URI.h>
#include <Poco/StreamCopier.h>
#include <Poco/Net/NetException.h>

#include <boost/filesystem/operations.hpp>

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

	// TESTING SOCKET.IO - TODO: add menu commands to trigger specific actions
	/*sio::client * h = get_sio_client();
	if (!h) {
		return;
	}*/

	setup_sync_room_event_handlers();
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
}

sio::client * sync_manager::get_sio_client() {
	auto cl = sio_ini->get_client();
	if (cl == nullptr) {
		sio_ini->connect(SYNC_SRV_URL);
		cl = sio_ini->get_client();
	}
	return cl;
}

void sync_manager::setup_sync_room_event_handlers() {
	auto h = get_sio_client();
	if (!h) {
		return;
	}
	auto & socket = h->socket();

	socket->on("room_added", [this](sio::event & e) {
		auto & msg = e.get_message();
		assert(msg->flag_string);
		console::print(msg->get_string().c_str());

		{
			std::lock_guard<std::mutex> guard(sync_room_list_mutex);
			sync_room_list.insert(msg->get_string());
		} // TODO: notify sync_room_window
	});
}

void sync_manager::create_sync_room(const std::string & name) {
	auto h = get_sio_client();
	auto & socket = h->socket();

	socket->emit("create_room", { name });
	sync_room_joined = name;
}

std::set<std::string>& sync_manager::get_sync_room_list() {
	return sync_room_list;
}

bool sync_manager::sync_room_is_joined(const std::string & name) {
	return sync_room_joined == name;
}

static initquit_factory_t<sync_manager> g_sync_manager;

sync_manager & sync_manager::get_instance() {
	sync_manager & inst = g_sync_manager.get_static_instance();
	console::printf("Static instance: %x", (intptr_t)&inst);
	return inst;
}

const std::string sync_manager::SYNC_SRV_URL = "http://webdev.fit.cvut.cz:4200"; // TODO: make the URL configurable
//const GUID sync_manager::class_guid = { 0x34afb0fb, 0x48ca, 0x4336,{ 0x8e, 0x09, 0x9e, 0x05, 0x2b, 0xe2, 0x4c, 0xefd } };
