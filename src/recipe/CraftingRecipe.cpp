#include "game/Inventory.h"
#include "recipe/CraftingRecipe.h"

namespace Game3 {
	CraftingRecipe::CraftingRecipe(Input input_, Output output_, Identifier station_type):
		input(std::move(input_)), output(std::move(output_)), stationType(std::move(station_type)) {}

	CraftingRecipe::Input CraftingRecipe::getInput(const std::shared_ptr<Game> &) {
		return input;
	}

	CraftingRecipe::Output CraftingRecipe::getOutput(const Input &, const std::shared_ptr<Game> &) {
		return output;
	}

	bool CraftingRecipe::canCraft(const std::shared_ptr<Container> &input_container) {
		auto inventory = std::dynamic_pointer_cast<Inventory>(input_container);
		if (!inventory)
			return false;

		for (const CraftingRequirement &requirement: input) {
			if (requirement.is<ItemStackPtr>()) {
				ItemStackPtr stack = requirement.get<ItemStackPtr>();
				if (0 < stack->count && inventory->count(stack) < stack->count)
					return false;
			} else {
				const auto &[attribute, count] = requirement.get<AttributeRequirement>();
				if (0 < count && inventory->countAttribute(attribute) < count)
					return false;
			}
		}

		return true;
	}

	bool CraftingRecipe::craft(const std::shared_ptr<Game> &game, const std::shared_ptr<Container> &input_container, const std::shared_ptr<Container> &output_container, std::optional<Output> &leftovers) {
		if (craft(game, input_container, output_container)) {
			leftovers.reset();
			return true;
		}

		return false;
	}

	bool CraftingRecipe::craft(const std::shared_ptr<Game> &, const std::shared_ptr<Container> &input_container, const std::shared_ptr<Container> &output_container) {
		auto inventory_in  = std::dynamic_pointer_cast<Inventory>(input_container);
		auto inventory_out = std::dynamic_pointer_cast<Inventory>(output_container);

		if (!inventory_in || !inventory_out || !canCraft(input_container))
			return false;

		// We need to check whether the output inventory has enough space to hold the outputs.
		// How we do this depends on the number of outputs as well as whether the input and output inventory are the same.

		if (inventory_in == inventory_out) {
			// If they're the same, we need to copy the inventory early to check for
			// removability and insertability, regardless of the number of outputs.
			std::unique_ptr<Inventory> copy = inventory_in->copy();
			for (const CraftingRequirement &requirement: input)
				copy->remove(requirement);

			for (const ItemStackPtr &stack: output)
				if (copy->add(stack))
					return false;

			inventory_in->replace(std::move(*copy));
			return true;
		}

		if (output.size() == 1) {
			// If there's just one output, we can check whether it's insertable without having to copy the output inventory.
			if (!inventory_out->canInsert(output[0]))
				return false;

			for (const CraftingRequirement &requirement: input)
				inventory_in->remove(requirement);

			inventory_out->add(output[0]);
			return true;
		}

		// At this point the input and output inventories are different and there are multiple outputs.
		// Make a copy of the output inventory and try to insert all the outputs in it.
		// If that succeeds, proceed to remove the ingredients from the input inventory and replace
		// the output inventory with the copy.
		auto out_copy = inventory_out->copy();
		for (const ItemStackPtr &stack: output)
			if (out_copy->add(stack))
				return false;

		for (const CraftingRequirement &requirement: input)
			inventory_in->remove(requirement);

		inventory_out->replace(std::move(*out_copy));
		return true;
	}

	void CraftingRecipe::toJSON(nlohmann::json &json) const {
		json["type"] = CraftingRecipeRegistry::ID();
		json["input"] = input;
		json["output"] = output;
		if (stationType)
			json["station"] = stationType;
	}

	CraftingRecipe CraftingRecipe::fromJSON(const std::shared_ptr<Game> &game, const nlohmann::json &json) {
		CraftingRecipe recipe;

		for (const auto &item: json.at("input"))
			recipe.input.emplace_back(CraftingRequirement::fromJSON(game, item));

		recipe.output = ItemStack::manyFromJSON(game, json.at("output"));

		if (auto iter = json.find("station"); iter != json.end())
			recipe.stationType = *iter;

		return recipe;
	}

	void to_json(nlohmann::json &json, const CraftingRecipe &recipe) {
		recipe.toJSON(json);
	}
}
