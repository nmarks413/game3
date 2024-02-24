#pragma once

#include "entity/Merchant.h"
#include "entity/Worker.h"

namespace Game3 {
	/** Lives in a town and produces tools during the day. */
	class Blacksmith: public Worker, public Merchant {
		public:
			static Identifier ID() { return {"base", "entity/blacksmith"}; }
			constexpr static double BUYING_TIME = 5.;
			constexpr static double CRAFTING_TIME = 5.;
			/** The number of iron bars, gold bars and diamonds that the blacksmith will try to maintain. */
			constexpr static ItemCount RESOURCE_TARGET = 64;

			static std::shared_ptr<Blacksmith> create(Game &);
			static std::shared_ptr<Blacksmith> create(Game &, RealmID overworld_realm, RealmID house_realm, Position house_position, std::shared_ptr<Building> keep_);
			static std::shared_ptr<Blacksmith> fromJSON(Game &, const nlohmann::json &);

			void toJSON(nlohmann::json &) const override;
			void absorbJSON(Game &, const nlohmann::json &) override;
			bool onInteractNextTo(const std::shared_ptr<Player> &, Modifiers, ItemStack *, Hand) override;
			void tick(const TickArgs &) override;
			std::string getName() const override { return "Blacksmith"; }
			void encode(Buffer &) override;
			void decode(Buffer &) override;

			friend class Entity;

		protected:
			double actionTime = 0.;

			Blacksmith();
			Blacksmith(RealmID overworld_realm, RealmID house_realm, Position house_position, std::shared_ptr<Building> keep_);

			void interact(const Position &);

		private:
			ItemCount coalNeeded = 0;
			ItemCount ironOreNeeded = 0;
			ItemCount goldOreNeeded = 0;
			ItemCount diamondOreNeeded = 0;

			void wakeUp();
			void buyResources();
			void goToForge();
			void craftTools();
			void goToCounter();
			void startSelling();
	};
}
