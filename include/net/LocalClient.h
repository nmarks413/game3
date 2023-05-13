#pragma once

#include <iostream>
#include <memory>

#include "net/Buffer.h"
#include "net/Sock.h"
#include "util/Math.h"

namespace Game3 {
	class ClientGame;
	class Packet;

	class LocalClient: public std::enable_shared_from_this<LocalClient> {
		public:
			constexpr static size_t MAX_PACKET_SIZE = 1 << 24;

			std::shared_ptr<ClientGame> game;

			LocalClient() = default;

			void connect(std::string_view hostname, uint16_t port);
			void read();
			void send(const Packet &);
			bool isConnected() const;

		private:
			enum class State {Begin, Data};
			State state = State::Begin;
			Buffer buffer;
			uint16_t packetType = 0;
			uint32_t payloadSize = 0;
			std::shared_ptr<Sock> sock;
			std::vector<uint8_t> headerBytes;

			template <std::integral T>
			void sendRaw(T value) {
				T little = toLittle(value);
				sock->send(&little, sizeof(little));
			}
	};
}
