#include "data/GameDB.h"
#include "entity/EntityFactory.h"
#include "entity/Player.h"
#include "error/FailedMigrationError.h"
#include "game/Game.h"
#include "graphics/Tileset.h"
#include "net/Buffer.h"
#include "realm/Realm.h"
#include "tileentity/TileEntity.h"
#include "tileentity/TileEntityFactory.h"
#include "util/Endian.h"
#include "util/Timer.h"
#include "util/Util.h"

#include <filesystem>
#include <iomanip>
#include <sstream>

#include <nlohmann/json.hpp>

namespace Game3 {
	GameDB::GameDB(Game &game_):
		game(game_) {}

	void GameDB::open(std::filesystem::path path_) {
		close();
		path = std::move(path_);
		auto db_lock = database.uniqueLock();
		database.getBase() = std::make_unique<SQLite::Database>(path, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);

		static_assert(LAYER_COUNT * sizeof(TileID) * CHUNK_SIZE * CHUNK_SIZE < 65536);
		static_assert(sizeof(BiomeType) * CHUNK_SIZE * CHUNK_SIZE < 65536);
		static_assert(sizeof(FluidInt) * CHUNK_SIZE * CHUNK_SIZE < 65536);

		database->exec(R"(
			CREATE TABLE IF NOT EXISTS chunks (
				realmID INT,
				x INT,
				y INT,
				terrain VARBINARY(65535),
				biomes  VARBINARY(65535),
				fluids  VARBINARY(65535),
				pathmap VARBINARY(65535),
				PRIMARY KEY (realmID, x, y)
			);

			CREATE TABLE IF NOT EXISTS realms (
				realmID INT PRIMARY KEY,
				json MEDIUMTEXT
			);

			CREATE TABLE IF NOT EXISTS users (
				username VARCHAR(32) PRIMARY KEY,
				displayName VARCHAR(64),
				json MEDIUMTEXT
			);

			CREATE TABLE IF NOT EXISTS tileEntities (
				globalID INT8 PRIMARY KEY,
				realmID INT,
				row INT8,
				column INT8,
				tileID VARCHAR(255),
				tileEntityID VARCHAR(255),
				encoded MEDIUMBLOB
			);

			CREATE TABLE IF NOT EXISTS entities (
				globalID INT8 PRIMARY KEY,
				realmID INT,
				row INT8,
				column INT8,
				entityType VARCHAR(255),
				direction TINYINT(1),
				encoded MEDIUMBLOB
			);

			CREATE TABLE IF NOT EXISTS realmTileMaps (
				realmID INT PRIMARY KEY,
				value MEDIUMTEXT
			);

			CREATE TABLE IF NOT EXISTS realmTilesetHashes (
				realmID INT PRIMARY KEY,
				value VARCHAR(128)
			);
		)");
	}

	void GameDB::close() {
		database.reset();
	}

	void GameDB::writeAllRealms() {
		Timer timer{"WriteAllRealms"};
		game.iterateRealms([this](const RealmPtr &realm) {
			writeRealm(realm);
		});
	}

	void GameDB::writeRealm(const RealmPtr &realm) {
		SQLite::Transaction transaction{*database};

		{
			Timer timer{"WriteRealmMeta"};
			writeRealmMeta(realm, false);
		}
		std::shared_lock lock(realm->tileProvider.chunkMutexes[0]);
		for (const auto &[chunk_position, chunk]: realm->tileProvider.chunkMaps[0]) {
			Timer timer{"WriteChunk"};
			writeChunk(realm, chunk_position, false);
		}
		{
			Timer timer{"WriteTileEntities"};
			writeTileEntities(realm, false);
		}
		{
			Timer timer{"WriteEntities"};
			writeEntities(realm, false);
		}
		{
			Timer timer{"WriteTileMap"};
			writeRealmTileMap(realm->id, realm->getTileset().getNames(), false);
		}
		{
			Timer timer{"WriteTilesetHash"};
			writeRealmTilesetHash(realm->id, realm->getTileset().getHash(), false);
		}

		Timer timer{"WriteRealmCommit"};
		transaction.commit();
	}

