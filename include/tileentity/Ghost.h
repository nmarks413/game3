#pragma once

#include <functional>

#include "Tileset.h"
#include "item/Item.h"
#include "tileentity/TileEntity.h"

namespace Game3 {
	class Game;

	// enum class GhostType {Invalid, Normal, WoodenWall, Tower, Custom};

	class GhostFunction: public NamedRegisterable {
		private:
			std::function<bool(const Identifier &, const Place &)> function;

		public:
			GhostFunction(Identifier, decltype(function));
			bool operator()(const Identifier &tilename, const Place &) const;
	};

	struct GhostDetails: NamedRegisterable {
		/** Note: the player field of the Place will be null! */
		using CustomFn = std::function<void(const Place &)>;

		// GhostType type = GhostType::Invalid;
		Identifier type;
		bool useMarchingSquares = false;
		Index columnsPerRow = 16;
		Index rowOffset     = 0;
		Index columnOffset  = 0;
		CustomFn customFn;
		Identifier tilsetName;
		Identifier customTileName;

		GhostDetails() = default;
		GhostDetails(Identifier identifier_, Identifier type_, bool use_marching_squares, Index columns_per_row, Index row_offset, Index column_offset):
			NamedRegisterable(std::move(identifier_)),
			type(std::move(type_)),
			useMarchingSquares(use_marching_squares),
			columnsPerRow(columns_per_row),
			rowOffset(row_offset),
			columnOffset(column_offset) {}

		GhostDetails(Identifier identifier_, CustomFn custom_fn, Identifier custom_tile_name):
			NamedRegisterable(std::move(identifier_)),
			type("base:ghost/custom"),
			customFn(std::move(custom_fn)),
			customTileName(std::move(custom_tile_name)) {}

		static GhostDetails & get(const Game &, const ItemStack &);
	};

	void from_json(const nlohmann::json &, GhostDetails &);

	void initGhosts(Game &);

	class Ghost: public TileEntity {
		public:
			GhostDetails details;
			ItemStack material;
			TileID marched = 0;

			Ghost(const Ghost &) = delete;
			Ghost(Ghost &&) = default;
			~Ghost() override = default;

			Ghost & operator=(const Ghost &) = delete;
			Ghost & operator=(Ghost &&) = default;

			void init() override {}
			void toJSON(nlohmann::json &) const override;
			void absorbJSON(const Game &, const nlohmann::json &) override;
			void onSpawn();
			void onNeighborUpdated(Index row_offset, Index column_offset) override;
			bool onInteractNextTo(const std::shared_ptr<Player> &) override;
			void render(SpriteRenderer &) override;
			/** This method doesn't remove the tile entity or decrement the realm's ghost count by itself. */
			void confirm();

			friend class TileEntity;

		protected:
			Ghost() = default;

			Ghost(const Place &place, ItemStack material_);

		private:
			void march();
	};
}
