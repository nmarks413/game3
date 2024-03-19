#include "biology/Gene.h"
#include "entity/Animal.h"
#include "game/Game.h"
#include "graphics/TextRenderer.h"
#include "net/Buffer.h"
#include "threading/ThreadContext.h"
#include "tileentity/Building.h"
#include "tileentity/Chest.h"
#include "tileentity/Teleporter.h"

namespace Game3 {
	ThreadPool Animal::threadPool{2};

	namespace {
		constexpr HitPoints MAX_HEALTH   = 40;
		constexpr size_t    PATHFIND_MAX = 256;
	}

	std::uniform_real_distribution<float> Animal::getWanderDistribution() {
		return std::uniform_real_distribution(10.f, 20.f);
	}

	Animal::Animal():
		Entity("base:invalid/Animal") {}

	void Animal::toJSON(nlohmann::json &json) const {
		LivingEntity::toJSON(json);

		nlohmann::json &genes = json["genes"];
		iterateGenes([&](const Gene &gene) {
			genes[gene.getName()] = gene;
		});
	}

	void Animal::absorbJSON(const std::shared_ptr<Game> &game, const nlohmann::json &json) {
		LivingEntity::absorbJSON(game, json);

		absorbGenes(json.at("genes"));
	}

	void Animal::updateRiderOffset(const std::shared_ptr<Entity> &rider) {
		rider->setOffset(getOffset() + Vector3{0.f, 0.f, .3f});
	}

	bool Animal::onInteractOn(const std::shared_ptr<Player> &player, Modifiers, const ItemStackPtr &, Hand) {
		if (getRider() == player) {
			setRider(nullptr);
			return true;
		}

		return false;
	}

	bool Animal::onInteractNextTo(const std::shared_ptr<Player> &player, Modifiers, const ItemStackPtr &used_item, Hand) {
		if (!used_item || used_item->item->identifier != "base:item/wrench") {
			setRider(player);
			return true;
		}

		INFO("{} {}:", typeid(*this).name(), getGID());
		INFO("  Path length is {}", path.size());
		auto realm = getRealm();
		{
			auto lock = visibleEntities.sharedLock();
			INFO("  Player is visible? {:s}", visiblePlayers.contains(player));
		}
		{
			auto lock = player->visibleEntities.sharedLock();
			INFO("  Visible to player? {:s}", player->visibleEntities.contains(getSelf()));
		}
		if (auto ptr = realm->getEntities(getChunk()); ptr && ptr->contains(getSelf()))
			SUCCESS_("  In chunk.");
		else
			ERROR_("  Not in chunk.");
		INFO("  First wander: {}", firstWander);
		INFO("  Attempting wander: {:s}", attemptingWander.load());
		return true;
	}

	void Animal::init(const GamePtr &game) {
		Entity::init(game);
		threadPool.start();
	}

	void Animal::tick(const TickArgs &args) {
		if (getSide() == Side::Server) {
			if (firstWander) {
				firstWander = false;
			} else if (wanderTick <= args.game->getCurrentTick()) {
				// The check here is to avoid spurious wanders if something else causes the animal to tick earlier than scheduled.
				wander();
				wanderTick = enqueueTick(std::chrono::milliseconds(int64_t(1000 * getWanderDistribution()(threadContext.rng))));
			}
		}

		Entity::tick(args);
	}

	HitPoints Animal::getMaxHealth() const {
		return MAX_HEALTH;
	}

	bool Animal::wander() {
		if (!attemptingWander.exchange(true)) {
			increaseUpdateCounter();
			const auto [row, column] = position.copyBase();
			return threadPool.add([this, row = row, column = column](ThreadPool &, size_t) {
				pathfind({
					threadContext.random(int64_t(row    - wanderRadius), int64_t(row    + wanderRadius)),
					threadContext.random(int64_t(column - wanderRadius), int64_t(column + wanderRadius))
				}, PATHFIND_MAX);

				attemptingWander = false;
			});
		}

		return false;
	}

	void Animal::encode(Buffer &buffer) {
		Entity::encode(buffer);
		LivingEntity::encode(buffer);
		buffer << wanderRadius;
	}

	void Animal::decode(Buffer &buffer) {
		Entity::decode(buffer);
		LivingEntity::decode(buffer);
		buffer >> wanderRadius;
	}
}
