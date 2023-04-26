#pragma once

#include "entity/Animal.h"

namespace Game3 {
	class Building;

	class Dog: public Animal {
		public:
			static Identifier ID() { return {"base", "entity/dog"}; }

			static std::shared_ptr<Dog> create(Game &game) {
				auto out = std::shared_ptr<Dog>(new Dog());
				out->init(game);
				return out;
			}

			/** You do not get to kill the dog. */
			HitPoints maxHealth() const override { return INVINCIBLE; }

		protected:
			Dog(): Animal(ID()) {}
	};
}
