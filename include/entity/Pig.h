#pragma once

#include "entity/Animal.h"

namespace Game3 {
	class Building;

	class Pig: public Animal {
		public:
			static Identifier ID() { return {"base", "entity/pig"}; }

			static std::shared_ptr<Pig> create(Game &game) {
				auto out = std::shared_ptr<Pig>(new Pig());
				out->init(game);
				return out;
			}

			static std::shared_ptr<Pig> fromJSON(Game &game, const nlohmann::json &json) {
				auto out = Entity::create<Pig>();
				out->absorbJSON(game, json);
				return out;
			}

			friend class Entity;

		protected:
			Pig(): Animal(ID()) {}
	};
}
