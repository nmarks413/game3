#include "Log.h"
#include "Position.h"
#include "entity/Player.h"
#include "game/Inventory.h"
#include "registry/Registries.h"
#include "tile/MineableTile.h"

#include <nlohmann/json.hpp>

namespace Game3 {
	MineableTile::MineableTile(Identifier identifier_, ItemStack stack_, bool consumable_):
		Tile(std::move(identifier_)), stack(std::move(stack_)), consumable(consumable_) {}

	bool MineableTile::interact(const Place &place, Layer layer) {
		PlayerPtr player = place.player;
		if (!player)
			return false;

		InventoryPtr inventory = player->getInventory();
		if (!inventory)
			return false;

		ItemStack *active = inventory->getActive();
		if (!active || !active->hasAttribute("base:attribute/pickaxe"))
			return false;

		player->give(stack);

		if (consumable && layer != Layer::Terrain) {
			// Terrain is never consumable with a regular pickaxe.
			place.set(layer, 0);
		}

		if (active->reduceDurability())
			inventory->erase(inventory->activeSlot);
		inventory->notifyOwner();

		return true;
	}

	void TileRegistry::addMineable(Identifier tilename, const ItemStack &stack, bool consumable) {
		add(tilename, std::make_unique<MineableTile>(tilename, stack, consumable));
	}

	void TileRegistry::addMineable(Identifier tilename, ItemStack &&stack, bool consumable) {
		add(tilename, std::make_unique<MineableTile>(tilename, std::move(stack), consumable));
	}
}
