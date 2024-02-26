#include "Log.h"
#include "entity/ClientPlayer.h"
#include "entity/ServerPlayer.h"
#include "game/ClientGame.h"
#include "game/Inventory.h"
#include "game/ServerGame.h"
#include "net/RemoteClient.h"
#include "packet/SetActiveSlotPacket.h"
#include "ui/MainWindow.h"
#include "ui/tab/InventoryTab.h"

namespace Game3 {
	void SetActiveSlotPacket::handle(const std::shared_ptr<ServerGame> &, RemoteClient &client) {
		if (const PlayerPtr player = client.getPlayer())
			if (const InventoryPtr inventory = player->getInventory(0))
				inventory->setActive(slot);
	}

	void SetActiveSlotPacket::handle(const ClientGamePtr &game) {
		game->getPlayer()->getInventory(0)->setActive(slot, true);
		game->getWindow().inventoryTab->activeSlotSet();
	}
}
