#pragma once

#include <chrono>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <shared_mutex>
#include <unordered_map>
#include <utility>

#include <gtkmm.h>
#include <nlohmann/json.hpp>

#include "Types.h"
#include "entity/Player.h"
#include "net/Buffer.h"
#include "realm/Realm.h"
#include "registry/Registries.h"
#include "registry/Registry.h"
#include "threading/Lockable.h"

namespace Game3 {
	class Canvas;
	class ClientGame;
	class LocalServer;
	class MainWindow;
	class Menu;
	class Player;
	class ServerGame;
	class Tile;
	class Tileset;
	struct GhostDetails;
	struct InteractionSet;
	struct Plantable;

	class Game: public std::enable_shared_from_this<Game>, public BufferContext {
		public:
			static constexpr const char *DEFAULT_PATH = "game.g3";
			static constexpr Version PROTOCOL_VERSION = 5;

			/** Seconds since the last tick */
			float delta = 0.f;
			std::chrono::system_clock::time_point startTime = std::chrono::system_clock::now();
			bool debugMode = true;
			/** 12 because the game starts at noon */
			float hourOffset = 12.;
			double time = 0.f;
			Tick currentTick = 0;
			size_t cavesGenerated = 0;
			size_t randomTicksPerChunk = 1;

			std::map<RealmType, std::shared_ptr<InteractionSet>> interactionSets;
			std::map<Identifier, std::unordered_set<std::shared_ptr<Item>>> itemsByAttribute;

			std::unordered_map<RealmID, RealmPtr> realms;

			RegistryRegistry registries;

			Lockable<std::unordered_map<GlobalID, std::weak_ptr<Agent>>> allAgents;

			virtual ~Game() = default;

			template <typename T>
			T & registry() {
				return registries.get<T>();
			}

			template <typename T>
			const T & registry() const {
				return registries.get<const T>();
			}

			virtual void tick();
			void initRegistries();
			void addItems();
			void addGhosts();
			virtual void addEntityFactories();
			void addTileEntityFactories();
			void addRealms();
			void addPacketFactories();
			void addLocalCommandFactories();
			void addTiles();
			void addModuleFactories();
			void initialSetup(const std::filesystem::path &dir = "data");
			void initEntities();
			void initInteractionSets();
			void add(std::shared_ptr<Item>);
			void add(std::shared_ptr<GhostDetails>);
			void add(EntityFactory &&);
			void add(TileEntityFactory &&);
			void add(RealmFactory &&);
			void add(PacketFactory &&);
			void add(LocalCommandFactory &&);
			void add(GhostFunction &&);
			void add(ModuleFactory &&);
			void traverseData(const std::filesystem::path &);
			void loadDataFile(const std::filesystem::path &);
			void addRecipe(const nlohmann::json &);
			RealmID newRealmID() const;
			double getTotalSeconds() const;
			double getHour() const;
			double getMinute() const;
			/** The value to divide the color values of the tilemap pixels by. Based on the time of day. */
			double getDivisor() const;
			std::optional<TileID> getFluidTileID(FluidID);
			std::shared_ptr<Fluid> getFluid(FluidID) const;
			std::shared_ptr<Tile> getTile(const Identifier &);

			virtual Side getSide() const = 0;

			inline void clearFluidCache() { fluidCache.clear(); }

			using ServerArgument = std::variant<Canvas *, std::shared_ptr<LocalServer>>;

			static std::shared_ptr<Game> create(Side, const ServerArgument &);
			static std::shared_ptr<Game> fromJSON(Side, const nlohmann::json &, const ServerArgument &);

			ClientGame & toClient();
			const ClientGame & toClient() const;
			std::shared_ptr<ClientGame> toClientPointer();

			ServerGame & toServer();
			const ServerGame & toServer() const;
			std::shared_ptr<ServerGame> toServerPointer();

			template <typename T = Agent>
			std::shared_ptr<T> getAgent(GlobalID gid) {
				auto shared_lock = allAgents.sharedLock();
				if (auto iter = allAgents.find(gid); iter != allAgents.end()) {
					if (auto agent = iter->second.lock()) {
						if (auto out = std::dynamic_pointer_cast<T>(agent))
							return out;
					} else {
						// This should *probably* not result in a data race in practice...
						shared_lock.unlock();
						auto unique_lock = allAgents.uniqueLock();
						allAgents.erase(gid);
					}
				}

				return nullptr;
			}

		protected:
			Game() = default;
			std::chrono::system_clock::time_point lastTime = startTime;

		private:
			std::unordered_map<FluidID, TileID> fluidCache;
	};

	void to_json(nlohmann::json &, const Game &);

	using GamePtr = std::shared_ptr<Game>;
}
