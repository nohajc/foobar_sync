#include "stdafx.h"

#include "sync_playlist.h"
#include "sync_manager.h"
#include "sync_fs.h"
#include "string_helpers.h"

#include <string>
#include <utility>
#include <algorithm>
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
}

decltype(sync_playlist::hnd) & sync_playlist::get_handle() {
	return hnd;
}

decltype(sync_playlist::info) sync_playlist::get_info() {
	return info;
}

std::future<sync_playlist::piece_data> sync_playlist::request_piece(int piece_idx) {
	using namespace libtorrent;

	//auto data_future = read_request[piece_idx].get_shared_future();
	//auto kv = std::make_pair(piece_idx, std::promise<piece_data>());

	std::future<piece_data> data_future;
	
	{ // Synchronized access to read_request multimap
		std::lock_guard<std::mutex> guard(read_request_mutex);

		// Create promise at piece_idx
		auto it = read_request.emplace(std::piecewise_construct, std::forward_as_tuple(piece_idx), std::forward_as_tuple());
		data_future = it->second.get_future();
	}
	
	hnd.set_piece_deadline(piece_idx, 0, torrent_handle::alert_when_available);

	return data_future;
}

size_t sync_playlist::read_file(int file_idx, void * buf, t_filesize offset, t_size length, abort_callback & p_abort) {
	using namespace libtorrent; // TODO: handle abort_callback

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
		data_future.push_back(request_piece(i));
	}

	// Wait for them
	for (auto & f : data_future) {
		f.wait();
	}

	// Copy them to the output buffer
	char * output = (char*)buf;
	int bytes_read_total = 0;
	int bytes_left_to_copy = length;
	int buf_offset = preq.start;

	for (int i = 0; i < data_future.size(); ++i) {
		auto data = data_future[i].get();
		char * piece_buf = data.buffer.get() + buf_offset;
		int bytes_to_read = data.size - buf_offset;

		if (bytes_left_to_copy < bytes_to_read) {
			bytes_to_read = bytes_left_to_copy;
		}

		memcpy(output, piece_buf, bytes_to_read);

		output += bytes_to_read;
		bytes_read_total += bytes_to_read;
		bytes_left_to_copy -= bytes_to_read;

		buf_offset = 0;
	}

	return bytes_read_total;
}