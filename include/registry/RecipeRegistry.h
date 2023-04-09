#pragma once

#include "item/Item.h"
#include "recipe/CraftingRecipe.h"
#include "registry/Registry.h"

namespace Game3 {
	struct CraftingRecipeRegistry: UnnamedRegistry<CraftingRecipe> {
		static Identifier ID() { return {"base", "crafting_recipe_registry"}; }
		CraftingRecipeRegistry(): UnnamedRegistry(ID()) {}
	};
}
