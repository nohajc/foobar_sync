#include "stdafx.h"

#include "sync_playlist.h"
#include "sync_fs.h"

sync_playlist::sync_playlist(const libtorrent::torrent_handle & h) : hnd(h), info(h.get_torrent_info()) {
	/*assert(static_api_ptr_t<sync_fs>().get_ptr() != NULL);
	console::printf("Ptr: %p", static_api_ptr_t<sync_fs>().get_ptr());

	auto & pl = static_api_ptr_t<sync_fs>()->get_playlist_map();

	pl[info.info_hash()] = std::unique_ptr<sync_playlist>(this);*/
}