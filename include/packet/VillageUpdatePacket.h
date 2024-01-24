#pragma once

#include "net/Buffer.h"
#include "packet/Packet.h"

namespace Game3 {
	class LivingEntity;
	class Village;

	struct VillageUpdatePacket: Packet {
		static PacketID ID() { return 57; }

		VillageID villageID;
		Resources resources;

		VillageUpdatePacket() = default;
		explicit VillageUpdatePacket(const Village &);
		VillageUpdatePacket(VillageID village_id, Resources resources_):
			villageID(village_id), resources(std::move(resources_)) {}

		PacketID getID() const override { return ID(); }

		void encode(Game &, Buffer &buffer) const override { buffer << villageID << resources; }
		void decode(Game &, Buffer &buffer)       override { buffer >> villageID >> resources; }

		void handle(ClientGame &) override;
	};
}
