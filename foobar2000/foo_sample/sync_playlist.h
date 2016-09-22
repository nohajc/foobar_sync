#pragma once

#include "stdafx.h"

#include "../socket.io/sio_client.h"

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
	std::string infohash_str;

	std::thread readahead_thread;
	sio::client & sio_client;

	void readahead();
public:
	class piece_data {
		std::shared_ptr<const std::string> buf_shptr;
	public:
		boost::shared_array<char> buffer;
		int piece;
		int size;

		piece_data() {}

		piece_data(const libtorrent::read_piece_alert & rpa) {
			buffer = rpa.buffer;
			piece = rpa.piece;
			size = rpa.size;
		}

		piece_data(int piece_idx, std::shared_ptr<const std::string> piece_data) {
			buf_shptr = piece_data; // We need to increment reference count,
			char * piece_data_ptr = const_cast<char*>(piece_data->data());
			// but also allow access through "buffer", so we pass an empty
			// deleter and let the "buf_shptr" take care of memory management.
			buffer = boost::shared_array<char>(piece_data_ptr, [](...){});
			piece = piece_idx;
			size = piece_data->size();
		}
	};

	decltype(hnd) & get_handle();
	decltype(info) get_info();

	sync_playlist(const libtorrent::torrent_handle & h, sio::client * cl, bool seed);

	enum piece_source {TORRENT_SOURCE, WEBSOCKET_SOURCE};
	std::future<piece_data> sync_playlist::request_piece(int piece_idx, int deadline, piece_source src = TORRENT_SOURCE);
	size_t read_file(int file_idx, void * buf, t_filesize offset, t_size length, abort_callback & p_abort);
	void piece_downloaded(int piece_idx, std::shared_ptr<const std::string> piece_data);
	void upload_piece(int piece_idx, const std::string & recipient_id);

	typedef std::packaged_task<piece_data(piece_data)> read_piece_task;
	std::unordered_multimap<int, read_piece_task> read_request;
	std::mutex read_request_mutex;

	std::vector<read_piece_task> pop_read_requests(int piece_idx);
private:
	std::vector<char> cached_data;
	std::vector<bool> piece_cached_bitmap;
	std::function<piece_data(piece_data)> cache_piece_callback;
	piece_data cache_piece(const piece_data & pd);
};