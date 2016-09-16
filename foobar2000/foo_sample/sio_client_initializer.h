#pragma once

#include "../socket.io/sio_client.h"

#include <functional>

class sio_client_initializer {
	sio::client h;
	bool is_connected;

public:
	sio_client_initializer(const char * url);
	sio::client * get_client();
};