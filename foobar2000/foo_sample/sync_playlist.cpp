#include "stdafx.h"

#include "sync_playlist.h"
#include "sync_manager.h"
#include "sync_fs.h"
#include "string_helpers.h"

#include <string>
#include <utility>
#include <algorithm>
#include <chrono>

#include <boost/filesystem/operations.hpp>

#include <cstring>

sync_playlist::sync_playlist(const libtorrent::torrent_handle & h) : hnd(h), info(h.get_torrent_info()) {
	using namespace libtorrent;
	using namespace boost::filesystem;
	console::print("Let's create playlist.");

	int num_files = info.num_files();

	auto mdb = static_api_ptr_t<metadb>();
	auto pl_manager = static_api_ptr_t<playlist_manager>();

	pfc::list_t<metadb_handle_ptr> mh_list;

	for (int i = 0; i < num_files; ++i) {
		const file_entry & e = info.file_at(i);
		if (e.pad_file) continue;

		path fp = e.path;

		// Get textual representation of the infohash
		std::string hash_binstr = info.info_hash().to_string();
		std::string hash_str = sha1ToHexString(hash_binstr);

		fp = hash_str / std::to_string(i) / fp;

		// Create full path with "sync://" prefix
		std::string path_with_prefix = sync_fs::prefix + fp.generic_string();

		// Add playable location meta handle to list
		metadb_handle_ptr mh = mdb->handle_create(path_with_prefix.c_str(), 0);
		mh_list.add_item(mh);

		console::printf("Path: %s", fp.generic_string().c_str());
	}

	/*const char pl_name[] = "Torrent playlist";
	t_size pl_idx = pl_manager->create_playlist(pl_name, sizeof(pl_name), pfc_infinite);
	pl_manager->playlist_add_items_filter(pl_idx, mh_list, true);
	pl_manager->set_active_playlist(pl_idx);*/
	pl_manager->activeplaylist_clear();
	pl_manager->activeplaylist_add_items_filter(mh_list, false);

	// Prepare cache_piece callback object
	cache_piece_callback = std::bind(&sync_playlist::cache_piece, this, std::placeholders::_1);

	// Allocate memory for cache
	cached_data.resize(info.total_size());
	// Init cache bitmap
	piece_cached_bitmap.resize(info.num_pieces(), false);

	// Start readahead thread
	readahead_thread = std::thread(&sync_playlist::readahead, this);
}

decltype(sync_playlist::hnd) & sync_playlist::get_handle() {
	return hnd;
}

decltype(sync_playlist::info) sync_playlist::get_info() {
	return info;
}

std::future<sync_playlist::piece_data> sync_playlist::request_piece(int piece_idx, int deadline) {
	using namespace libtorrent;

	//auto data_future = read_request[piece_idx].get_shared_future();
	//auto kv = std::make_pair(piece_idx, std::promise<piece_data>());

	std::future<piece_data> data_future;
	
	{ // Synchronized access to read_request multimap
		std::lock_guard<std::mutex> guard(read_request_mutex);

		// Create promise at piece_idx
		//auto it = read_request.emplace(std::piecewise_construct, std::forward_as_tuple(piece_idx), std::forward_as_tuple());
		auto it = read_request.emplace(std::make_pair(piece_idx, cache_piece_callback));
		data_future = it->second.get_future();
	}
	
	hnd.set_piece_deadline(piece_idx, deadline, torrent_handle::alert_when_available);

	return data_future;
}

size_t sync_playlist::read_file(int file_idx, void * buf, t_filesize offset, t_size length, abort_callback & p_abort) {
	using namespace libtorrent;

	console::printf("Requested read: idx = %d, off = %u", file_idx, offset);
	console::printf("Piece size = %d", info.piece_length());
	peer_request preq = info.map_file(file_idx, offset, length);

	int first_piece_to_read = preq.piece;
	int total_bytes_in_pieces = preq.start + preq.length;
	int num_of_pieces_to_read = total_bytes_in_pieces / info.piece_length();
	// Plus one if there are any leftovers
	num_of_pieces_to_read += !!(total_bytes_in_pieces % info.piece_length());

	console::printf("Which translates to: piece = %d, start = %d, length = %d", preq.piece, preq.start, preq.length);
	console::printf("Number of pieces to read: %d", num_of_pieces_to_read);

	std::vector<std::future<piece_data>> data_future;
	// Request all the pieces
	for (int i = first_piece_to_read; i < first_piece_to_read + num_of_pieces_to_read; ++i) {
		if (!piece_cached_bitmap[i]) {
			data_future.push_back(request_piece(i, 0));
		}
		else { // Cached piece
			data_future.emplace_back(); // Insert empty future (valid() == false)
		}
	}

	// Wait for them
	for (auto & f : data_future) {
		if (f.valid()) {
			std::future_status st;
			do {
				p_abort.check();
				st = f.wait_for(std::chrono::milliseconds(100));
			} while (st == std::future_status::timeout);
		}
	}

	// Copy them to the output buffer
	char * output = (char*)buf;
	int bytes_read_total = 0;
	int bytes_left_to_copy = length;
	int buf_offset = preq.start;
	int read_offset = first_piece_to_read * info.piece_length() + buf_offset;

	for (int i = 0; i < data_future.size(); ++i) {
		auto & f = data_future[i];
		int bytes_to_read;
		char * piece_buf;

		if (f.valid()) { // Read from the returned piece_data
			auto data = f.get();
			piece_buf = data.buffer.get() + buf_offset;
			bytes_to_read = data.size - buf_offset;
			console::print(">>> READING FROM PIECE_DATA");
		}
		else { // Read from cache
			piece_buf = &cached_data[read_offset];
			bytes_to_read = info.piece_size(first_piece_to_read + i) - buf_offset;
			console::print(">>> READING FROM CACHE");
		}

		if (bytes_left_to_copy < bytes_to_read) {
			bytes_to_read = bytes_left_to_copy;
		}

		memcpy(output, piece_buf, bytes_to_read);

		read_offset += bytes_to_read;
		output += bytes_to_read;
		bytes_read_total += bytes_to_read;
		bytes_left_to_copy -= bytes_to_read;

		buf_offset = 0;
	}

	return bytes_read_total;
}

sync_playlist::piece_data sync_playlist::cache_piece(const piece_data & pd) {
	console::print("| CACHE PIECE |");

	int piece_idx = pd.piece;

	if (!piece_cached_bitmap[piece_idx]) {
		char * buf = pd.buffer.get();
		int piece_offset = piece_idx * info.piece_length();

		memcpy(&cached_data[piece_offset], buf, pd.size);
		piece_cached_bitmap[piece_idx] = true;
	}

	return pd;
}

void sync_playlist::readahead() {
	console::print("| READAHEAD THREAD STARTED |");

	int num_pieces = info.num_pieces();
	std::vector<std::future<piece_data>> data_future;

	// Request all the pieces

	for (int i = 0, deadline = 0; i < num_pieces; ++i, deadline += 500) {
		data_future.push_back(request_piece(i, deadline));
	}

	for (auto & f : data_future) {
		// We don't need the data here but we should also
		// destroy the reference to them (shared_array)
		auto data = f.get();
	}

	console::print("| READAHEAD THREAD FINISHED |");
}