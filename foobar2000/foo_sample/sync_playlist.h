#pragma once

#include "stdafx.h"

#include <libtorrent/entry.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/session.hpp>

// Must not create static instance, we store instances in sync_fs_impl::pl map as unique_ptrs.
class sync_playlist {
	libtorrent::torrent_handle hnd;
	const libtorrent::torrent_info & info;
public:
	sync_playlist(const libtorrent::torrent_handle & h);
};