	void GameDB::writeChunk(const RealmPtr &realm, ChunkPosition chunk_position, bool use_transaction) {
		assert(database);
		TileProvider &provider = realm->tileProvider;

		auto db_lock = database.uniqueLock();

		std::optional<SQLite::Transaction> transaction;
		if (use_transaction)
			transaction.emplace(*database);

		SQLite::Statement statement{*database, "INSERT OR REPLACE INTO chunks VALUES (?, ?, ?, ?, ?, ?, ?)"};

		std::string raw_terrain;
		std::string raw_biomes;
		std::string raw_fluids;
		std::string raw_pathmap;

		{
			Timer timer{"GetRawTerrain"};
			raw_terrain = provider.getRawTerrain(chunk_position);
		}

		{
			Timer timer{"GetRawBiomes"};
			raw_biomes = provider.getRawBiomes(chunk_position);
		}

		{
			Timer timer{"GetRawFluids"};
			raw_fluids = provider.getRawFluids(chunk_position);
		}

		{
			Timer timer{"GetRawPathmap"};
			raw_pathmap = provider.getRawPathmap(chunk_position);
		}

		statement.bind(1, realm->id);
		statement.bind(2, chunk_position.x);
		statement.bind(3, chunk_position.y);
		{
			Timer timer{"BindTerrain"};
			statement.bind(4, raw_terrain);
		}
		{
			Timer timer{"BindBiomes"};
			statement.bind(5, raw_biomes);
		}
		{
			Timer timer{"BindFluids"};
			statement.bind(6, raw_fluids);
		}
		{
			Timer timer{"BindPathmap"};
			statement.bind(7, raw_pathmap);
		}

		{
			Timer timer{"ExecStatement"};
			statement.exec();
		}

		if (transaction) {
			Timer timer{"CommitTransaction"};
			transaction->commit();
		}
	}

	void GameDB::readAllRealms() {
		assert(database);

		Timer lock_timer{"LockDB"};
		auto db_lock = database.uniqueLock();
		lock_timer.stop();

		SQLite::Statement query{*database, "SELECT * FROM chunks"};

		Timer query_timer{"ExecuteStep"};

		while (query.executeStep()) {
			query_timer.stop();
			{
				Timer iteration_timer{"ChunkLoad"};
				RealmID realm_id = query.getColumn(0);
				ChunkPosition::IntType x = query.getColumn(1);
				ChunkPosition::IntType y = query.getColumn(2);
				const void *terrain = query.getColumn(3);
				const void *biomes  = query.getColumn(4);
				const void *fluids  = query.getColumn(5);
				const void *pathmap = query.getColumn(6);
				Timer chunk_set_timer{"ChunkSet"};
				ChunkSet chunk_set{
					std::span<const char>(reinterpret_cast<const char *>(terrain), query.getColumn(3).getBytes()),
					std::span<const char>(reinterpret_cast<const char *>(biomes),  query.getColumn(4).getBytes()),
					std::span<const char>(reinterpret_cast<const char *>(fluids),  query.getColumn(5).getBytes()),
					std::span<const char>(reinterpret_cast<const char *>(pathmap), query.getColumn(6).getBytes())
				};
				chunk_set_timer.stop();
				RealmPtr realm;
				{
					Timer get_realm_timer{"GetRealm"};
					realm = game.getRealm(realm_id, [&] { return loadRealm(realm_id, false); });
				}
				{
					Timer absorb_timer{"Absorb"};
					realm->tileProvider.absorb(ChunkPosition(x, y), std::move(chunk_set));
				}
			}
			query_timer.restart();
		}

		const bool force_migrate = std::filesystem::exists(".force-migrate");
		if (force_migrate)
			std::filesystem::remove(".force-migrate");

		game.iterateRealms([&](const RealmPtr &realm) {
			Tileset &tileset = realm->getTileset();

			if (tileset.getHash() == readRealmTilesetHash(realm->id, false))
				return;

			INFO("Auto-migrating tiles for realm " << realm->id);

			Timer migration_timer{"TileMigration"};
			std::unordered_map<TileID, TileID> migration_map;

			const std::unordered_map<TileID, Identifier> old_map = readRealmTileMap<std::unordered_map>(realm->id, false);
			const std::unordered_map<Identifier, TileID> new_map = tileset.getIDs();

			for (const auto &[numeric, identifier]: old_map)
				if (auto new_tile_iter = new_map.find(identifier); new_tile_iter != new_map.end())
					migration_map[numeric] = new_tile_iter->second;

			TileProvider &provider = realm->tileProvider;
			std::unordered_set<TileID> covered;
			std::unordered_set<TileID> warned;

			for (const Layer layer: allLayers) {
				std::unique_lock chunk_map_lock{provider.chunkMutexes.at(getIndex(layer))};
				TileProvider::ChunkMap &chunk_map = provider.chunkMaps.at(getIndex(layer));
				for (auto &[chunk_position, chunk]: chunk_map) {
					std::unique_lock chunk_lock = chunk.uniqueLock();
					for (TileID &tile_id: chunk) {
						const TileID old = tile_id;
						if (auto iter = migration_map.find(tile_id); iter != migration_map.end()) {
							tile_id = iter->second;
							if (tile_id != old && covered.insert(old).second)
								INFO(old_map.at(old) << " (" << old << ") → " << tileset.getNames().at(tile_id) << " (" << tile_id << ')');
						} else if (force_migrate) {
							if (warned.insert(old).second)
								WARN("Replacing tile " << old_map.at(old) << " (" << old << ") with nothing.");
							tile_id = 0;
						} else {
							Identifier tilename = old_map.at(old);
							ERROR("Canceling tile migration; tile " << tilename << " (" << old << ") is missing from the new tileset. Create .force-migrate to force migration.");
							throw FailedMigrationError("Migration failed due to missing tile " + tilename.str() + " (" + std::to_string(old) + ')');
						}
					}
				}
			}

			SUCCESS("Finished tile migration for realm " << realm->id);
		});
	}

