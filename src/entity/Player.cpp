#include <nanogui/opengl.h>

#include <iostream>

#include "entity/Player.h"
#include "game/Game.h"
#include "game/Realm.h"
#include "ui/Canvas.h"
#include "ui/MainWindow.h"

namespace Game3 {
	std::shared_ptr<Player> Player::fromJSON(const nlohmann::json &json) {
		auto out = Entity::create<Player>(json.at("id"));
		out->absorbJSON(json);
		return out;
	}

	nlohmann::json Player::toJSON() const {
		nlohmann::json json;
		to_json(json, *this);
		return json;
	}

	void Player::tick(float delta) {
		Entity::tick(delta);
		Direction final_direction = direction;
		if (movingLeft)
			move(final_direction = Direction::Left);
		if (movingRight)
			move(final_direction = Direction::Right);
		if (movingUp)
			move(final_direction = Direction::Up);
		if (movingDown)
			move(final_direction = Direction::Down);
		direction = final_direction;
	}

	void Player::interactOn() {
		auto realm = getRealm();
		auto player = std::dynamic_pointer_cast<Player>(shared_from_this());
		auto entity = realm->findEntity(position, player);
		if (!entity)
			return;
		entity->onInteractOn(player);
	}

	void Player::interactNextTo() {
		auto realm = getRealm();
		auto player = std::dynamic_pointer_cast<Player>(shared_from_this());
		auto entity = realm->findEntity(nextTo(), player);
		if (entity)
			entity->onInteractNextTo(player);
		else if (auto tileEntity = realm->tileEntityAt(nextTo()))
			tileEntity->onInteractNextTo(player);
	}

	void Player::teleport(const Position &position, const std::shared_ptr<Realm> &new_realm) {
		Entity::teleport(position, new_realm);
		auto &new_game = new_realm->getGame();
		new_game.canvas.window.glContext()->make_current();
		new_realm->reupload();
		new_game.activeRealm = new_realm;
		focus(new_game.canvas, false);
	}

	void to_json(nlohmann::json &json, const Player &player) {
		to_json(json, static_cast<const Entity &>(player));
		json["isPlayer"] = true;
	}
}
