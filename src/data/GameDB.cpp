#include "data/GameDB.h"
#include "game/Game.h"
#include "realm/Realm.h"
#include "util/Endian.h"
#include "util/Util.h"

#include <nlohmann/json.hpp>

namespace Game3 {
	GameDB::GameDB(Game &game_):
		game(game_) {}

	void GameDB::open(std::filesystem::path path_) {
		close();
		path = std::move(path_);

		leveldb::DB *db;
		leveldb::Options options;
		options.create_if_missing = true;
		leveldb::Status status = leveldb::DB::Open(options, path, &db);
		database.reset(db);
	}

	void GameDB::close() {
		database.reset();
	}

	std::string GameDB::read(const std::string &key) {

	}

	std::string GameDB::read(const std::string &key, std::string otherwise) {

	}

	void GameDB::writeAllChunks() {
		game.iterateRealms([this](const RealmPtr &realm) {
			std::shared_lock lock(realm->tileProvider.chunkMutexes[0]);
			for (const auto &[chunk_position, chunk]: realm->tileProvider.chunkMaps[0])
				writeChunk(realm, chunk_position);
		});
	}

	void GameDB::writeChunk(const RealmPtr &realm, ChunkPosition chunk_position) {
		assert(database);
		const std::string key = getChunkKey(realm->id, chunk_position);
		database->Put({}, key, realm->tileProvider.getRawChunks(chunk_position));
	}

	void GameDB::readAllChunks() {
		assert(database);
		std::unique_ptr<leveldb::Iterator> iter{database->NewIterator({})};

		for (iter->SeekToFirst(); iter->Valid(); iter->Next()) {
			const auto key = iter->key();
			if (key.empty() || key[0] != 'C')
				continue;
			RealmID realm_id{};
			ChunkPosition chunk_position{};
			parseChunkKey(key.ToString(), realm_id, chunk_position);
			const auto value = iter->value();
			ChunkSet chunk_set(std::span(value.data(), value.size()));
			RealmPtr realm = game.getRealm(realm_id, [&] { return loadRealm(realm_id); });
			realm->tileProvider.absorb(chunk_position, chunk_set);
		}
	}

	RealmPtr GameDB::loadRealm(RealmID realm_id) {
		assert(database);
		std::string raw;

		if (!database->Get({}, getRealmKey(realm_id), &raw).ok())
			throw std::out_of_range("Couldn't find realm " + std::to_string(realm_id) + " in database");

		nlohmann::json json = nlohmann::json::parse(std::move(raw));
		return Realm::fromJSON(game, json, false);
	}

	std::optional<ChunkSet> GameDB::getChunk(RealmID realm_id, ChunkPosition chunk_position) {
		assert(database);
		std::string raw;

		if (!database->Get({}, getChunkKey(realm_id, chunk_position), &raw).ok())
			return std::nullopt;

		return ChunkSet(std::span(raw));
	}

	std::string GameDB::getChunkKey(RealmID realm_id, ChunkPosition chunk_position) {
		std::string out{'C'};
		appendBytes(out, realm_id);
		appendBytes(out, chunk_position.x);
		appendBytes(out, chunk_position.y);
		return out;
	}

	void GameDB::parseChunkKey(const std::string &key, RealmID &realm_id, ChunkPosition &chunk_position) {
		if (key.empty() || key[0] != 'C' || key.size() != 1 + 3 * sizeof(int32_t))
			throw std::invalid_argument("Invalid GameDB chunk key");
		std::span span(key.begin() + 1, key.end());
		realm_id = decodeLittleS32(span);
		span = span.subspan(sizeof(RealmID));
		chunk_position.x = decodeLittleS32(span);
		chunk_position.y = decodeLittleS32(span.subspan(sizeof(ChunkPosition::IntType)));
	}

	std::string GameDB::getRealmKey(RealmID realm_id) {
		std::string out{'R'};
		appendBytes(out, realm_id);
		return out;
	}

	RealmID GameDB::parseRealmKey(const std::string &key) {
		if (key.empty() || key[0] != 'R' || key.size() != 1 + sizeof(int32_t))
			throw std::invalid_argument("Invalid GameDB realm key");
		return decodeLittleS32(std::span(key.begin() + 1, key.end()));
	}
}
