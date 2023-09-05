#pragma once

#include "data/GameDB.h"
#include "entity/ServerPlayer.h"
#include "game/Fluids.h"
#include "game/Game.h"
#include "net/RemoteClient.h"
#include "threading/MTQueue.h"

#include <mutex>

namespace Game3 {
	class LocalServer;
	class Packet;
	class RemoteClient;

	class ServerGame: public Game {
		public:
			constexpr static float GARBAGE_COLLECTION_TIME = 60.f;

			std::unordered_set<ServerPlayerPtr> players;
			std::shared_ptr<LocalServer> server;
			GameDB database{*this};
			float lastGarbageCollection = 0.f;

			ServerGame(std::shared_ptr<LocalServer> server_):
				server(std::move(server_)) {}

			void addEntityFactories() override;
			void tick() final;
			void garbageCollect();
			void broadcastTileUpdate(RealmID, Layer, const Position &, TileID);
			void broadcastFluidUpdate(RealmID, const Position &, FluidTile);
			Side getSide() const override { return Side::Server; }
			void queuePacket(std::shared_ptr<RemoteClient>, std::shared_ptr<Packet>);
			void runCommand(RemoteClient &, const std::string &, GlobalID);
			void entityChangingRealms(Entity &, const RealmPtr &new_realm, const Position &new_position);
			void entityTeleported(Entity &, MovementContext);
			void entityDestroyed(const Entity &);
			void tileEntitySpawned(const TileEntityPtr &);
			void tileEntityDestroyed(const TileEntity &);
			void remove(const ServerPlayerPtr &);
			void queueRemoval(const ServerPlayerPtr &);
			void openDatabase(std::filesystem::path);

			inline auto lockPlayersShared() { return std::shared_lock(playersMutex); }
			inline auto lockPlayersUnique() { return std::unique_lock(playersMutex); }

			template <typename P>
			void broadcast(const Place &place, const P &packet) {
				auto lock = lockPlayersShared();
				for (const auto &player: players)
					if (player->canSee(place.realm->id, place.position))
						if (auto client = player->toServer()->weakClient.lock())
							client->send(packet);
			}

		private:
			std::shared_mutex playersMutex;
			MTQueue<std::pair<std::weak_ptr<RemoteClient>, std::shared_ptr<Packet>>> packetQueue;
			MTQueue<std::weak_ptr<ServerPlayer>> playerRemovalQueue;
			double timeSinceTimeUpdate = 0.;

			void handlePacket(RemoteClient &, Packet &);
			std::tuple<bool, std::string> commandHelper(RemoteClient &, const std::string &);
	};
}
