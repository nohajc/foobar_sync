#include "stdafx.h"

#include "sync_playlist.h"
#include "sync_manager.h"
#include "sync_fs.h"

sync_playlist::sync_playlist(const libtorrent::torrent_handle & h) : hnd(h), info(h.get_torrent_info()) {
	sync_manager & sm = sync_manager::get_instance();
	auto & pl = sm.get_playlist_map();

	pl[info.info_hash()] = std::unique_ptr<sync_playlist>(this);
}
