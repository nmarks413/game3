#pragma once

#include "net/SendBuffer.h"

#include <cstddef>
#include <string>

#include <asio.hpp>
#include <asio/ssl.hpp>

namespace Game3 {
	class Packet;
	class Server;

	struct GenericClient {
		Server &server;
		int id = -1;
		std::string ip;
		SendBuffer sendBuffer;
		std::mutex networkMutex;
		asio::ip::tcp::socket socket;

		asio::ssl::stream<asio::ip::tcp::socket &> sslStream;

		GenericClient() = delete;
		GenericClient(const GenericClient &) = delete;
		GenericClient(GenericClient &&) = delete;
		GenericClient(Server &server_, std::string_view ip_, int id_);

		virtual ~GenericClient() = default;

		GenericClient & operator=(const GenericClient &) = delete;
		GenericClient & operator=(GenericClient &&) = delete;

		void init();

		virtual void handleInput(std::string_view) = 0;
		virtual void onMaxLineSizeExceeded() {}
	};
}
