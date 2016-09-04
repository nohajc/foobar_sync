#include "stdafx.h"

#include "sync_playlist.h"
#include "sync_manager.h"
#include "sync_fs.h"

sync_playlist::sync_playlist(const libtorrent::torrent_handle & h) : hnd(h), info(h.get_torrent_info()) {
	console::print("Let's create playlist.");
}
