#include "game/TileProvider.h"
#include "graphics/Tileset.h"
#include "realm/Realm.h"
#include "threading/ThreadPool.h"
#include "threading/Waiter.h"
#include "types/ChunkPosition.h"
#include "worldgen/VillageGen.h"

#include <boost/functional/hash.hpp>

#include <random>
#include <tuple>

namespace Game3 {
	VillageOptions::VillageOptions(int width_, int height_, int padding_):
		width(width_), height(height_), padding(padding_) {}

	bool chunkValidForVillage(const ChunkPosition &chunk_position, int realm_seed) {
		// Credit: https://github.com/crbednarz/AMIDST/blob/6c0cd3394268fb6570798ffbbbb9e23d053a98f4/src/amidst/map/layers/VillageLayer.java

		constexpr static int32_t SUPERCHUNK_SIZE = 4;
		constexpr static int32_t SUPERCHUNK_OFFSET = 1;

		const auto [original_x, original_y] = chunk_position;

		auto [x, y] = chunk_position;

		if (x < 0)
			x -= SUPERCHUNK_SIZE - 1;

		if (y < 0)
			y -= SUPERCHUNK_SIZE - 1;

		auto super_x = x / SUPERCHUNK_SIZE;
		auto super_y = y / SUPERCHUNK_SIZE;

		std::mt19937 prng(boost::hash_value(std::make_tuple(super_x, super_y, realm_seed)));
		std::uniform_int_distribution distribution(0, SUPERCHUNK_SIZE - SUPERCHUNK_OFFSET - 1);

		x = super_x * SUPERCHUNK_SIZE + distribution(prng);
		y = super_y * SUPERCHUNK_SIZE + distribution(prng);

		return original_x == x && original_y == y;
	}

	std::vector<Position> getVillageCandidates(const Realm &realm, const ChunkPosition &chunk_position, const VillageOptions &options, ThreadPool &pool, std::optional<std::vector<Position>> starts) {
		constexpr static size_t SECTOR_SIZE = 512;
		constexpr static size_t GUESS_FACTOR = 16;

		const TileProvider &provider = realm.tileProvider;
		const Tileset &tileset = realm.getTileset();

		if (!starts)
			starts.emplace(provider.getLand(realm.getGame(), ChunkRange(chunk_position, chunk_position), options.height + options.padding * 2, options.width + options.padding * 2));

		std::vector<Position> candidates;
		std::mutex candidates_mutex;

		candidates.reserve(starts->size() / GUESS_FACTOR);
		const size_t sector_max = updiv(starts->size(), SECTOR_SIZE);

		Waiter waiter(sector_max);

		for (size_t sector = 0; sector < sector_max; ++sector) {
			pool.add([&, sector](ThreadPool &, size_t) {
				std::vector<Position> thread_candidates;

				for (size_t i = sector * SECTOR_SIZE, max = std::min((sector + 1) * SECTOR_SIZE, starts->size()); i < max; ++i) {
					const auto position = (*starts)[i];
					const Index row_start = position.row + options.padding;
					const Index row_end = row_start + options.height;
					const Index column_start = position.column + options.padding;
					const Index column_end = column_start + options.width;

					for (Index row = row_start; row < row_end; row += 2) {
						for (Index column = column_start; column < column_end; column += 2) {
							if (auto tile = provider.tryTile(Layer::Terrain, {row, column}); !tile || !tileset.isLand(*tile))
								goto failed;

							if (realm.hasFluid({row, column}))
								goto failed;
						}
					}

					thread_candidates.push_back(position);

					failed:
					continue;
				}

				std::unique_lock lock(candidates_mutex);
				candidates.insert(candidates.end(), thread_candidates.begin(), thread_candidates.end());
				--waiter;
			});
		}

		waiter.wait();
		return candidates;
	}

}