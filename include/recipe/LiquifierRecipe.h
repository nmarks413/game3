#pragma once

#include "data/Identifier.h"
#include "game/Fluids.h"
#include "item/Item.h"
#include "recipe/Recipe.h"
#include "registry/Registries.h"

#include <nlohmann/json_fwd.hpp>

namespace Game3 {
	struct LiquifierRecipe: Recipe<ItemStackPtr, FluidStack> {
		Input input;
		Output output;

		LiquifierRecipe() = default;
		LiquifierRecipe(Input, Output);

		Input getInput(const GamePtr &) override;
		Output getOutput(const Input &, const GamePtr &) override;
		/** Doesn't lock the container. */
		bool canCraft(const std::shared_ptr<Container> &) override;
		/** Doesn't lock either container. */
		bool craft(const GamePtr &, const std::shared_ptr<Container> &input_container, const std::shared_ptr<Container> &output_container, std::optional<Output> &leftovers) override;
		/** Doesn't lock either container. */
		bool craft(const GamePtr &, const std::shared_ptr<Container> &input_container, const std::shared_ptr<Container> &output_container) override;
		void toJSON(nlohmann::json &) const override;

		static LiquifierRecipe fromJSON(const GamePtr &, const nlohmann::json &);
	};

	struct LiquifierRecipeRegistry: UnnamedJSONRegistry<LiquifierRecipe> {
		static Identifier ID() { return {"base", "liquifier_recipe"}; }
		LiquifierRecipeRegistry(): UnnamedJSONRegistry(ID()) {}
	};
}
