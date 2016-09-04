#include "stdafx.h"

#include "sync_manager.h"

#include <libtorrent/entry.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/session.hpp>

#include <memory>
#include <thread>
#include <unordered_map>

void sync_manager::on_init() {
	console::print("Starting libtorrent session...");
	torrent_session = std::make_unique<libtorrent::session>();
	console::printf("Sync manager initialized, addr %x.", (intptr_t)this);

	// TODO: Initialize torrent client - could it be done on demand only?
	using namespace libtorrent;

	session & s = *torrent_session;
	error_code ec;
	
	s.listen_on(std::make_pair(6881, 6889), ec);
	if (ec) {
		console::print(ec.message().c_str());
		return;
	}

	s.start_upnp();
	s.start_natpmp();

	alert_handler_thread = std::thread(&sync_manager::alert_handler, this);
}

void sync_manager::on_quit() {
}

libtorrent::session & sync_manager::get_torrent_session() {
	return *torrent_session;
}

decltype(sync_manager::pl) & sync_manager::get_playlist_map() {
	return pl;
}

void sync_manager::alert_handler() {
	using namespace libtorrent;

	while (true) {
		while (!torrent_session->wait_for_alert(time_duration(10000000))) {
			console::print("Waiting for event...");
		}
		auto a = torrent_session->pop_alert();
	}
}

static initquit_factory_t<sync_manager> g_sync_manager;

sync_manager & sync_manager::get_instance() {
	sync_manager & inst = g_sync_manager.get_static_instance();
	console::printf("Static instance: %x", (intptr_t)&inst);
	return inst;
}

//const GUID sync_manager::class_guid = { 0x34afb0fb, 0x48ca, 0x4336,{ 0x8e, 0x09, 0x9e, 0x05, 0x2b, 0xe2, 0x4c, 0xefd } };