	RealmPtr GameDB::loadRealm(RealmID realm_id, bool do_lock) {
		assert(database);

		std::unique_lock<std::shared_mutex> db_lock;
		if (do_lock)
			db_lock = database.uniqueLock();

		std::string raw_json;

		{
			SQLite::Statement query{*database, "SELECT json FROM realms WHERE realmID = ? LIMIT 1"};

			query.bind(1, realm_id);

			if (!query.executeStep())
				throw std::out_of_range("Couldn't find realm " + std::to_string(realm_id) + " in database");

			raw_json = query.getColumn(0).getString();
		}

		RealmPtr realm = Realm::fromJSON(game, nlohmann::json::parse(raw_json), false);

		{
			SQLite::Statement query{*database, "SELECT tileEntityID, encoded, globalID FROM tileEntities WHERE realmID = ?"};

			query.bind(1, realm_id);

			while (query.executeStep()) {
				const Identifier tile_entity_id{query.getColumn(0).getString()};
				auto factory = game.registry<TileEntityFactoryRegistry>().at(tile_entity_id);
				assert(factory);
				TileEntityPtr tile_entity = (*factory)(game);
				tile_entity->setGID(GlobalID(query.getColumn(2).getInt64()));

				const auto *buffer_bytes = reinterpret_cast<const uint8_t *>(query.getColumn(1).getBlob());
				const size_t buffer_size = query.getColumn(1).getBytes();

				tile_entity->setRealm(realm);

				Buffer buffer(std::vector<uint8_t>(buffer_bytes, buffer_bytes + buffer_size));
				buffer.context = game.shared_from_this();
				tile_entity->decode(game, buffer);
				tile_entity->init(game);

				// TODO: functionize this
				realm->tileEntities.emplace(tile_entity->position, tile_entity);
				realm->tileEntitiesByGID[tile_entity->globalID] = tile_entity;
				realm->attach(tile_entity);
				tile_entity->onSpawn();
				if (tile_entity_id == "base:te/ghost"_id)
					++realm->ghostCount;
			}
		}

		{
			SQLite::Statement query{*database, "SELECT entityType, encoded FROM entities WHERE realmID = ?"};

			query.bind(1, realm_id);

			while (query.executeStep()) {
				const Identifier entity_id{query.getColumn(0).getString()};

				auto factory = game.registry<EntityFactoryRegistry>().at(entity_id);
				assert(factory);
				EntityPtr entity = (*factory)(game);

				const auto *buffer_bytes = reinterpret_cast<const uint8_t *>(query.getColumn(1).getBlob());
				const size_t buffer_size = query.getColumn(1).getBytes();

				entity->setRealm(realm);

				Buffer buffer(std::vector<uint8_t>(buffer_bytes, buffer_bytes + buffer_size));
				buffer.context = game.shared_from_this();
				entity->decode(buffer);
				entity->init(game);

				realm->entities.insert(entity);
				realm->entitiesByGID[entity->globalID] = entity;
				realm->attach(entity);
			}
		}

		return realm;
	}

