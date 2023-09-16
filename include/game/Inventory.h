#pragma once

#include "game/Container.h"
#include "item/Item.h"
#include "recipe/CraftingRequirement.h"
#include "threading/Atomic.h"
#include "threading/HasMutex.h"
#include "threading/Lockable.h"

#include <atomic>
#include <functional>
#include <map>
#include <mutex>
#include <optional>

namespace Game3 {
	class Agent;
	class Buffer;
	struct CraftingRecipe;

	struct SlotRange {
		Slot min;
		Slot max;
		inline bool contains(Slot slot) const {
			return min <= slot && slot <= max;
		}
	};

	/** Inventories should be locked appropriately (see HasMutex) when something is calling Inventory methods. The Inventory will not lock itself. */
	class Inventory: public Container, public HasMutex<> {
		protected:
			Inventory() = default;
			Inventory(std::shared_ptr<Agent> owner, Slot slot_count, Slot active_slot = 0);
			Inventory(const Inventory &);
			Inventory(Inventory &&);

		public:
			std::weak_ptr<Agent> weakOwner;
			Atomic<Slot> slotCount = 0;
			Atomic<Slot> activeSlot = 0;
			/** Called before the swap occurs. The first argument always refers to this object. */
			std::function<void(Inventory &, Slot, Inventory &, Slot)> onSwap;
			/** Called before the move occurs. The first pair of arguments are always the source may or not refer to this object; the second pair of arguments are always the destination. */
			std::function<void(Inventory &, Slot from, Inventory &, Slot to, bool consumed)> onMove;

			Inventory & operator=(const Inventory &);
			Inventory & operator=(Inventory &&);

			virtual std::unique_ptr<Inventory> copy() const = 0;

			virtual ItemStack * operator[](size_t) = 0;
			virtual const ItemStack * operator[](size_t) const = 0;

			bool operator==(const Inventory &other) const { return this == &other; }

			/** Iterates over all items in the inventory until all have been iterated or the iteration function returns true. */
			virtual void iterate(const std::function<bool(const ItemStack &, Slot)> &) = 0;

			virtual ItemStack * firstItem(Slot *slot_out) = 0;

			/** If the ItemStack couldn't be inserted into the inventory, this function returns an ItemStack
			 *  containing the leftovers that couldn't be inserted. Otherwise, this function returns nothing.
			 *  The predicate will be used to determine which slots can be inserted into. */
			virtual std::optional<ItemStack> add(const ItemStack &, const std::function<bool(Slot)> &predicate, Slot start) = 0;
			virtual std::optional<ItemStack> add(const ItemStack &stack, const std::function<bool(Slot)> &predicate) { return add(stack, predicate, -1); }
			virtual std::optional<ItemStack> add(const ItemStack &stack, Slot start) { return add(stack, [](Slot) { return true; }, start); }
			virtual std::optional<ItemStack> add(const ItemStack &stack) { return add(stack, [](Slot) { return true; }, -1); }

			virtual bool canInsert(const ItemStack &) const = 0;
			virtual bool canInsert(const ItemStack &, Slot) const = 0;

			virtual bool canExtract(Slot) const = 0;

			virtual ItemCount insertable(const ItemStack &, Slot) const = 0;

			virtual bool decrease(ItemStack &, Slot, ItemCount amount = 1);

			/** Removes an item from the inventory and drops it at the owner's location. */
			virtual void drop(Slot) = 0;

			/** Like erase, but sends a packet to the server instead if run on a client. */
			virtual void discard(Slot) = 0;

			/** Swaps two slots. */
			virtual void swap(Slot, Slot) = 0;

			/** Erases a given slot. Doesn't notify the owner! */
			virtual void erase(Slot) = 0;

			/** Erases the active slot. Doesn't notify the owner! */
			virtual void erase();

			virtual bool empty() const = 0;

			/** Counts the number of an item in the inventory. */
			virtual ItemCount count(const ItemID &) const = 0;

			/** Counts the number of an item in the inventory. */
			virtual ItemCount count(const Item &) const = 0;

			/** Counts the number of an item in the inventory.
			 *  This takes ItemStack data into account but ignores the given ItemStack's count. */
			virtual ItemCount count(const ItemStack &) const = 0;

			/** Counts the number of an item in the inventory, given a predicate to select the slots read from.
			 *  This takes ItemStack data into account but ignores the given ItemStack's count. */
			virtual ItemCount count(const ItemStack &, const std::function<bool(Slot)> &) const = 0;

			/** Counts the number of items with a given attribute in the inventory. */
			virtual ItemCount countAttribute(const Identifier &) const = 0;

			virtual bool hasSlot(Slot) const = 0;

			std::shared_ptr<Agent> getOwner() const;

			virtual ItemStack & front() = 0;
			virtual const ItemStack & front() const = 0;

			/** Attempts to remove a given amount of an item from the inventory.
			 *  Returns the count removed. */
			virtual ItemCount remove(const ItemStack &) = 0;

			/** Attempts to remove a given amount of an item from the inventory.
			 *  Uses a predicate to determine which slots can be removed from.
			 *  Returns the count removed. */
			virtual ItemCount remove(const ItemStack &, const std::function<bool(Slot)> &) = 0;

			/** Attempts to remove a given amount of an item from a specific slot. Returns the count removed. */
			virtual ItemCount remove(const ItemStack &, Slot) = 0;

			virtual ItemCount remove(const CraftingRequirement &) = 0;

			virtual ItemCount remove(const AttributeRequirement &) = 0;

			virtual bool contains(Slot) const = 0;

			/** Returns whether the inventory contains at least a minimum amount of a given item. */
			virtual bool contains(const ItemStack &) const = 0;

			/** Returns the slot containing a given item ID if one exists. */
			virtual std::optional<Slot> find(const ItemID &) const = 0;

			/** Returns the first slot containing an item with the given attribute if one exists. */
			virtual std::optional<Slot> findAttribute(const Identifier &) const = 0;

			virtual ItemStack * getActive() = 0;

			virtual const ItemStack * getActive() const = 0;

			virtual void setActive(Slot, bool force) = 0;

			virtual void setActive(Slot slot) { setActive(slot, false); }

			virtual void prevSlot();

			virtual void nextSlot();

			virtual void notifyOwner() = 0;

			/** Returns the number of times a recipe can be crafted with the inventory's items.
			 *  Doesn't take the output of the recipe into account. */
			virtual ItemCount craftable(const CraftingRecipe &) const;

			static std::shared_ptr<Inventory> create(Side side, std::shared_ptr<Agent> owner, Slot slot_count, Slot active_slot = 0, std::map<Slot, ItemStack> storage = {});
			static std::shared_ptr<Inventory> create(std::shared_ptr<Agent> owner, Slot slot_count, Slot active_slot = 0, std::map<Slot, ItemStack> storage = {});

		protected:
			/** Removes every slot whose item count is zero from the storage map. */
			virtual void compact() = 0;

			Atomic<bool> suppressInventoryNotifications{false};

		public:
			struct Suppressor {
				Inventory &parent;
				bool active = true;

				explicit Suppressor(Inventory &parent_): parent(parent_) {
					parent.suppressInventoryNotifications = true;
				}

				~Suppressor() {
					if (active)
						parent.suppressInventoryNotifications = false;
				}

				void cancel(bool notify = false) {
					active = false;
					parent.suppressInventoryNotifications = false;
					if (notify)
						parent.notifyOwner();
				}
			};

			Suppressor suppress() {
				return Suppressor(*this);
			}
	};

	using InventoryPtr = std::shared_ptr<Inventory>;
}
