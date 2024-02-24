#include "entity/ClientPlayer.h"
#include "game/ClientGame.h"
#include "game/Inventory.h"
#include "graphics/RectangleRenderer.h"
#include "graphics/RendererContext.h"
#include "graphics/TextRenderer.h"
#include "net/LocalClient.h"
#include "packet/AgentMessagePacket.h"
#include "packet/ChunkRequestPacket.h"
#include "packet/ContinuousInteractionPacket.h"
#include "packet/EntityRequestPacket.h"
#include "packet/JumpPacket.h"
#include "packet/MovePlayerPacket.h"
#include "packet/TileEntityRequestPacket.h"
#include "threading/ThreadContext.h"
#include "ui/Canvas.h"
#include "ui/MainWindow.h"

namespace Game3 {
	namespace {
		constexpr Tick getMaxMessageAge(ClientGame &game) {
			return 7 * game.getWindow().settings.tickFrequency;
		}
	}

	ClientPlayer::ClientPlayer():
		Entity(ID()), Player() {}

	std::shared_ptr<ClientPlayer> ClientPlayer::create(Game &) {
		return Entity::create<ClientPlayer>();
	}

	void ClientPlayer::tick(const TickArgs &args) {
		if (lastMessageAge < std::numeric_limits<decltype(lastMessageAge)>::max())
			++lastMessageAge;

		Player::tick(args);

		Direction final_direction = direction;

		if (movingLeft && !movingRight)
			final_direction = Direction::Left;

		if (movingRight && !movingLeft)
			final_direction = Direction::Right;

		if (movingUp && !movingDown)
			final_direction = Direction::Up;

		if (movingDown && !movingUp)
			final_direction = Direction::Down;

		const MovementContext context{
			.clearOffset = false,
			.facingDirection = final_direction,
		};

		if (movingLeft && !movingRight)
			move(Direction::Left, context);

		if (movingRight && !movingLeft)
			move(Direction::Right, context);

		if (movingUp && !movingDown)
			move(Direction::Up, context);

		if (movingDown && !movingUp)
			move(Direction::Down, context);

		direction = final_direction;
	}

	void ClientPlayer::render(const RendererContext &renderers) {
		if (!isVisible())
			return;

		TextRenderer &text = renderers.text;

		Player::render(renderers);

		const auto [row, column] = getPosition();
		const auto [x, y, z] = offset.copyBase();

		const bool show_message = lastMessageAge.load() < getMaxMessageAge(game->toClient());
		const double health_offset = canShowHealthBar()? -.5 : 0;
		const double name_offset = health_offset + (show_message? -1 : 0);

		LivingEntity::render(renderers);

		if (show_message) {
			text.drawOnMap(lastMessage, {
				.x = double(column) + x + .5,
				.y = double(row) + y - z + health_offset - .25,
				.scaleX = .75,
				.scaleY = .75,
				.align = TextAlign::Center,
			});
		}

		text.drawOnMap(displayName, {
			.x = double(column) + x + .5,
			.y = double(row) + y - z + name_offset,
			.align = TextAlign::Center,
		});
	}

	void ClientPlayer::renderLighting(const RendererContext &renderers) {
		if (!isVisible())
			return;

		const auto [row, column] = getPosition();
		const auto [x, y, z] = offset.copyBase();

		constexpr static double radius = 8;

		renderers.circle.drawOnMap(RenderOptions{
			.x = column + x + .5,
			.y = row + y - z + .5,
			.sizeX = radius,
			.sizeY = radius,
			.color = {1, 1, 1, 1},
		}, 0.5);
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
		if (getRidden())
			return;

		double z{};
		{
			auto lock = offset.sharedLock();
			z = offset.z;
		}
		if (std::abs(z) <= 0.001) {
			{
				auto lock = velocity.uniqueLock();
				velocity.z = getJumpSpeed();
			}
			getGame().toClient().playSound("base:sound/jump");
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

	void ClientPlayer::addMoney(MoneyCount to_add) {
		setMoney(money + to_add);
	}

	bool ClientPlayer::removeMoney(MoneyCount to_remove) {
		if (money < to_remove)
			return false;

		setMoney(money - to_remove);
		return true;
	}

	void ClientPlayer::setMoney(MoneyCount new_value) {
		Entity::setMoney(new_value);
		auto shared = getShared();
		if (getGame().toClient().getPlayer() == shared) {
			getRealm()->getGame().toClient().signalPlayerMoneyUpdate().emit(getShared());
		}
	}

	void ClientPlayer::movedToNewChunk(const std::optional<ChunkPosition> &old_position) {
		if (auto realm = weakRealm.lock()) {
			std::set<ChunkPosition> chunk_requests;
			std::vector<EntityRequest> entity_requests;
			std::vector<TileEntityRequest> tile_entity_requests;

			auto process_chunk = [&](ChunkPosition chunk_position) {
				chunk_requests.insert(chunk_position);

				if (auto entities = realm->getEntities(chunk_position)) {
					auto lock = entities->sharedLock();
					for (const auto &entity: *entities)
						entity_requests.emplace_back(*entity);
				}

				if (auto tile_entities = realm->getTileEntities(chunk_position)) {
					auto lock = tile_entities->sharedLock();
					for (const auto &tile_entity: *tile_entities)
						tile_entity_requests.emplace_back(*tile_entity);
				}
			};

			if (old_position) {
				const ChunkRange old_range(*old_position);
				ChunkRange(getChunk()).iterate([&process_chunk, old_range](ChunkPosition chunk_position) {
					if (!old_range.contains(chunk_position))
						process_chunk(chunk_position);
				});
			} else {
				ChunkRange(getChunk()).iterate(process_chunk);
			}

			if (!chunk_requests.empty())
				send(ChunkRequestPacket(*realm, chunk_requests));

			if (!entity_requests.empty())
				send(EntityRequestPacket(realm->id, std::move(entity_requests)));

			if (!tile_entity_requests.empty())
				send(TileEntityRequestPacket(realm->id, std::move(tile_entity_requests)));
		}

		Entity::movedToNewChunk(old_position);
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
			getGame().toClient().getClient()->send(AgentMessagePacket(destination->getGID(), name, *buffer));
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
