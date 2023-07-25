#include "Flasker.h"
#include "util/Util.h"

#include <sstream>

#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace Game3 {
	// Credit for RGB to HSL: https://www.programmingalgorithms.com/algorithm/rgb-to-hsl/cpp/
	// Credit for HSL to RGB: https://www.programmingalgorithms.com/algorithm/hsl-to-rgb/cpp/

	struct HSL {
		uint16_t h{};
		float s{};
		float l{};
		uint8_t a{};

		HSL() = default;

		HSL(uint16_t h_, float s_, float l_, uint8_t a_ = 255):
			h(h_), s(s_), l(l_), a(a_) {}
	};

	struct RGB {
		uint8_t r{};
		uint8_t g{};
		uint8_t b{};
		uint8_t a{};

		RGB() = default;

		RGB(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_ = 255):
			r(r_), g(g_), b(b_), a(a_) {}

		RGB(uint8_t *ptr):
			r(ptr[0]), g(ptr[1]), b(ptr[2]), a(ptr[3]) {}
	};

	static HSL toHSL(RGB rgb) {
		HSL hsl;

		float r = rgb.r / 255.f;
		float g = rgb.g / 255.f;
		float b = rgb.b / 255.f;
		hsl.a = rgb.a;

		const float min = std::min(std::min(r, g), b);
		const float max = std::max(std::max(r, g), b);
		const float delta = max - min;

		hsl.l = (max + min) / 2.f;

		if (std::abs(delta) <= 0.001f) {
			hsl.h = 0;
			hsl.s = 0.f;
		} else {
			hsl.s = hsl.l <= .5f? delta / (max + min) : delta / (2 - max - min);

			float hue = 0.f;

			if (std::abs(r - max) < 0.0001f)
				hue = (g - b) / 6 / delta;
			else if (std::abs(g - max) < 0.0001f)
				hue = 1.f / 3.f + (b - r) / 6 / delta;
			else
				hue = 2.f / 3.f + (r - g) / 6 / delta;

			if (hue < 0.f)
				hue += 1.f;
			else if (1.f < hue)
				hue -= 1.f;

			hsl.h = static_cast<uint16_t>(hue * 360.f);
		}

		return hsl;
	}

	static float hueToRGB(float v1, float v2, float vh) {
		if (vh < 0.f)
			vh += 1.f;
		else if (1.f < vh)
			vh -= 1.f;

		if (6 * vh < 1)
			return v1 + (v2 - v1) * 6 * vh;

		if (2 * vh < 1)
			return v2;

		if (3 * vh < 2)
			return v1 + (v2 - v1) * (2.f / 3.f - vh) * 6.f;

		return v1;
	}

	static RGB toRGB(HSL hsl) {
		RGB rgb{};

		if (hsl.s == 0.f) {
			rgb.r = rgb.g = rgb.b = static_cast<uint8_t>(hsl.l * 255.f);
		} else {
			float hue = static_cast<float>(hsl.h) / 360.f;
			float v2 = (hsl.l < .5f)? (hsl.l * (1.f + hsl.s)) : (hsl.l + hsl.s - hsl.l * hsl.s);
			float v1 = 2.f * hsl.l - v2;

			rgb.r = static_cast<uint8_t>(255.f * hueToRGB(v1, v2, hue + 1.f / 3.f));
			rgb.g = static_cast<uint8_t>(255.f * hueToRGB(v1, v2, hue));
			rgb.b = static_cast<uint8_t>(255.f * hueToRGB(v1, v2, hue - 1.f / 3.f));
		}

		return rgb;
	}

	std::string generateFlask(uint16_t hue, float saturation) {
		int base_width  = 0;
		int base_height = 0;
		int mask_width  = 0;
		int mask_height = 0;
		int base_channels = 0;
		int mask_channels = 0;

		uint8_t *base = stbi_load("resources/flaskbase.png", &base_width, &base_height, &base_channels, 0);
		if (base == nullptr)
			throw std::runtime_error("Couldn't load resources/flaskbase.png");

		uint8_t *mask = stbi_load("resources/flaskmask.png", &mask_width, &mask_height, &mask_channels, 0);
		if (mask == nullptr)
			throw std::runtime_error("Couldn't load resources/flaskmask.png");

		if (base_width != mask_width)
			throw std::runtime_error("Width mismatch (" + std::to_string(base_width) + " base vs. " + std::to_string(mask_width) + " mask)");

		if (base_height != mask_height)
			throw std::runtime_error("Height mismatch (" + std::to_string(base_height) + " base vs. " + std::to_string(mask_height) + " mask)");

		if (base_channels != mask_channels)
			throw std::runtime_error("Channels mismatch (" + std::to_string(base_channels) + " base vs. " + std::to_string(mask_channels) + " mask)");

		const int width = base_width;
		const int height = base_height;
		const int channels = base_channels;

		if (channels != 4)
			throw std::runtime_error("Unexpected number of channels (" + std::to_string(channels) + "; expected 4)");

		auto raw = std::make_unique<uint8_t[]>(width * height * 4);

		for (int i = 0; i < width * height * channels; i += channels) {
			if (mask[i] == 0) {
				for (int j = 0; j < 4; ++j)
					raw[i + j] = base[i + j];
				continue;
			}

			HSL hsl = toHSL(RGB(&base[i]));
			hsl.h = hue;
			hsl.s = saturation;

			const RGB rgb = toRGB(hsl);
			raw[i] = rgb.r;
			raw[i + 1] = rgb.g;
			raw[i + 2] = rgb.b;
			raw[i + 3] = base[i + 3];
		}

		std::stringstream ss;

		stbi_write_png_to_func(+[](void *context, void *data, int size) {
			std::stringstream &ss = *reinterpret_cast<std::stringstream *>(context);
			ss << std::string_view(reinterpret_cast<const char *>(data), size);
		}, &ss, width, height, 4, raw.get(), width * 4);

		return ss.str();
	}

	std::string generateFlask(std::string_view hue, std::string_view saturation) {
		return generateFlask(parseNumber<uint16_t>(hue), parseNumber<float>(saturation));
	}
}
