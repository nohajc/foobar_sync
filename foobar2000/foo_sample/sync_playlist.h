#pragma once

#include "stdafx.h"

#include <libtorrent/entry.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/session.hpp>

// Creates playlist from torrent
class sync_playlist {
	libtorrent::torrent_handle hnd;
	const libtorrent::torrent_info & info;
public:
	sync_playlist(const libtorrent::torrent_handle & h);
};