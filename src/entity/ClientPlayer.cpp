#include "entity/ClientPlayer.h"
#include "game/ClientGame.h"
#include "game/Inventory.h"
#include "graphics/RectangleRenderer.h"
#include "graphics/RendererSet.h"
#include "graphics/TextRenderer.h"
#include "net/LocalClient.h"
#include "packet/AgentMessagePacket.h"
#include "packet/ContinuousInteractionPacket.h"
#include "packet/JumpPacket.h"
#include "packet/MovePlayerPacket.h"
#include "threading/ThreadContext.h"
#include "ui/Canvas.h"
#include "ui/MainWindow.h"

namespace Game3 {
	ClientPlayer::ClientPlayer(): Player() {}

	std::shared_ptr<ClientPlayer> ClientPlayer::create(Game &) {
		return Entity::create<ClientPlayer>();
	}

	void ClientPlayer::tick(Game &game, float delta) {
		lastMessageAge = lastMessageAge + delta;
		Player::tick(game, delta);
	}

	void ClientPlayer::render(const RendererSet &renderers) {
		RectangleRenderer &rectangle = renderers.rectangle;
		TextRenderer &text = renderers.text;

		if (!isVisible())
			return;

		Player::render(renderers);

		const auto [row, column] = getPosition();
		const auto [x, y, z] = offset.copyBase();

		const bool show_message = lastMessageAge.load() < 7;
		const bool show_health = !isInvincible() && 0 < maxHealth();
		float name_offset = show_message? -1 : 0;

		if (show_health) {
			name_offset -= .5;

			constexpr static float bar_offset = .15f;
			constexpr static float bar_width  = 1.5f;
			constexpr static float bar_height = .18f;
			constexpr static float thickness  = .05f;

			const float bar_x = float(column) + x - (bar_width - 1) / 2;
			const float bar_y = float(row) + y - z - bar_offset - bar_height;
			const float fraction = double(health) / maxHealth();

			rectangle.drawOnMap(RenderOptions {
				.x = bar_x - thickness,
				.y = bar_y - thickness,
				.sizeX = bar_width  + thickness * 2,
				.sizeY = bar_height + thickness * 2,
				.color = {.1, .1, .1, .9},
			});

			rectangle.drawOnMap(RenderOptions {
				.x = bar_x,
				.y = bar_y,
				.sizeX = bar_width * fraction,
				.sizeY = bar_height,
				.color = {0, 1, 0, 1},
			});

			if (fraction < 0.9999f) {
				rectangle.drawOnMap(RenderOptions {
					.x = bar_x + fraction * bar_width,
					.y = bar_y,
					.sizeX = bar_width * (1 - fraction),
					.sizeY = bar_height,
					.color = {1, 0, 0, 1},
				});
			}
		}

		if (show_message) {
			text.drawOnMap(lastMessage, {
				.x = float(column) + x + .525f,
				.y = float(row) + y - z - 0.225f,
				.scaleX = .75f,
				.scaleY = .75f,
				.color = {0.f, 0.f, 0.f, 1.f},
				.align = TextAlign::Center,
			});

			text.drawOnMap(lastMessage, {
				.x = float(column) + x + .5f,
				.y = float(row) + y - z - 0.25f,
				.scaleX = .75f,
				.scaleY = .75f,
				.color = {1.f, 1.f, 1.f, 1.f},
				.align = TextAlign::Center,
			});
		}

		text.drawOnMap(displayName, {
			.x = float(column) + x + .525f,
			.y = float(row) + y - z + name_offset + .025f,
			.color = {0.f, 0.f, 0.f, 1.f},
			.align = TextAlign::Center,
		});

		text.drawOnMap(displayName, {
			.x = float(column) + x + .5f,
			.y = float(row) + y - z + name_offset,
			.color = {1.f, 1.f, 1.f, 1.f},
			.align = TextAlign::Center,
		});
	}

	void ClientPlayer::stopContinuousInteraction() {
		send(ContinuousInteractionPacket());
	}

	void ClientPlayer::setContinuousInteraction(bool on, Modifiers modifiers) {
		if (on != continuousInteraction) {
			continuousInteraction = on;
			if (on)
				send(ContinuousInteractionPacket(modifiers));
			else
				send(ContinuousInteractionPacket());
		}

		continuousInteractionModifiers = modifiers;
	}

	void ClientPlayer::jump() {
		float z{};
		{
			auto lock = offset.sharedLock();
			z = offset.z;
		}
		if (std::abs(z) <= 0.001f) {
			zSpeed = getJumpSpeed();
			send(JumpPacket());
		}
	}

	const std::unordered_set<Layer> & ClientPlayer::getVisibleLayers() const {
		static std::unordered_set<Layer> main_layers {Layer::Terrain, Layer::Submerged, Layer::Objects, Layer::Highest};
		return main_layers;
	}

	bool ClientPlayer::move(Direction direction, MovementContext context) {
		const bool moved = Entity::move(direction, context);
		if (moved)
			send(MovePlayerPacket(position, direction, context.facingDirection, offset));
		return moved;
	}

	void ClientPlayer::handleMessage(const std::shared_ptr<Agent> &source, const std::string &name, std::any &data) {
		if (name == "ModuleMessage") {
			auto *buffer = std::any_cast<Buffer>(&data);
			assert(buffer != nullptr);
			const auto module_name = buffer->take<Identifier>();
			const auto message_name = buffer->take<std::string>();
			getGame().toClient().getWindow().moduleMessageBuffer(module_name, source, message_name, std::move(*buffer));
		}
	}

	void ClientPlayer::sendMessage(const std::shared_ptr<Agent> &destination, const std::string &name, std::any &data) {
		assert(destination);
		if (auto *buffer = std::any_cast<Buffer>(&data))
			getGame().toClient().client->send(AgentMessagePacket(destination->getGID(), name, *buffer));
		else
			throw std::runtime_error("Expected data to be a Buffer in ClientPlayer::sendMessage");
	}

	void ClientPlayer::setLastMessage(std::string message) {
		lastMessage = std::move(message);
		lastMessageAge = 0;
	}

	void ClientPlayer::face(Direction new_direction) {
		if (direction.exchange(new_direction) == new_direction)
			return;

		send(MovePlayerPacket(position, new_direction, new_direction));
	}
}