	void GameDB::writeRealmMeta(const RealmPtr &realm, bool use_transaction) {
		assert(database);

		nlohmann::json json;
		realm->toJSON(json, false);

		auto db_lock = database.uniqueLock();

		std::optional<SQLite::Transaction> transaction;
		if (use_transaction)
			transaction.emplace(*database);

		SQLite::Statement statement{*database, "INSERT OR REPLACE INTO realms VALUES (?, ?)"};

		statement.bind(1, realm->id);
		statement.bind(2, json.dump());

		statement.exec();

		if (transaction)
			transaction->commit();
	}

	std::optional<ChunkSet> GameDB::getChunk(RealmID realm_id, ChunkPosition chunk_position) {
		assert(database);

		auto db_lock = database.uniqueLock();

		SQLite::Statement query{*database, "SELECT terrain, biomes, fluids, pathmap FROM chunks WHERE realmID = ? AND x = ? AND y = ? LIMIT 1"};

		query.bind(1, realm_id);
		query.bind(2, chunk_position.x);
		query.bind(3, chunk_position.y);

		while (query.executeStep()) {
			const void *terrain = query.getColumn(0);
			const void *biomes  = query.getColumn(1);
			const void *fluids  = query.getColumn(2);
			const void *pathmap = query.getColumn(3);
			return ChunkSet{
				std::span<const char>(reinterpret_cast<const char *>(terrain), query.getColumn(0).getBytes()),
				std::span<const char>(reinterpret_cast<const char *>(biomes),  query.getColumn(1).getBytes()),
				std::span<const char>(reinterpret_cast<const char *>(fluids),  query.getColumn(2).getBytes()),
				std::span<const char>(reinterpret_cast<const char *>(pathmap), query.getColumn(3).getBytes())
			};
		}

		return std::nullopt;
	}

	bool GameDB::readUser(std::string_view username, std::string *display_name_out, nlohmann::json *json_out) {
		assert(database);
		auto db_lock = database.uniqueLock();

		SQLite::Statement query{*database, "SELECT displayName, json FROM users WHERE username = ? LIMIT 1"};

		query.bind(1, username.data());

		while (query.executeStep()) {
			if (display_name_out != nullptr)
				*display_name_out = std::string(query.getColumn(0));
			if (json_out != nullptr)
				*json_out = nlohmann::json::parse(std::string(query.getColumn(1)));
			return true;
		}

		return false;
	}

	void GameDB::writeUser(std::string_view username, const nlohmann::json &json) {
		assert(database);
		auto db_lock = database.uniqueLock();

		SQLite::Transaction transaction{*database};
		SQLite::Statement statement{*database, "INSERT OR REPLACE INTO users VALUES (?, ?, ?)"};

		statement.bind(1, username.data());
		statement.bind(2, json.at("displayName").get<std::string>());
		statement.bind(3, json.dump());

		statement.exec();
		transaction.commit();
	}

	bool GameDB::hasName(std::string_view username, std::string_view display_name) {
		assert(database);
		auto db_lock = database.uniqueLock();

		SQLite::Statement query{*database, "SELECT NULL FROM users WHERE username = ? OR displayName = ? LIMIT 1"};

		query.bind(1, username.data());
		query.bind(2, display_name.data());

		return query.executeStep();
	}

	void GameDB::writeTileEntities(const std::function<bool(TileEntityPtr &)> &getter, bool use_transaction) {
		assert(database);
		TileEntityPtr tile_entity;
		auto db_lock = database.uniqueLock();

		std::optional<SQLite::Transaction> transaction;
		if (use_transaction)
			transaction.emplace(*database);

		SQLite::Statement statement{*database, "INSERT OR REPLACE INTO tileEntities VALUES (?, ?, ?, ?, ?, ?, ?)"};

		while (getter(tile_entity)) {
			statement.bind(1, std::make_signed_t<GlobalID>(tile_entity->getGID()));
			statement.bind(2, tile_entity->realmID);
			statement.bind(3, tile_entity->position.row);
			statement.bind(4, tile_entity->position.column);
			statement.bind(5, tile_entity->tileID.str());
			statement.bind(6, tile_entity->tileEntityID.str());
			Buffer buffer;
			tile_entity->encode(tile_entity->getGame(), buffer);
			statement.bind(7, buffer.bytes.data(), buffer.bytes.size());
			statement.exec();
			statement.reset();
		}

		if (transaction)
			transaction->commit();
	}

