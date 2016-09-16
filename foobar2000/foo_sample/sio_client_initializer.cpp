#include "stdafx.h"
#include "sio_client_initializer.h"

#include <future>

sio_client_initializer::sio_client_initializer(const std::string & url) {
	std::promise<bool> connected;
	auto connected_future = connected.get_future();

	h.set_open_listener([&connected]() {
		connected.set_value(true);
	});

	h.set_fail_listener([&connected]() {
		connected.set_value(false);
	});

	h.connect(url);
	is_connected = connected_future.get();
}

sio::client * sio_client_initializer::get_client() {
	if (is_connected) {
		return &h;
	}
	else {
		return NULL;
	}
}