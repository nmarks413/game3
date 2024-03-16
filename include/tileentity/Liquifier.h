#pragma once

#include "tileentity/EnergeticTileEntity.h"
#include "tileentity/FluidHoldingTileEntity.h"
#include "tileentity/InventoriedTileEntity.h"

namespace Game3 {
	class Liquifier: public FluidHoldingTileEntity, public EnergeticTileEntity, public InventoriedTileEntity {
		public:
			static Identifier ID() { return {"base", "te/liquifier"}; }

			FluidAmount getMaxLevel(FluidID) override;

			std::string getName() const override { return "Liquifier"; }

			void init(Game &) override;
			void tick(const TickArgs &) override;
			void toJSON(nlohmann::json &) const override;
			bool onInteractNextTo(const std::shared_ptr<Player> &, Modifiers, const ItemStackPtr &, Hand) override;
			void absorbJSON(const std::shared_ptr<Game> &, const nlohmann::json &) override;

			void encode(Game &, Buffer &) override;
			void decode(Game &, Buffer &) override;
			void broadcast(bool force) override;

			GamePtr getGame() const final;

		private:
			Liquifier();
			Liquifier(Identifier tile_id, Position);
			Liquifier(Position);

			friend class TileEntity;
	};
}
