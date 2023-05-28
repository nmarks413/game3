#include "threading/ThreadContext.h"
#include "entity/Animal.h"
#include "game/Game.h"
#include "net/Buffer.h"
#include "realm/Keep.h"
#include "tileentity/Building.h"
#include "tileentity/Chest.h"
#include "tileentity/Teleporter.h"
#include "ui/TextRenderer.h"

namespace Game3 {
	ThreadPool Animal::threadPool{2};

	Animal::Animal(EntityType type_):
		Entity(std::move(type_)) {}

	void Animal::render(SpriteRenderer &sprite, TextRenderer &text) {
		if (!isVisible())
			return;

		Entity::render(sprite, text);

		text.drawOnMap(std::to_string(getGID()), {
			.x = static_cast<float>(position.column) + offset.x() + .5f,
			.y = static_cast<float>(position.row) + offset.y(),
			.color = {path.empty()? 0.f : 1.f, 0.f, 0.f, 1.f},
			.align = TextAlign::Center,
		});
	}

	bool Animal::onInteractNextTo(const std::shared_ptr<Player> &player) {
		INFO("Path length for " << typeid(*this).name() << " is " << path.size());
		{
			auto lock = lockVisibleEntitiesShared();
			INFO("Player is visible? " << std::boolalpha << visiblePlayers.contains(player));
		}
		return true;
	}

	void Animal::toJSON(nlohmann::json &json) const {
		Entity::toJSON(json);

		if (timeUntilWander != 0.f)
			json["timeUntilWander"] = timeUntilWander.load();
	}

	void Animal::absorbJSON(Game &game, const nlohmann::json &json) {
		Entity::absorbJSON(game, json);

		if (auto iter = json.find("timeUntilWander"); iter != json.end())
			timeUntilWander = *iter;
	}

	void Animal::init(Game &game) {
		Entity::init(game);
		threadPool.start();
	}

	void Animal::tick(Game &game, float delta) {
		Entity::tick(game, delta);

		if (getSide() == Side::Server) {
			if (!attemptingWander && (timeUntilWander -= delta) <= 0.f)
				wander();
		}
	}

	bool Animal::wander() {
		if (!attemptingWander.exchange(true)) {
			const auto [row, column] = position;
			return threadPool.add([this, row, column](ThreadPool &, size_t) {
				pathfind({
					threadContext.random(static_cast<int64_t>(row    - wanderRadius), static_cast<int64_t>(row    + wanderRadius)),
					threadContext.random(static_cast<int64_t>(column - wanderRadius), static_cast<int64_t>(column + wanderRadius))
				});

				timeUntilWander = getWanderDistribution()(threadContext.rng);
				attemptingWander = false;
			});
		}

		return false;
	}

	void Animal::encode(Buffer &buffer) {
		Entity::encode(buffer);
		buffer << destination;
		buffer << timeUntilWander.load();
		buffer << wanderRadius;
	}

	void Animal::decode(Buffer &buffer) {
		Entity::decode(buffer);
		buffer >> destination;
		timeUntilWander = buffer.take<float>();
		buffer >> wanderRadius;
	}
}
