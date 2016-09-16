#pragma once

#include "../socket.io/sio_client.h"

class sio_client_initializer {
	sio::client h;

public:
	sio_client_initializer(const char * url);
	sio::client & get_client();
};