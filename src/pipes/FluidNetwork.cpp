#include "Log.h"
#include "game/Fluids.h"
#include "pipes/FluidNetwork.h"
#include "realm/Realm.h"
#include "tileentity/FluidHoldingTileEntity.h"

namespace Game3 {
	void FluidNetwork::tick(Tick tick_id) {
		if (!canTick(tick_id))
			return;

		PipeNetwork::tick(tick_id);

		auto realm = weakRealm.lock();
		if (!realm || insertions.empty())
			return;

		auto &levels = fluidContainer->levels;

		{
			auto fluid_lock = levels.uniqueLock();
			if (auto begin = levels.begin(); begin != levels.end()) {
				auto &[id, amount] = *begin;

				if (const FluidAmount remainder = distribute(FluidStack(id, amount)); 0 < remainder) {
					amount = remainder;
					return;
				}

				levels.erase(begin);
			}
		}

		auto fluid_lock = levels.sharedLock();

		for (auto iter = extractions.begin(), end = extractions.end(); iter != end; ++iter) {
			const auto &[position, direction] = *iter;

			auto fluid_holding = std::dynamic_pointer_cast<FluidHoldingTileEntity>(realm->tileEntityAt(position));
			if (!fluid_holding)
				continue;

			// Extract the first fluid that isn't contained in our overflow storage.
			std::optional<FluidStack> extracted = fluid_holding->extractFluid(direction, [&](FluidID candidate) {
				return !levels.contains(candidate);
			}, true, {});

			if (extracted) {
				if (const FluidAmount remainder = distribute(*extracted); 0 < remainder)
					levels[extracted->id] = remainder;
				return;
			}
		}
	}

	bool FluidNetwork::canWorkWith(const std::shared_ptr<TileEntity> &tile_entity) const {
		return std::dynamic_pointer_cast<FluidHoldingTileEntity>(tile_entity) != nullptr;
	}

	FluidAmount FluidNetwork::distribute(const FluidStack &stack) {
		auto [id, amount] = stack;

		RealmPtr realm = weakRealm.lock();
		if (!realm)
			return amount;

		{
			auto insertions_lock = insertions.sharedLock();
			if (insertions.empty())
				return amount;
		}

		std::vector<std::pair<std::shared_ptr<FluidHoldingTileEntity>, Direction>> accepting_insertions;

		{
			auto insertions_lock = insertions.uniqueLock();
			accepting_insertions.reserve(insertions.size());

			FluidStack minimum{id, 1};

			std::erase_if(insertions, [&](const std::pair<Position, Direction> &pair) {
				const auto &[position, direction] = pair;

				auto fluid_holding = std::dynamic_pointer_cast<FluidHoldingTileEntity>(realm->tileEntityAt(position));
				if (!fluid_holding)
					return true;

				if (fluid_holding->canInsertFluid(minimum, direction))
					accepting_insertions.emplace_back(fluid_holding, direction);

				return false;
			});
		}

		if (accepting_insertions.empty())
			return amount;

		size_t insertions_remaining = accepting_insertions.size();

		for (const auto &[insertion, direction]: accepting_insertions) {
			FluidAmount to_distribute = amount / insertions_remaining;

			if (insertions_remaining == 1) {
				const FluidAmount remainder = amount % insertions_remaining;
				to_distribute += remainder;
			}

			const FluidAmount leftover = insertion->addFluid(FluidStack(id, to_distribute), direction);
			const FluidAmount distributed = to_distribute - leftover;
			amount -= distributed;

			--insertions_remaining;
		}

		return amount;
	}
}
