#pragma once

#include "stdafx.h"
#include "sync_playlist.h"
#include "../socket.io/sio_client.h"

#include <libtorrent/entry.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/session.hpp>

#include <memory>
#include <thread>
#include <unordered_map>
#include <vector>
#include <functional>

class sync_manager : public initquit {
	std::unique_ptr<libtorrent::session> torrent_session;
	std::thread alert_handler_thread;
	// TODO: We want this to be unordered map - template specialization for hash<sha1_hash> has to be written
	std::map<libtorrent::sha1_hash, std::unique_ptr<sync_playlist>> pl;

	static const char SYNC_SRV_URL[];
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

	static sync_manager & get_instance();
};