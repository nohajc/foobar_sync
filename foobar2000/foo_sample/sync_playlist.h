#pragma once

#include "stdafx.h"

#include <libtorrent/entry.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/alert.hpp>
#include <libtorrent/alert_types.hpp>

#include <vector>

#include "future_ext.h"


// Creates playlist from torrent
class sync_playlist {
	libtorrent::torrent_handle hnd;
	const libtorrent::torrent_info & info;
	std::vector<shared_promise<libtorrent::read_piece_alert>> read_request;
public:
	decltype(hnd) & get_handle();
	decltype(info) get_info();

	sync_playlist(const libtorrent::torrent_handle & h);

	size_t read_file(int file_idx, void * buf, t_filesize offset, t_size length, abort_callback & p_abort);
};