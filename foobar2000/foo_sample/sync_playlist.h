#pragma once

#include "stdafx.h"

#include <libtorrent/entry.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/alert.hpp>
#include <libtorrent/alert_types.hpp>

#include <vector>
#include <unordered_map>
#include <future>
#include <mutex>
#include <thread>

// Creates playlist from torrent
class sync_playlist {
	libtorrent::torrent_handle hnd;
	const libtorrent::torrent_info & info;
	std::thread readahead_thread;

	void readahead();
public:
	struct piece_data {
		boost::shared_array<char> buffer;
		int piece;
		int size;

		piece_data() {}

		piece_data(const libtorrent::read_piece_alert & rpa) {
			buffer = rpa.buffer;
			piece = rpa.piece;
			size = rpa.size;
		}
	};

	decltype(hnd) & get_handle();
	decltype(info) get_info();

	sync_playlist(const libtorrent::torrent_handle & h);

	enum piece_source {TORRENT_SOURCE, WEBSOCKET_SOURCE};
	std::future<piece_data> sync_playlist::request_piece(int piece_idx, int deadline, piece_source src = TORRENT_SOURCE);
	size_t read_file(int file_idx, void * buf, t_filesize offset, t_size length, abort_callback & p_abort);

	typedef std::packaged_task<piece_data(piece_data)> read_piece_task;
	std::unordered_multimap<int, read_piece_task> read_request;
	std::mutex read_request_mutex;

private:
	std::vector<char> cached_data;
	std::vector<bool> piece_cached_bitmap;
	std::function<piece_data(piece_data)> cache_piece_callback;
	piece_data cache_piece(const piece_data & pd);
};