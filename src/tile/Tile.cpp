#include "entity/EntityFactory.h"
#include "entity/Monster.h"
#include "entity/Player.h"
#include "game/Game.h"
#include "realm/Realm.h"
#include "threading/ThreadContext.h"
#include "tile/Tile.h"
#include "util/Util.h"

namespace Game3 {
	Tile::Tile(Identifier identifier_):
		NamedRegisterable(std::move(identifier_)) {}

	void Tile::randomTick(const Place &place) {
		if (!canSpawnMonsters(place))
			return;

		Game &game = place.getGame();

		makeMonsterFactories(game);

		EntityPtr monster;
		{
			auto factories_lock = monsterFactories.sharedLock();
			monster = (*choose(*monsterFactories))(game);
		}

		place.realm->spawn(monster, place.position);
	}

	bool Tile::interact(const Place &, Layer, ItemStack *, Hand) {
		return false;
	}

	bool Tile::canSpawnMonsters(const Place &place) const {
		RealmPtr realm = place.realm;

		// Don't spawn indoors.
		if (!realm->outdoors)
			return false;

		// Don't spawn on fluids.
		if (realm->hasFluid(place.position))
			return false;

		// Don't spawn on a tile that can't be walked on.
		if (!realm->isPathable(place.position))
			return false;

		// Don't spawn too close to players or other monsters.
		const bool any_in_range = realm->hasEntitiesSquare(place.position, 16, [](const EntityPtr &entity) {
			return entity->isPlayer() || std::dynamic_pointer_cast<Monster>(entity);
		});

		if (any_in_range)
			return false;

		const double hour = place.getGame().getHour();
		if (5. <= hour && hour < 21.)
			return false;

		std::uniform_real_distribution<float> distribution(0, 1);
		return distribution(threadContext.rng) < getMonsterSpawnProbability();
	}

	float Tile::getMonsterSpawnProbability() const {
		return 0.1;
	}

	void Tile::makeMonsterFactories(Game &game) {
		{
			auto shared = monsterFactories.sharedLock();
			if (monsterFactories)
				return;
		}

		auto unique = monsterFactories.uniqueLock();
		if (monsterFactories) // Just in case it was somehow made between unlocking the shared lock and acquiring the unique lock.
			return;

		auto &registry = game.registry<EntityFactoryRegistry>();
		monsterFactories.emplace();

		for (const auto &[identifier, factory]: registry) {
			EntityPtr entity = (*factory)(game);
			entity->spawning = true;
			if (entity->isSpawnableMonster())
				monsterFactories->emplace_back(factory);
		}
	}
}
