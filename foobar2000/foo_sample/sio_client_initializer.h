#pragma once

#include "../socket.io/sio_client.h"

#include <future>
#include <functional>
#include <string>

class sio_client_initializer {
	sio::client h;
	bool is_connected;
	std::promise<bool> connected_promise;
	std::future<bool> connected_future;
	std::function<void()> open_listener, fail_listener;

	void really_connect(const std::string & url);
public:
	sio_client_initializer();
	void connect(const std::string & url);
	sio::client * get_client();
};