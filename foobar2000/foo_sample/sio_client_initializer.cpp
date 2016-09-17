#include "stdafx.h"
#include "sio_client_initializer.h"

#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/URI.h>
#include <Poco/Net/NetException.h>

sio_client_initializer::sio_client_initializer() {
	is_connected = false;
}

void sio_client_initializer::really_connect(const std::string & url) {
	connected_future = connected_promise.get_future();

	h.set_reconnect_attempts(0);

	open_listener = [this]() {
		connected_promise.set_value(true);
	};
	h.set_open_listener(open_listener);

	fail_listener = [this]() {
		connected_promise.set_value(false);
	};
	h.set_fail_listener(fail_listener);

	h.connect(url);
	is_connected = connected_future.get();
}

void sio_client_initializer::connect(const std::string & url) {
	using namespace Poco;
	using namespace Net;

	// First, check whether the server is available using a simple HTTP request.
	// There is a bug in socket.io library, so we must make sure the subsequent
	// sio::client::connect() call does not fail.
	try {
		URI uri(url);
		HTTPClientSession cs(uri.getHost(), uri.getPort());
		std::string path(uri.getPathAndQuery());

		if (path.empty()) {
			path = "/";
		}

		HTTPRequest req(HTTPRequest::HTTP_HEAD, path, HTTPMessage::HTTP_1_1);
		HTTPResponse resp;

		cs.sendRequest(req);
		std::istream & resp_body = cs.receiveResponse(resp);

		if (resp.getStatus() == HTTPResponse::HTTP_OK) {
			really_connect(url);
			return;
		}
	}
	catch (SyntaxException e) {
		console::print(e.message().c_str());
	}
	catch (NetException e) {
		console::print(e.message().c_str());
	}

	is_connected = false;
}

sio::client * sio_client_initializer::get_client() {
	if (is_connected) {
		return &h;
	}
	else {
		return NULL;
	}
}