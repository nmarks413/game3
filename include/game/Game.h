#pragma once

#include <chrono>
#include <filesystem>
#include <map>
#include <memory>
#include <random>
#include <unordered_map>
#include <utility>

#include <gtkmm.h>
#include <nlohmann/json.hpp>

#include "Types.h"
#include "entity/Player.h"
#include "realm/Realm.h"
#include "registry/Registries.h"
#include "registry/Registry.h"

namespace Game3 {
	class Canvas;
	class ClientGame;
	class LocalServer;
	class MainWindow;
	class Menu;
	class Player;
	class ServerGame;
	struct GhostDetails;
	struct InteractionSet;
	struct Plantable;

	class Game: public std::enable_shared_from_this<Game> {
		public:
			static constexpr const char *DEFAULT_PATH = "game.g3";
			static constexpr Version PROTOCOL_VERSION = 1;

			/** Seconds since the last tick */
			float delta = 0.f;
			std::chrono::system_clock::time_point startTime = std::chrono::system_clock::now();
			bool debugMode = true;
			/** 12 because the game starts at noon */
			float hourOffset = 12.;
			size_t cavesGenerated = 0;
			std::map<RealmType, std::shared_ptr<InteractionSet>> interactionSets;
			std::map<Identifier, std::unordered_set<std::shared_ptr<Item>>> itemsByAttribute;

			std::unordered_map<RealmID, RealmPtr> realms;
			RealmPtr activeRealm;

			RegistryRegistry registries;

			virtual ~Game() = default;

			template <typename T>
			T & registry() {
				return registries.get<T>();
			}

			template <typename T>
			const T & registry() const {
				return registries.get<const T>();
			}

			void initRegistries();
			void addItems();
			void addGhosts();
			void addEntityFactories();
			void addTileEntityFactories();
			void addRealms();
			void addPacketFactories();
			void initialSetup(const std::filesystem::path &dir = "data");
			void initEntities();
			void initInteractionSets();
			void add(std::shared_ptr<Item>);
			void add(std::shared_ptr<GhostDetails>);
			void add(EntityFactory &&);
			void add(TileEntityFactory &&);
			void add(RealmFactory &&);
			void add(PacketFactory &&);
			void add(GhostFunction &&);
			void traverseData(const std::filesystem::path &);
			void loadDataFile(const std::filesystem::path &);
			void addRecipe(const nlohmann::json &);
			RealmID newRealmID() const;
			double getTotalSeconds() const;
			double getHour() const;
			double getMinute() const;
			/** The value to divide the color values of the tilemap pixels by. Based on the time of day. */
			double getDivisor() const;

			virtual Side getSide() const = 0;

			using ServerArgument = std::variant<Canvas *, std::shared_ptr<LocalServer>>;

			static std::shared_ptr<Game> create(Side, const ServerArgument &);
			static std::shared_ptr<Game> fromJSON(Side, const nlohmann::json &, const ServerArgument &);

			ClientGame & toClient();
			const ClientGame & toClient() const;
			std::shared_ptr<ClientGame> toClientPointer();

			ServerGame & toServer();
			const ServerGame & toServer() const;
			std::shared_ptr<ServerGame> toServerPointer();

		protected:
			Game() = default;
			std::chrono::system_clock::time_point lastTime = startTime;
	};

	void to_json(nlohmann::json &, const Game &);

	using GamePtr = std::shared_ptr<Game>;
}
