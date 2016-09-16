#include "stdafx.h"
#include "sio_client_initializer.h"

sio_client_initializer::sio_client_initializer(const char * url) {
	h.connect(url);
}

sio::client & sio_client_initializer::get_client() {
	return h;
}