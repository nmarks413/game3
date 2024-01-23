#pragma once

#include "game/Village.h"
#include "threading/Lockable.h"

#include <atomic>
#include <cstddef>
#include <map>

namespace SQLite {
	class Database;
}

namespace Game3 {
	class ServerGame;
	struct Place;

	class OwnsVillages {
		public:
			OwnsVillages() = default;

			size_t getNewVillageID();
			VillagePtr getVillage(size_t id) const;
			VillagePtr addVillage(ServerGame &, ChunkPosition, const Place &, const VillageOptions &);
			void saveVillages(SQLite::Database &, bool use_transaction = true);
			void loadVillages(const std::shared_ptr<ServerGame> &, SQLite::Database &);

		protected:
			virtual void associateWithRealm(const VillagePtr &, RealmID) = 0;

		private:
			Lockable<std::map<size_t, VillagePtr>> villageMap;
			std::atomic_size_t lastVillageID = 0;
	};
}
