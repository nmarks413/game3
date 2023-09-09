#include "Position.h"
#include "entity/Player.h"
#include "game/Crop.h"
#include "game/Inventory.h"
#include "threading/ThreadContext.h"
#include "tile/TreeTile.h"

namespace Game3 {
	TreeTile::TreeTile(std::shared_ptr<Crop> crop_):
		CropTile(ID(), std::move(crop_)) {}

	bool TreeTile::interact(const Place &place, Layer layer) {
		assert(!crop->stages.empty());

		PlayerPtr player = place.player;
		const InventoryPtr inventory = player->getInventory();
		Game &game = player->getGame();

		if (ItemStack *active_stack = inventory->getActive()) {
			if (active_stack->hasAttribute("base:attribute/axe") && !inventory->add(crop->product)) {
				// Remove tree
				place.set(layer, 0);

				// Handle axe durability
				if (active_stack->reduceDurability())
					inventory->erase(inventory->activeSlot);

				// Give saplings if possible
				if (auto sapling = crop->customData.find("sapling"); sapling != crop->customData.end()) {
					ItemCount saplings = 1;
					std::uniform_int_distribution distribution(1, 4);
					while (distribution(threadContext.rng) == 1)
						++saplings;
					player->give({game, sapling->get<Identifier>(), saplings});
				}

				return true;
			}
		}

		if (auto honey = crop->customData.find("honey"); honey != crop->customData.end()) {
			if (auto tilename = place.getName(layer); tilename && tilename->get() == honey->at("full")) {
				if (!inventory->add({game, honey->at("item").get<Identifier>()})) {
					place.set(layer, honey->at("empty").get<Identifier>());
					return true;
				}
			}
		}

		return false;
	}
}
