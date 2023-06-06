#pragma once

#include <set>
#include <vector>

#include "game/Game.h"
#include "packet/Packet.h"
#include "ui/Modifiers.h"

namespace Game3 {
	struct EntityRequest {
		GlobalID entityID;
		UpdateCounter threshold;
		EntityRequest(Entity &);
		EntityRequest(GlobalID entity_id, UpdateCounter threshold_): entityID(entity_id), threshold(threshold_) {}
	};

	struct EntityRequestPacket: Packet {
		static PacketID ID() { return 36; }

		RealmID realmID = -1;
		std::vector<EntityRequest> requests;

		EntityRequestPacket() = default;
		EntityRequestPacket(Realm &, const std::set<ChunkPosition> &);
		EntityRequestPacket(RealmID realm_id, std::vector<EntityRequest> requests_):
			realmID(realm_id), requests(std::move(requests_)) {}

		PacketID getID() const override { return ID(); }

		void encode(Game &, Buffer &buffer) const override;
		void decode(Game &, Buffer &buffer)       override;

		void handle(ServerGame &, RemoteClient &) override;
	};
}
