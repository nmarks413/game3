#include "entity/Player.h"
#include "game/ClientGame.h"
#include "graphics/SpriteRenderer.h"
#include "graphics/Tileset.h"
#include "interface/HasFluidType.h"
#include "packet/OpenModuleForAgentPacket.h"
#include "realm/Realm.h"
#include "tileentity/EternalFountain.h"
#include "ui/module/MultiModule.h"

namespace Game3 {
	namespace {
		constexpr std::chrono::milliseconds PERIOD{250};
	}

	EternalFountain::EternalFountain() = default;

	EternalFountain::EternalFountain(Identifier tile_id, Position position_):
		TileEntity(std::move(tile_id), ID(), position_, true) {}

	EternalFountain::EternalFountain(Position position_):
		EternalFountain("base:tile/eternal_fountain"_id, position_) {}

	size_t EternalFountain::getMaxFluidTypes() const {
		return 100;
	}

	FluidAmount EternalFountain::getMaxLevel(FluidID) {
		return 1'000 * FluidTile::FULL;
	}

	void EternalFountain::init(Game &game) {
		HasFluids::init(safeDynamicCast<HasFluids>(shared_from_this()));
		TileEntity::init(game);
		HasInventory::setInventory(Inventory::create(shared_from_this(), 1), 0);
	}

	void EternalFountain::tick(const TickArgs &args) {
		RealmPtr realm = weakRealm.lock();
		if (!realm || realm->getSide() != Side::Server)
			return;

		Ticker ticker{*this, args};
		enqueueTick(PERIOD);

		const InventoryPtr inventory = getInventory(0);

		ItemStackPtr stack;
		{
			auto inventory_lock = inventory->sharedLock();
			stack = (*inventory)[0];
		}

		if (!stack)
			return;

		Identifier fluid_type;

		if (auto has_fluid_type = std::dynamic_pointer_cast<HasFluidType>(stack->item)) {
			fluid_type = has_fluid_type->getFluidType();
		} else {
			return;
		}

		if (fluid_type.empty())
			return;

		auto fluids_lock = fluidContainer->levels.uniqueLock();
		const auto fluid_id = args.game->getFluid(fluid_type)->registryID;

		FluidAmount max = getMaxLevel(fluid_id);
		FluidAmount &level = fluidContainer->levels[fluid_id];
		if (level != max) {
			level = max;
			broadcast(false);
		}
	}

	void EternalFountain::toJSON(nlohmann::json &json) const {
		TileEntity::toJSON(json);
		FluidHoldingTileEntity::toJSON(json);
		InventoriedTileEntity::toJSON(json);
	}

	bool EternalFountain::onInteractNextTo(const PlayerPtr &player, Modifiers modifiers, const ItemStackPtr &, Hand) {
		RealmPtr realm = getRealm();

		if (modifiers.onlyAlt()) {
			getInventory(0)->iterate([&](const ItemStackPtr &stack, Slot) {
				stack->spawn(getPlace());
				return false;
			});
			realm->queueDestruction(getSelf());
			player->give(ItemStack::create(realm->getGame(), "base:item/eternal_fountain"_id));
			return true;
		}

		player->send(OpenModuleForAgentPacket(MultiModule<Substance::Item, Substance::Fluid>::ID(), getGID()));
		FluidHoldingTileEntity::addObserver(player, true);
		InventoriedTileEntity::addObserver(player, true);

		return false;
	}

	void EternalFountain::absorbJSON(const GamePtr &game, const nlohmann::json &json) {
		TileEntity::absorbJSON(game, json);
		FluidHoldingTileEntity::absorbJSON(game, json);
		InventoriedTileEntity::absorbJSON(game, json);
	}

	void EternalFountain::encode(Game &game, Buffer &buffer) {
		TileEntity::encode(game, buffer);
		FluidHoldingTileEntity::encode(game, buffer);
		InventoriedTileEntity::encode(game, buffer);
	}

	void EternalFountain::decode(Game &game, Buffer &buffer) {
		TileEntity::decode(game, buffer);
		FluidHoldingTileEntity::decode(game, buffer);
		InventoriedTileEntity::decode(game, buffer);
	}

	void EternalFountain::broadcast(bool force) {
		assert(getSide() == Side::Server);

		if (force) {
			TileEntity::broadcast(true);
			return;
		}

		const TileEntityPacket packet(getSelf());

		auto fluid_holding_lock = FluidHoldingTileEntity::observers.uniqueLock();

		std::erase_if(FluidHoldingTileEntity::observers, [&](const std::weak_ptr<Player> &weak_player) {
			if (auto player = weak_player.lock()) {
				player->send(packet);
				return false;
			}

			return true;
		});

		auto inventoried_lock = InventoriedTileEntity::observers.uniqueLock();

		std::erase_if(InventoriedTileEntity::observers, [&](const std::weak_ptr<Player> &weak_player) {
			if (auto player = weak_player.lock()) {
				if (!FluidHoldingTileEntity::observers.contains(player))
					player->send(packet);
				return false;
			}

			return true;
		});
	}

	GamePtr EternalFountain::getGame() const {
		return TileEntity::getGame();
	}
}
