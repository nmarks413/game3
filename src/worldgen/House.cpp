#include "Tileset.h"
#include "entity/Miner.h"
#include "game/Game.h"
#include "realm/Realm.h"
#include "tileentity/Building.h"
#include "tileentity/Chest.h"
#include "tileentity/Sign.h"
#include "tileentity/Teleporter.h"
#include "util/Timer.h"
#include "util/Util.h"
#include "worldgen/Carpet.h"
#include "worldgen/House.h"
#include "worldgen/Indoors.h"

namespace Game3::WorldGen {
	void generateHouse(const std::shared_ptr<Realm> &realm, std::default_random_engine &rng, const std::shared_ptr<Realm> &parent_realm, Index width, Index height, const Position &entrance) {
		realm->markGenerated(0, 0);
		Timer timer("GenerateHouse");

		realm->tileProvider.ensureAllChunks(ChunkPosition{0, 0});
		auto pauser = realm->pauseUpdates();
		generateIndoors(realm, rng, parent_realm, width, height, entrance);

		const auto &tileset = realm->getTileset();
		const auto &plants = tileset.getTilesByCategory("base:category/plants"_id);

		realm->setTile(Layer::Submerged, {1, 1}, choose(plants, rng), false, true);
		realm->setTile(Layer::Submerged, {1, width - 2}, choose(plants, rng), false, true);
		realm->setTile(Layer::Submerged, {height - 2, 1}, choose(plants, rng), false, true);
		realm->setTile(Layer::Submerged, {height - 2, width - 2}, choose(plants, rng), false, true);

		const auto &beds = tileset.getTilesByCategory("base:category/beds"_id);
		std::array<Index, 2> edges {1, width - 2};
		const Position bed_position(2 + rng() % (height - 4), choose(edges, rng));
		realm->setTile(Layer::Objects, bed_position, choose(beds, rng), false, true);
		realm->extraData["bed"] = bed_position;

		// const auto house_position = entrance - Position(1, 0);
		// realm->spawn<Miner>({exit_position.row - 1, exit_position.column}, parent_realm->id, realm->id, house_position, parent_realm->closestTileEntity<Building>(house_position,
		// 	[](const auto &building) { return building->tileID == "base:tile/keep_sw"_id; }));

		Game &game = realm->getGame();

		// switch(rng() % 2) {
		switch(1) {
			case 0: {
				std::array<const char *, 13> texts {
					"Express ideas directly in code.",
					"Write in ISO Standard C++.",
					"Express intent.",
					"Ideally, a program should be statically type safe.",
					"Prefer compile-time checking to run-time checking.",
					"What cannot be checked at compile time should be checkable at run time.",
					"Catch run-time errors early.",
					"Don't leak any resources.",
					"Don't waste time or space.",
					"Prefer immutable data to mutable data.",
					"Encapsulate messy constructs, rather than spreading through the code.",
					"Use supporting tools as appropriate.",
					"Use support libraries as appropriate.",
				};

				std::shuffle(texts.begin(), texts.end(), rng);

				for (Index column = 2; column < width - 2; ++column) {
					realm->setTile(Layer::Objects, {1, column}, "base:tile/bookshelf"_id, false, true);
					realm->add(TileEntity::create<Sign>(game, "base:tile/empty"_id, Position(1, column), texts.at((column - 2) % texts.size()), "Bookshelf"));
				}
				break;
			}

			case 1: {
				auto chest = TileEntity::create<Chest>(game, "base:tile/empty"_id, Position(1, width / 2), "Chest");
				chest->setInventory(10);
				realm->add(chest);
				auto chest2 = TileEntity::create<Chest>(game, "base:tile/empty"_id, Position(4, width / 2), "Chest");
				chest2->setInventory(10);
				realm->add(chest2);
				auto chest3 = TileEntity::create<Chest>(game, "base:tile/empty"_id, Position(4, width / 2 - 2), "Chest");
				chest3->setInventory(10);
				realm->add(chest3);
				break;
			}

			default:
				break;
		}

		WorldGen::generateCarpet(realm, rng, width, height);
	}
}
