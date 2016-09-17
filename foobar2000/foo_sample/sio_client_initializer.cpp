#include "stdafx.h"
#include "sio_client_initializer.h"

void sio_client_initializer::cleanup() {
	if (is_connected) {
		h.sync_close();
	}
}

void sio_client_initializer::connect(const std::string & url) {
	connected_future = connected_promise.get_future();

	h.set_reconnect_attempts(0);

	open_listener = [this]() {
		connected_promise.set_value(true);
	};
	h.set_open_listener(open_listener);

	/*h.set_reconnect_listener([](unsigned a, unsigned b) {
		console::print("Reconnect.");
	});*/

	fail_listener = [this]() {
		connected_promise.set_value(false);
	};
	h.set_fail_listener(fail_listener);

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