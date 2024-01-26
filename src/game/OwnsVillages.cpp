#include "game/OwnsVillages.h"
#include "game/Game.h"
#include "realm/Realm.h"

#include <nlohmann/json.hpp>
#include <SQLiteCpp/SQLiteCpp.h>

#include <optional>

namespace Game3 {
	VillageID OwnsVillages::getNewVillageID() {
		return ++lastVillageID;
	}

	VillagePtr OwnsVillages::getVillage(VillageID id) const {
		return villageMap.at(id);
	}

	VillagePtr OwnsVillages::addVillage(Game &game, ChunkPosition chunk_position, const Place &place, const VillageOptions &options) {
		auto lock = villageMap.uniqueLock();
		const auto new_id = getNewVillageID();
		VillagePtr new_village = std::make_shared<Village>(game, new_id, place.realm->getID(), chunk_position, place.position, options);
		villageMap[new_id] = new_village;
		associateWithRealm(new_village, place.realm->getID());
		return new_village;
	}

	VillagePtr OwnsVillages::addVillage(Game &game, VillageID village_id, std::string name, RealmID realm_id, ChunkPosition chunk_position, const Position &position, Resources resources) {
		VillagePtr new_village = std::make_shared<Village>(village_id, realm_id, std::move(name), chunk_position, position, VillageOptions{}, Richness{}, std::move(resources), LaborAmount{}, double{});

		villageMap[village_id] = new_village;
		lastVillageID = std::max(lastVillageID.load(), village_id);

		new_village->setGame(game.shared_from_this());
		associateWithRealm(new_village, realm_id);
		return new_village;
	}

	void OwnsVillages::saveVillages(SQLite::Database &database, bool use_transaction) {
		std::optional<SQLite::Transaction> transaction;
		if (use_transaction)
			transaction.emplace(database);

		SQLite::Statement statement{database, "INSERT OR REPLACE INTO villages VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"};

		auto lock = villageMap.sharedLock();
		for (const auto &[id, village]: villageMap) {
			statement.bind(1, int64_t(village->getID()));
			statement.bind(2, village->getRealmID());
			statement.bind(3, std::string(village->getChunkPosition()));
			statement.bind(4, std::string(village->getPosition()));
			statement.bind(5, nlohmann::json(village->options).dump());
			statement.bind(6, nlohmann::json(village->getRichness()).dump());
			statement.bind(7, nlohmann::json(village->getResources()).dump());
			statement.bind(8, village->getName());
			statement.bind(9, int64_t(village->getLabor()));
			statement.bind(10, int(village->getRandomValue() * 1e9));
			statement.exec();
			statement.reset();
		}
		lock.unlock();

		if (transaction)
			transaction->commit();
	}

	void OwnsVillages::loadVillages(const std::shared_ptr<Game> &game, SQLite::Database &database) {
		SQLite::Statement query{database, "SELECT * FROM villages"};

		auto lock = villageMap.uniqueLock();
		villageMap.clear();
		lastVillageID = 0;

		while (query.executeStep()) {
			size_t id(query.getColumn(0).getInt64());
			RealmID realm_id(query.getColumn(1).getInt());
			ChunkPosition chunk_position(query.getColumn(2).getString());
			Position position(query.getColumn(3).getString());
			VillageOptions options(nlohmann::json::parse(query.getColumn(4).getString()));
			Richness richness(nlohmann::json::parse(query.getColumn(5).getString()));
			Resources resources(nlohmann::json::parse(query.getColumn(6).getString()));
			std::string name(query.getColumn(7));
			LaborAmount labor(query.getColumn(8).getInt64());
			double randomValue(query.getColumn(9).getInt() / 1e9);

			auto village = std::make_shared<Village>(id, realm_id, std::move(name), chunk_position, position, options, std::move(richness), std::move(resources), labor, randomValue);

			villageMap[id] = village;
			lastVillageID = std::max(lastVillageID.load(), id);

			village->setGame(game);
			associateWithRealm(village, realm_id);
		}
	}
}
