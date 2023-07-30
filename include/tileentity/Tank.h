#pragma once

#include "tileentity/FluidHoldingTileEntity.h"

namespace Game3 {
	class Tank: public FluidHoldingTileEntity {
		public:
			static Identifier ID() { return {"base", "te/tank"}; }

			FluidAmount getMaxLevel(FluidID) const override;

			void toJSON(nlohmann::json &) const override;
			bool onInteractNextTo(const std::shared_ptr<Player> &, Modifiers) override;
			void absorbJSON(Game &, const nlohmann::json &) override;

			void encode(Game &, Buffer &) override;
			void decode(Game &, Buffer &) override;

		private:
			Tank() = default;
			Tank(Identifier tile_id, Position);
			Tank(Position);

			friend class TileEntity;
	};
}
