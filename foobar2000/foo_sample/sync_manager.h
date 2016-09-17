#pragma once

#include "stdafx.h"
#include "sync_playlist.h"
#include "../socket.io/sio_client.h"
#include "sio_client_initializer.h"

#include <libtorrent/entry.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/session.hpp>

#include <memory>
#include <thread>
#include <unordered_map>
#include <vector>
#include <set>
#include <functional>
#include <string>
#include <atomic>
#include <mutex>

class sync_manager : public initquit {
	std::unique_ptr<libtorrent::session> torrent_session;
	std::thread alert_handler_thread;
	std::atomic<bool> app_running;
	// TODO: We want this to be unordered map - template specialization for hash<sha1_hash> has to be written
	std::map<libtorrent::sha1_hash, std::unique_ptr<sync_playlist>> pl;

	std::unique_ptr<sio_client_initializer> sio_ini;
	static const std::string SYNC_SRV_URL;

	std::set<std::string> sync_room_list;
	std::mutex sync_room_list_mutex;
	std::string sync_room_joined;
public:
	virtual void on_init();

	virtual void on_quit();

	libtorrent::session & get_torrent_session();

	decltype(pl) & get_playlist_map();

	void alert_handler();

	void add_torrent_from_url(const char * url);
	void add_torrent_from_data(const char * data, std::streamsize size);
	void add_torrent(libtorrent::torrent_info * ti);

	sio::client * get_sio_client();

	void setup_sync_room_event_handlers();
	void create_sync_room(const std::string & name);
	std::set<std::string> & get_sync_room_list();
	bool sync_room_is_joined(const std::string & name);

	static sync_manager & get_instance();
};