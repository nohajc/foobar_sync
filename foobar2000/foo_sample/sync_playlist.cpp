#include "stdafx.h"

#include "sync_playlist.h"
#include "sync_manager.h"
#include "sync_fs.h"
#include "string_helpers.h"

#include <string>
#include <boost/filesystem/operations.hpp>

sync_playlist::sync_playlist(const libtorrent::torrent_handle & h) : hnd(h), info(h.get_torrent_info()), read_request(info.num_pieces()) {
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

std::shared_future<sync_playlist::piece_data> sync_playlist::request_piece(int piece_idx) {
	using namespace libtorrent;

	auto data_future = read_request[piece_idx].get_shared_future();
	hnd.set_piece_deadline(piece_idx, 0, torrent_handle::alert_when_available);

	return data_future;
}

size_t sync_playlist::read_file(int file_idx, void * buf, t_filesize offset, t_size length, abort_callback & p_abort) {
	console::print("TEST: Requesting first torrent piece...");
	auto data_future = request_piece(0);
	auto data = data_future.get();
	console::print("TEST: First torrent piece obtained!");

	return 0;
}