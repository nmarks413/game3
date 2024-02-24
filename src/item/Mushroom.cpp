#include "graphics/Tileset.h"
#include "entity/Player.h"
#include "game/Game.h"
#include "game/Inventory.h"
#include "item/Mushroom.h"
#include "realm/Realm.h"
#include "ui/Canvas.h"
#include "ui/MainWindow.h"

namespace Game3 {
	Mushroom::Mushroom(ItemID id_, std::string name_, MoneyCount base_price, Mushroom::ID sub_id):
		Item(std::move(id_), std::move(name_), base_price, 64),
		subID(sub_id) {}

	void Mushroom::getOffsets(const Game &game, std::shared_ptr<Texture> &texture, double &x_offset, double &y_offset) {
		texture = game.registry<TextureRegistry>().at("base:texture/mushrooms");
		texture->init();
		x_offset = (subID % 6) * 8.;
		y_offset = (subID / 6) * 8.;
	}

	Glib::RefPtr<Gdk::Pixbuf> Mushroom::makeImage(const Game &game, const ItemStack &) const {
		auto texture = game.registry<TextureRegistry>().at("base:texture/mushrooms");
		texture->init();
		constexpr int width  = 16;
		constexpr int height = 16;
		const int channels = texture->format == GL_RGBA? 4 : 3;
		const size_t row_size = size_t(channels) * width;

		const auto x = (subID % 6) * 16;
		const auto y = (subID / 6) * 16;

		if (!rawImage) {
			rawImage = std::make_unique<uint8_t[]>(size_t(channels) * width * height);
			uint8_t *raw_pointer = rawImage.get();
			uint8_t *texture_pointer = texture->data.get() + ptrdiff_t(y) * texture->width * channels + ptrdiff_t(x) * channels;
			for (int row = 0; row < height; ++row) {
				std::memcpy(raw_pointer + row_size * row, texture_pointer, row_size);
				texture_pointer += ptrdiff_t(channels) * texture->width;
			}
		}

		constexpr int doublings = 3;
		return Gdk::Pixbuf::create_from_data(rawImage.get(), Gdk::Colorspace::RGB, texture->alpha, 8, width, height, row_size)
		       ->scale_simple(width << doublings, height << doublings, Gdk::InterpType::NEAREST);
	}
}
