#pragma once

#include "stdafx.h"

#include <libtorrent/entry.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/alert.hpp>
#include <libtorrent/alert_types.hpp>

#include <unordered_map>
#include <future>
#include <mutex>

// Creates playlist from torrent
class sync_playlist {
	libtorrent::torrent_handle hnd;
	const libtorrent::torrent_info & info;
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

	std::future<piece_data> sync_playlist::request_piece(int piece_idx);
	size_t read_file(int file_idx, void * buf, t_filesize offset, t_size length, abort_callback & p_abort);

	std::unordered_multimap<int, std::promise<piece_data>> read_request;
	std::mutex read_request_mutex;
};