	void GameDB::writeTileEntities(const RealmPtr &realm, bool use_transaction) {
		decltype(realm->tileEntities)::Base copy;
		{
			auto lock = realm->tileEntities.sharedLock();
			copy = realm->tileEntities.getBase();
		}
		auto iter = copy.begin();
		writeTileEntities([&](TileEntityPtr &out) {
			if (iter == copy.end())
				return false;
			out = iter++->second;
			return true;
		}, use_transaction);
	}

	void GameDB::deleteTileEntity(const TileEntityPtr &tile_entity) {
		assert(database);
		auto db_lock = database.uniqueLock();
		SQLite::Transaction transaction{*database};
		SQLite::Statement statement{*database, "DELETE FROM tileEntities WHERE globalID = ?"};
		statement.bind(1, std::make_signed_t<GlobalID>(tile_entity->getGID()));
		statement.exec();
		transaction.commit();
	}

	void GameDB::writeEntities(const std::function<bool(EntityPtr &)> &getter, bool use_transaction) {
		assert(database);
		EntityPtr entity;
		auto db_lock = database.uniqueLock();

		std::optional<SQLite::Transaction> transaction;
		if (use_transaction)
			transaction.emplace(*database);

		SQLite::Statement statement{*database, "INSERT OR REPLACE INTO entities VALUES (?, ?, ?, ?, ?, ?, ?)"};

		while (getter(entity)) {
			if (entity->isPlayer())
				continue;
			statement.bind(1, std::make_signed_t<GlobalID>(entity->getGID()));
			statement.bind(2, entity->realmID);
			statement.bind(3, entity->position.row);
			statement.bind(4, entity->position.column);
			statement.bind(5, entity->type.str());
			statement.bind(6, int(entity->direction.load()));
			Buffer buffer;
			entity->encode(buffer);
			statement.bind(7, buffer.bytes.data(), buffer.bytes.size());
			statement.exec();
			statement.reset();
		}

		if (transaction)
			transaction->commit();
	}

	void GameDB::writeEntities(const RealmPtr &realm, bool use_transaction) {
		decltype(realm->entities)::Base copy;
		{
			auto lock = realm->entities.sharedLock();
			copy = realm->entities.getBase();
		}
		auto iter = copy.begin();
		writeEntities([&](EntityPtr &out) {
			if (iter == copy.end())
				return false;
			out = *iter++;
			return true;
		}, use_transaction);
	}

	void GameDB::deleteEntity(const EntityPtr &entity) {
		assert(database);
		auto db_lock = database.uniqueLock();
		SQLite::Transaction transaction{*database};
		SQLite::Statement statement{*database, "DELETE FROM entities WHERE globalID = ?"};
		statement.bind(1, std::make_signed_t<GlobalID>(entity->getGID()));
		statement.exec();
		transaction.commit();
	}

	std::string GameDB::readRealmTilesetHash(RealmID realm_id, bool do_lock) {
		assert(database);

		std::unique_lock<std::shared_mutex> db_lock;
		if (do_lock)
			db_lock = database.uniqueLock();

		SQLite::Statement query{*database, "SELECT value FROM realmTilesetHashes WHERE realmID = ? LIMIT 1"};

		query.bind(1, realm_id);

		while (query.executeStep())
			return query.getColumn(0).getString();

		throw std::out_of_range("Can't find tileset hash for realm " + std::to_string(realm_id));
	}

	void GameDB::writeRealmTilesetHash(RealmID realm_id, const std::string &hash, bool use_transaction) {
		assert(database);
		auto db_lock = database.uniqueLock();

		std::optional<SQLite::Transaction> transaction;
		if (use_transaction)
			transaction.emplace(*database);

		SQLite::Statement statement{*database, "INSERT OR REPLACE INTO realmTilesetHashes VALUES (?, ?)"};

		statement.bind(1, realm_id);
		statement.bind(2, hash);

		statement.exec();

		if (transaction)
			transaction->commit();
	}

	void GameDB::bind(SQLite::Statement &statement, const PlayerPtr &player) {
		statement.bind(1, player->username);
		statement.bind(2, player->displayName);
		statement.bind(3, nlohmann::json(*player).dump());
	}
}
