#pragma once

#include "game/Inventory.h"

#include <functional>
#include <map>
#include <memory>
#include <optional>

namespace Game3 {
	class StorageInventory: public Inventory {
		public:
			using Storage = std::map<Slot, ItemStackPtr>;

		protected:
			Lockable<Storage> storage;

			StorageInventory() = default;
			StorageInventory(std::shared_ptr<Agent> owner, Slot slot_count, Slot active_slot, InventoryID, Storage);
			StorageInventory(const StorageInventory &);
			StorageInventory(StorageInventory &&);

		public:
			StorageInventory & operator=(const StorageInventory &);
			StorageInventory & operator=(StorageInventory &&);

			ItemStackPtr operator[](Slot) const override;

			void set(Slot, ItemStackPtr) override;

			Slot getSlotCount() const override;
			void setSlotCount(Slot) override;

			void iterate(const std::function<bool(const ItemStackPtr &, Slot)> &) const override;

			ItemStackPtr firstItem(Slot *slot_out) override;
			ItemStackPtr firstItem(Slot *slot_out, const std::function<bool(const ItemStackPtr &, Slot)> &) override;

			bool canInsert(const ItemStackPtr &, const std::function<bool(Slot)> &) const override;
			bool canInsert(const ItemStackPtr &, Slot) const override;

			bool canExtract(Slot) const override;

			ItemCount insertable(const ItemStackPtr &, Slot) const override;

			/** Counts the number of an item in the inventory. */
			ItemCount count(const ItemID &) const override;

			/** Counts the number of an item in the inventory. */
			ItemCount count(const Item &) const override;

			/** Counts the number of an item in the inventory. This takes ItemStack data into account but ignores the given ItemStack's count. */
			ItemCount count(const ItemStackPtr &) const override;

			/** Counts the number of an item in the inventory, given a predicate to select the slots read from.
			 *  This takes ItemStack data into account but ignores the given ItemStack's count. */
			ItemCount count(const ItemStackPtr &, const std::function<bool(Slot)> &) const override;

			/** Counts the number of items with a given attribute in the inventory. */
			ItemCount countAttribute(const Identifier &) const override;

			bool hasSlot(Slot) const override;

			bool empty() const override;

			ItemStackPtr front() const override;

			bool contains(Slot) const override;

			/** Returns whether the inventory contains at least a minimum amount of a given item, given a predicate. */
			bool contains(const ItemStackPtr &, const Predicate &) const override;

			/** Returns the slot containing a given item ID if one exists. */
			std::optional<Slot> find(const ItemID &, const Predicate &) const override;

			/** Returns the first slot containing an item with the given attribute if one exists. */
			std::optional<Slot> findAttribute(const Identifier &, const Predicate &) const override;

			ItemStackPtr getActive() const override;

			void prevSlot() override;
			void nextSlot() override;

			Slot slotsOccupied() const override;

			void replace(const Inventory &) override;
			void replace(Inventory &&) override;

			inline auto & getStorage() { return storage; }
			inline const auto & getStorage() const { return storage; }
			inline void setStorage(Lockable<Storage> &&new_storage) { storage = std::move(new_storage); }
			inline void setStorage(Storage &&new_storage) { storage = std::move(new_storage); }

		protected:
			Atomic<Slot> slotCount = 0;

			/** Removes every slot whose item count is zero from the storage map. */
			void compact() override;
	};
}
