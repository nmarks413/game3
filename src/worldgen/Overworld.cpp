#include <iostream>

#include <libnoise/noise.h>

#include "Tiles.h"
#include "realm/Realm.h"
#include "tileentity/Teleporter.h"
#include "util/Timer.h"
#include "util/Util.h"
#include "worldgen/Overworld.h"
#include "worldgen/Town.h"

namespace Game3::WorldGen {
	void generateOverworld(const std::shared_ptr<Realm> &realm, std::default_random_engine &rng, int noise_seed, double noise_zoom, double noise_threshold) {
		const auto width  = realm->getWidth();
		const auto height = realm->getHeight();

		noise::module::Perlin perlin;
		perlin.SetSeed(noise_seed);

		auto &tilemap1 = realm->tilemap1;
		auto &tilemap2 = realm->tilemap2;
		auto &tilemap3 = realm->tilemap3;

		tilemap1->tiles.assign(tilemap1->tiles.size(), 0);
		tilemap2->tiles.assign(tilemap2->tiles.size(), 0);
		tilemap3->tiles.assign(tilemap3->tiles.size(), 0);

		static const std::vector<TileID> grasses {
			OverworldTiles::GRASS_ALT1, OverworldTiles::GRASS_ALT2,
			OverworldTiles::GRASS, OverworldTiles::GRASS, OverworldTiles::GRASS, OverworldTiles::GRASS, OverworldTiles::GRASS, OverworldTiles::GRASS, OverworldTiles::GRASS
		};

		auto saved_noise = std::make_unique<double[]>(width * height);

		Timer noise_timer("Noise");
		for (int row = 0; row < height; ++row)
			for (int column = 0; column < width; ++column) {
				double noise = perlin.GetValue(row / noise_zoom, column / noise_zoom, 0.666);
				saved_noise[row * width + column] = noise;
				if (noise < noise_threshold)
					realm->setLayer1(row, column, OverworldTiles::DEEPER_WATER);
				else if (noise < noise_threshold + 0.1)
					realm->setLayer1(row, column, OverworldTiles::DEEP_WATER);
				else if (noise < noise_threshold + 0.2)
					realm->setLayer1(row, column, OverworldTiles::WATER);
				else if (noise < noise_threshold + 0.3)
					realm->setLayer1(row, column, OverworldTiles::SHALLOW_WATER);
				else if (noise < noise_threshold + 0.4)
					realm->setLayer1(row, column, OverworldTiles::SAND);
				else if (noise < noise_threshold + 0.5)
					realm->setLayer1(row, column, OverworldTiles::LIGHT_GRASS);
				else
					realm->setLayer1(row, column, choose(grasses, rng));
			}
		noise_timer.stop();

		constexpr static int m = 26, n = 34, pad = 2;
		Timer land_timer("GetLand");
		auto starts = tilemap1->getLand(realm->type, m + pad * 2, n + pad * 2);
		if (starts.empty())
			throw std::runtime_error("Map has no land");
		land_timer.stop();

		Timer oil_timer("Oil");
		auto oil_starts = tilemap1->getLand(realm->type);
		std::shuffle(oil_starts.begin(), oil_starts.end(), rng);
		for (size_t i = 0, max = oil_starts.size() / 2000; i < max; ++i) {
			const Index index = oil_starts.back();
			if (noise_threshold + 0.6 <= saved_noise[index])
				realm->setLayer2(index, OverworldTiles::OIL);
			oil_starts.pop_back();
		}
		oil_timer.stop();

		realm->randomLand = choose(starts, rng);
		std::vector<Index> candidates;
		candidates.reserve(starts.size() / 16);
		Timer candidate_timer("Candidates");
		auto &tiles1 = tilemap1->tiles;
		for (const auto index: starts) {
			const size_t row_start = index / tilemap1->width + pad, row_end = row_start + m;
			const size_t column_start = index % tilemap1->width + pad, column_end = column_start + n;
			for (size_t row = row_start; row < row_end; ++row)
				for (size_t column = column_start; column < column_end; ++column) {
					const Index index = row * tilemap1->width + column;
					if (!overworldTiles.isLand(tiles1[index]))
						goto failed;
				}
			candidates.push_back(index);
			failed:
			continue;
		}
		candidate_timer.stop();

		std::cout << "Found " << candidates.size() << " candidate" << (candidates.size() == 1? "" : "s") << ".\n";
		if (!candidates.empty())
			WorldGen::generateTown(realm, rng, choose(candidates, rng) + pad * (tilemap1->width + 1), n, m, pad);

		Timer::summary();
		Timer::clear();
	}
}
