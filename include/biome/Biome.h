#pragma once

#include <map>
#include <memory>
#include <random>

#include "types/Types.h"

namespace Game3 {
	class NoiseGenerator;
	class Realm;
	struct WorldGenParams;

	class Biome {
		public:
			constexpr static BiomeType VOID      = 0;
			constexpr static BiomeType GRASSLAND = 1;
			constexpr static BiomeType VOLCANIC  = 2;
			constexpr static BiomeType SNOWY     = 3;
			constexpr static BiomeType DESERT    = 4;
			constexpr static BiomeType CAVE      = 5;
			constexpr static BiomeType GRIMSTONE = 6;
			constexpr static BiomeType COUNT     = GRIMSTONE + 1;

			const BiomeType type;

			Biome() = delete;
			Biome(BiomeType type_): type(type_) {}
			virtual ~Biome() = default;

			virtual void init(Realm &realm_, int noise_seed);

			/** Returns the noise value generated for the position. */
			virtual double generate(Index row, Index column, std::default_random_engine &, const NoiseGenerator &, const WorldGenParams &, double suggested_noise) {
				(void) row; (void) column; (void) suggested_noise;
				return 0.;
			}

			virtual void postgen(Index row, Index column, std::default_random_engine &, const NoiseGenerator &, const WorldGenParams &) {
				(void) row; (void) column;
			}

			static std::map<BiomeType, std::shared_ptr<Biome>> getMap(Realm &, int noise_seed);

		protected:
			inline Realm * getRealm() { return realm; }
			inline void setRealm(Realm &realm_) { realm = &realm_; }
			virtual std::shared_ptr<Biome> clone() const { return std::make_shared<Biome>(*this); }

		private:
			Realm *realm = nullptr;
			static std::map<BiomeType, std::shared_ptr<const Biome>> map;
	};

	using BiomePtr = std::shared_ptr<Biome>;
}