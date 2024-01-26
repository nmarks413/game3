#include "Log.h"
#include "game/ClientGame.h"
#include "game/Village.h"
#include "packet/VillageUpdatePacket.h"
#include "ui/MainWindow.h"

namespace Game3 {
	VillageUpdatePacket::VillageUpdatePacket(const Village &village):
		VillageUpdatePacket(village.getID(), village.getLabor(), village.getResources()) {}

	void VillageUpdatePacket::handle(ClientGame &) {
		INFO("Resources for village " << villageID << " (labor = " << labor << "):");
		for (const auto &[key, value]: resources)
			INFO("    " << key << " => " << value);
	}
}
