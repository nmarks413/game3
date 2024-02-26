#pragma once

#include "types/Types.h"
#include "data/ChunkSet.h"
#include "game/Chunk.h"
#include "types/ChunkPosition.h"
#include "threading/Lockable.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <SQLiteCpp/SQLiteCpp.h>

namespace Game3 {
	class Entity;
	class Player;
	class Realm;
	class ServerGame;
	class Tileset;
	class TileEntity;

	class GameDB {
		private:
			// TODO: weak_ptr
			ServerGame &game;
			std::filesystem::path path;

			void bind(SQLite::Statement &, const std::shared_ptr<Player> &);

		public:
			Lockable<std::unique_ptr<SQLite::Database>, std::recursive_mutex> database;

			GameDB(ServerGame &);

			void open(std::filesystem::path);
			void close();

			void writeAll();
			void readAll();

			void writeRules();
			void readRules();

			/** Writes metadata, chunk data, entity data and tile entity data for all realms. */
			void writeAllRealms();

			void writeRealm(const std::shared_ptr<Realm> &);

			void writeChunk(const std::shared_ptr<Realm> &, ChunkPosition, bool use_transaction = true);

			void readAllRealms();

			/** Reads metadata from the database and returns an empty realm based on the metadata. */
			std::shared_ptr<Realm> loadRealm(RealmID, bool do_lock);

			void writeRealmMeta(const std::shared_ptr<Realm> &, bool use_transaction = true);

			std::optional<ChunkSet> getChunk(RealmID, ChunkPosition);

			bool readUser(const std::string &username, std::string *display_name_out, nlohmann::json *json_out, std::optional<Place> *release_place);
			void writeUser(const std::string &username, const nlohmann::json &, const std::optional<Place> &release_place);
			void writeUser(const Player &);
			void writeReleasePlace(const std::string &username, const std::optional<Place> &release_place);
			bool hasName(const std::string &username, const std::string &display_name);
			std::optional<Place> readReleasePlace(const std::string &username);

			void writeTileEntities(const std::function<bool(std::shared_ptr<TileEntity> &)> &, bool use_transaction = true);
			void writeTileEntities(const std::shared_ptr<Realm> &, bool use_transaction = true);
			void deleteTileEntity(const std::shared_ptr<TileEntity> &);

			void writeEntities(const std::function<bool(std::shared_ptr<Entity> &)> &, bool use_transaction = true);
			void writeEntities(const std::shared_ptr<Realm> &, bool use_transaction = true);
			void deleteEntity(const std::shared_ptr<Entity> &);

			std::string readRealmTilesetHash(RealmID, bool do_lock = true);
			bool hasTileset(const std::string &hash, bool do_lock = true);

			void writeTilesetMeta(const Tileset &, bool use_transaction = true);
			/** Returns whether anything was found. */
			bool readTilesetMeta(const std::string &hash, nlohmann::json &, bool do_lock = true);

			inline bool isOpen() {
				return database != nullptr;
			}

			template <typename C>
			void writeUsers(const C &container) {
				if (container.empty())
					return;

				assert(database);
				auto db_lock = database.uniqueLock();

				SQLite::Transaction transaction{*database};
				SQLite::Statement statement{*database, "INSERT OR REPLACE INTO users VALUES (?, ?, ?, ?, ?)"};

				for (const std::shared_ptr<Player> player: container) {
					bind(statement, player);
					statement.exec();
					statement.reset();
				}

				transaction.commit();
			}
	};
}
