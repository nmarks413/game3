#include "Log.h"
#include "entity/ItemEntity.h"
#include "entity/Player.h"
#include "game/ClientGame.h"
#include "game/Inventory.h"
#include "graphics/ItemTexture.h"
#include "graphics/RendererContext.h"
#include "graphics/SpriteRenderer.h"
#include "item/Item.h"
#include "net/Buffer.h"
#include "realm/Realm.h"
#include "registry/Registries.h"
#include "ui/Canvas.h"

namespace Game3 {
	ItemEntity::ItemEntity(const GamePtr &game):
		Entity(ID()), stack(game) {}

	ItemEntity::ItemEntity(ItemStack stack_):
		Entity(ID()), stack(std::move(stack_)) {}

	void ItemEntity::setStack(ItemStack stack_) {
		stack = std::move(stack_);
		setTexture(getRealm()->getGame());
	}

	void ItemEntity::setTexture(const GamePtr &game) {
		if (getSide() != Side::Client)
			return;
		std::shared_ptr<ItemTexture> item_texture = game->registry<ItemTextureRegistry>().at(stack.item->identifier);
		texture = stack.getTexture(*game);
		texture->init();
		offsetX = item_texture->x / 2.f;
		offsetY = item_texture->y / 2.f;
		sizeX = float(item_texture->width);
		sizeY = float(item_texture->height);
	}

	std::shared_ptr<ItemEntity> ItemEntity::create(const GamePtr &) {
		return Entity::create<ItemEntity>();
	}

	std::shared_ptr<ItemEntity> ItemEntity::create(const GamePtr &, ItemStack stack) {
		return Entity::create<ItemEntity>(std::move(stack));
	}

	std::shared_ptr<ItemEntity> ItemEntity::fromJSON(const GamePtr &game, const nlohmann::json &json) {
		if (json.is_null())
			return create(game, ItemStack(game));
		auto out = create(game, ItemStack::fromJSON(game, json.at("stack")));
		out->absorbJSON(game, json);
		return out;
	}

	void ItemEntity::toJSON(nlohmann::json &json) const {
		Entity::toJSON(json);
		json["stack"] = getStack();
	}

	void ItemEntity::init(const GamePtr &game) {
		Entity::init(game);
		if (stack.item && getSide() == Side::Client)
			stack.item->getOffsets(*game, texture, offsetX, offsetY);
	}

	void ItemEntity::tick(const TickArgs &) {
		if (firstTick)
			firstTick = false;
		else
			--secondsLeft;

		if (secondsLeft <= 0)
			remove();
		else
			enqueueTick(std::chrono::seconds(1));
	}

	void ItemEntity::render(const RendererContext &renderers) {
		SpriteRenderer &sprite_renderer = renderers.batchSprite;

		if (!isVisible())
			return;

		if (texture == nullptr || needsTexture) {
			if (needsTexture) {
				setTexture(sprite_renderer.canvas->game);
				needsTexture = false;
			} else
				return;
		}

		const float x = position.column + offset.x;
		const float y = position.row + offset.y;

		sprite_renderer(texture, {
			.x = x + .125f,
			.y = y + .125f,
			.offsetX = offsetX,
			.offsetY = offsetY,
			.sizeX = sizeX,
			.sizeY = sizeY,
			.scaleX = .75f * 16.f / sizeX,
			.scaleY = .75f * 16.f / sizeY,
		});
	}

	bool ItemEntity::interact(const std::shared_ptr<Player> &player) {
		if (getSide() != Side::Server)
			return true;

		if (std::optional<ItemStack> leftover = player->getInventory(0)->add(stack)) {
			stack = std::move(*leftover);
			increaseUpdateCounter();
		} else
			remove();

		return true;
	}

	std::string ItemEntity::getName() const {
		return stack.item->name;
	}

	void ItemEntity::encode(Buffer &buffer) {
		Entity::encode(buffer);
		GamePtr game = getGame();
		stack.encode(*game, buffer);
		buffer << secondsLeft;
	}

	void ItemEntity::decode(Buffer &buffer) {
		Entity::decode(buffer);
		GamePtr game = getGame();
		stack.decode(*game, buffer);
		buffer >> secondsLeft;
	}

	void to_json(nlohmann::json &json, const ItemEntity &item_entity) {
		item_entity.toJSON(json);
	}
}
