#pragma once

#include "game/ChunkPosition.h"
#include "game/Game.h"
#include "net/Buffer.h"
#include "packet/Packet.h"

namespace Game3 {
	struct ChunkTilesPacket: Packet {
		static PacketID ID() { return 8; }

		RealmID realmID;
		ChunkPosition chunkPosition;
		std::vector<uint16_t> tiles;

		ChunkTilesPacket() = default;
		ChunkTilesPacket(RealmID realm_id, ChunkPosition chunk_position, std::vector<uint16_t> tiles_):
			realmID(realm_id), chunkPosition(chunk_position), tiles(std::move(tiles_)) {}

		PacketID getID() const override { return ID(); }

		void encode(Game &, Buffer &buffer) const override { buffer << realmID << chunkPosition << tiles; }
		void decode(Game &, Buffer &buffer)       override { buffer >> realmID >> chunkPosition >> tiles; }

		void handle(ClientGame &) override;
	};
}
