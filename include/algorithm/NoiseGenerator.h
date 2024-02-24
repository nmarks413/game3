#pragma once

#include <noise/noise.h>
#include <FastNoise/FastNoise.h>
#include <FastNoise/Generators/Simplex.h>

#include <memory>
#include <vector>

namespace Game3 {
	class NoiseGenerator {
		protected:
			int seed = 0;

		public:
			NoiseGenerator() = default;

			NoiseGenerator(int seed_):
				seed(seed_) {}

			virtual ~NoiseGenerator() = default;
			virtual double operator()(double x, double y) const = 0;
			virtual double operator()(double x, double y, double z) const = 0;
			// virtual void fill(std::vector<float> &, double x, double y,
			virtual void setSeed(int) = 0;
	};

	class FastNoise2Generator: public NoiseGenerator {
		private:
			FastNoise::SmartNode<> fastNoise = FastNoise::NewFromEncodedNodeTree(getNodeTree(), FastSIMD::Level_AVX512);

			static const char * getNodeTree();

		public:
			using NoiseGenerator::NoiseGenerator;

			double operator()(double x, double y) const override {
				return fastNoise->GenSingle2D(x, y, seed);
			}

			double operator()(double x, double y, double z) const override {
				return fastNoise->GenSingle3D(x, y, z, seed);
			}

			void setSeed(int seed_) override {
				seed = seed_;
			}

			void fill(std::vector<float> &vector, int x_start, int y_start, int x_count, int y_count, float frequency) const {
				vector.resize(x_count * y_count);
				fastNoise->GenUniformGrid2D(vector.data(), x_start, y_start, x_count, y_count, frequency, seed);
			}
	};

	class LibnoiseGenerator: public NoiseGenerator {
		private:
			noise::module::Perlin perlin;

		public:
			using NoiseGenerator::NoiseGenerator;

			LibnoiseGenerator(int seed_): NoiseGenerator(seed_) {
				perlin.SetSeed(seed);
			}

			LibnoiseGenerator(LibnoiseGenerator &&other):
				LibnoiseGenerator(other.seed) {}

			LibnoiseGenerator(const LibnoiseGenerator &other):
				LibnoiseGenerator(other.seed) {}

			LibnoiseGenerator & operator=(LibnoiseGenerator &&other) {
				perlin.SetSeed(other.seed);
				seed = other.seed;
				return *this;
			}

			LibnoiseGenerator & operator=(const LibnoiseGenerator &other) {
				perlin.SetSeed(other.seed);
				seed = other.seed;
				return *this;
			}

			double operator()(double x, double y) const override {
				return perlin.GetValue(x, y, 0.0);
			}

			double operator()(double x, double y, double z) const override {
				return perlin.GetValue(x, y, z);
			}

			void setSeed(int seed_) override {
				seed = seed_;
				perlin.SetSeed(seed_);
			}
	};

	using DefaultNoiseGenerator = FastNoise2Generator;
}
