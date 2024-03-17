#include <iostream>

#include "Log.h"
#include "entity/ClientPlayer.h"
#include "game/ClientGame.h"
#include "game/ClientInventory.h"
#include "item/Tool.h"
#include "packet/MoveSlotsPacket.h"
#include "packet/SetHeldItemPacket.h"
#include "packet/UseItemPacket.h"
#include "ui/MainWindow.h"
#include "ui/gtk/NumericEntry.h"
#include "ui/gtk/DragSource.h"
#include "ui/gtk/Util.h"
#include "ui/tab/InventoryTab.h"
#include "ui/module/InventoryModule.h"
#include "ui/module/Module.h"
#include "util/Util.h"

namespace Game3 {
	InventoryTab::InventoryTab(MainWindow &main_window): Tab(main_window.notebook), mainWindow(main_window) {
		scrolled.set_child(vbox);
		scrolled.set_hexpand();
		scrolled.set_vexpand();

		vbox.set_spacing(0);
		actionBox.set_hexpand(true);
		actionBox.set_halign(Gtk::Align::CENTER);
		actionBox.set_margin_top(5);
		actionBox.set_margin_bottom(5);

		motion = Gtk::EventControllerMotion::create();
		motion->signal_motion().connect([this](double, double) {
			lastModifiers = Modifiers(motion->get_current_event_state());
		});
		scrolled.add_controller(motion);

		auto use_function = [this](Slot slot, Modifiers) {
			lastGame.load()->getPlayer()->send(UseItemPacket(slot, Modifiers{}));
		};

		auto hold_left_function = [this](Slot slot, Modifiers) {
			lastGame.load()->getPlayer()->send(SetHeldItemPacket(true, slot));
		};

		auto hold_right_function = [this](Slot slot, Modifiers) {
			lastGame.load()->getPlayer()->send(SetHeldItemPacket(false, slot));
		};

		auto drop_function = [this](Slot slot, Modifiers) {
			lastGame.load()->getPlayer()->getInventory(0)->drop(slot);
		};

		auto discard_function = [this](Slot slot, Modifiers modifiers) {
			InventoryPtr inventory = lastGame.load()->getPlayer()->getInventory(0);
			auto lock = inventory->uniqueLock();
			if (modifiers.onlyShift()) {
				ItemStackPtr stack = (*inventory)[slot];
				if (!stack)
					return;
				std::vector<Slot> slots_to_remove;
				inventory->iterate([&](const ItemStackPtr &iterated, Slot iterated_slot) -> bool {
					if (iterated->canMerge(*stack))
						slots_to_remove.push_back(iterated_slot);
					return false;
				});
				for (Slot slot_to_remove: slots_to_remove)
					inventory->discard(slot_to_remove);
			} else {
				inventory->discard(slot);
			}
		};

		initAction(holdLeftAction,  "pan-start-symbolic",  "Hold Left",  hold_left_function);
		initAction(holdRightAction, "pan-end-symbolic",    "Hold Right", hold_right_function);
		initAction(dropAction,      "pan-down-symbolic",   "Drop",       drop_function);
		initAction(discardAction,   "user-trash-symbolic", "Discard",    discard_function);

		group = Gio::SimpleActionGroup::create();

		group->add_action("use", [this, use_function] {
			if (lastGame.load())
				use_function(lastSlot, Modifiers{});
			else
				WARN("{}:{}: no lastGame", __FILE__, __LINE__);
		});

		group->add_action("hold_left", [this, hold_left_function] {
			if (lastGame.load())
				hold_left_function(lastSlot, Modifiers{});
			else
				WARN("{}:{}: no lastGame", __FILE__, __LINE__);
		});

		group->add_action("hold_right", [this, hold_right_function] {
			if (lastGame.load())
				hold_right_function(lastSlot, Modifiers{});
			else
				WARN("{}:{}: no lastGame", __FILE__, __LINE__);
		});

		group->add_action("drop", [this, drop_function] {
			if (lastGame.load())
				drop_function(lastSlot, Modifiers{});
			else
				WARN("{}:{}: no lastGame", __FILE__, __LINE__);
		});

		group->add_action("discard", [this, discard_function] {
			if (lastGame.load())
				discard_function(lastSlot, Modifiers{});
			else
				WARN("{}:{}: no lastGame", __FILE__, __LINE__);
		});

		mainWindow.insert_action_group("inventory_popup", group);

		vbox.set_hexpand();
		vbox.set_vexpand();
	}

	InventoryTab::~InventoryTab() = default;

	void InventoryTab::onResize(const std::shared_ptr<ClientGame> &) {
		const int new_width = scrolled.get_width();
		if (new_width != lastWidth && currentModule) {
			lastWidth = new_width;
			currentModule->onResize(new_width);
		}
	}

	void InventoryTab::update(const std::shared_ptr<ClientGame> &game) {
		if (!game || !game->getPlayer())
			return;

		lastGame = game;

		mainWindow.queue([this, game] {
			updateInventory(game);
			auto lock = currentModule.trySharedLock();
			if (currentModule)
				currentModule->update();
		});
	}

	void InventoryTab::reset(const std::shared_ptr<ClientGame> &game) {
		if (!game) {
			clear();
			lastGame = nullptr;
			if (inventoryModule) {
				vbox.remove(inventoryModule->getWidget());
				vbox.remove(actionBox);
				inventoryModule.reset();
			}
			return;
		}

		if (!game->getPlayer())
			return;

		lastGame = game;

		mainWindow.queue([this, game] {
			clear();
			updateInventory(game);
			auto lock = currentModule.sharedLock();
			if (currentModule)
				currentModule->reset();
		});
	}

	void InventoryTab::clear() {
		if (inventoryModule)
			inventoryModule->reset();
	}

	void InventoryTab::populate(std::shared_ptr<ClientInventory> inventory) {
		if (!inventoryModule)
			return;

		if (inventory != inventoryModule->getInventory()) {
			inventoryModule->setInventory(inventory);
		} else {
			inventoryModule->update();
		}

		updatePlayerClasses(lastGame);
	}

	void InventoryTab::setModule(std::shared_ptr<Module> module_) {
		assert(module_);
		removeModule();
		auto lock = currentModule.uniqueLock();
		currentModule.std::shared_ptr<Module>::operator=(std::move(module_));
		vbox.append(currentModule->getWidget());
		currentModule->onResize(vbox.get_width());
		currentModule->reset();
	}

	Module & InventoryTab::getModule() const {
		assert(currentModule);
		return *currentModule;
	}

	Module * InventoryTab::getModule(std::shared_lock<DefaultMutex> &lock) {
		if (currentModule)
			lock = currentModule.sharedLock();
		return currentModule.get();
	}

	Module * InventoryTab::getModule(std::unique_lock<DefaultMutex> &lock) {
		if (currentModule)
			lock = currentModule.uniqueLock();
		return currentModule.get();
	}

	void InventoryTab::removeModule() {
		auto lock = currentModule.uniqueLock();
		if (currentModule) {
			vbox.remove(currentModule->getWidget());
			currentModule.reset();
		}
	}

	GlobalID InventoryTab::getExternalGID() const {
		throw std::logic_error("InventoryTab::getExternalGID() needs to be replaced");
	}

	void InventoryTab::slotClicked(Slot slot, bool is_right_click, Modifiers modifiers) {
		if (is_right_click) {
			lastSlot = slot;
		} else {
			leftClick(slot, modifiers);
		}
	}

	void InventoryTab::slotDoubleClicked(Slot slot) {
		InventoryPtr inventory;
		PlayerPtr player;
		{
			if (!lastGame.load())
				return;
			player = lastGame.load()->getPlayer();
			if (player)
				inventory = player->getInventory(0);
		}

		{
			auto lock = inventory->sharedLock();
			if (!inventory->contains(slot))
				return;
		}

		player->send(UseItemPacket(slot, Modifiers{}));
	}

	void InventoryTab::activeSlotSet() {
		updatePlayerClasses(lastGame);
	}

	int InventoryTab::gridWidth() const {
		return scrolled.get_width() / (TILE_SIZE + 2 * TILE_MARGIN);
	}

	void InventoryTab::leftClick(Slot slot, Modifiers modifiers) {
		mainWindow.onBlur();

		if (!lastGame.load())
			return;

		if (modifiers.onlyShift()) {
			shiftClick(lastGame, slot);
		} else {
			lastGame.load()->getPlayer()->getInventory(0)->setActive(slot, false);
		}
	}

	void InventoryTab::shiftClick(const std::shared_ptr<ClientGame> &game, Slot slot) {
		if (!game)
			return;

		InventoryPtr inventory = game->getPlayer()->getInventory(0);
		if (!inventory || !inventory->contains(slot))
			return;

		std::unique_lock<DefaultMutex> lock;
		Module *module_ = getModule(lock);
		if (!module_)
			return;

		if (module_->handleShiftClick(inventory, slot))
			return;

		std::shared_ptr<InventoryModule> external_module = module_->getPrimaryInventoryModule();
		if (!external_module)
			return;

		InventoryPtr external_inventory = external_module->getInventory();
		if (!external_inventory)
			return;

		AgentPtr owner = external_inventory->weakOwner.lock();
		if (!owner)
			return;

		game->getPlayer()->send(MoveSlotsPacket(game->getPlayer()->getGID(), owner->getGID(), slot, -1, 0, external_inventory->index));
	}

	void InventoryTab::updatePlayerClasses(const std::shared_ptr<ClientGame> &game) {
		if (!inventoryModule)
			return;

		const Slot active_slot = game->getPlayer()->getInventory(0)->activeSlot;

		inventoryModule->removeCSSClass("active-slot");
		inventoryModule->addCSSClass("active-slot", active_slot);
	}

	void InventoryTab::gmenuSetup(InventoryModule &module_, Glib::RefPtr<Gio::Menu> gmenu, Slot slot, const ItemStackPtr &stack) {
		if (!stack || !stack->item->populateMenu(module_.getInventory(), slot, stack, gmenu, group))
			gmenu->append("_Use", "inventory_popup.use");
		gmenu->append("Hold (_Left)", "inventory_popup.hold_left");
		gmenu->append("Hold (_Right)", "inventory_popup.hold_right");
		gmenu->append("_Drop", "inventory_popup.drop");
		gmenu->append("D_iscard", "inventory_popup.discard");
	}

	void InventoryTab::updateInventory(const ClientGamePtr &game) {
		if (const InventoryPtr inventory = game->getPlayer()->getInventory(0)) {
			auto client_inventory = std::static_pointer_cast<ClientInventory>(inventory);

			if (!inventoryModule) {
				inventoryModule.emplace(game, client_inventory, this, sigc::mem_fun(*this, &InventoryTab::gmenuSetup));
				inventoryModule->setShowLabel(false);
				vbox.prepend(inventoryModule->getWidget());
				vbox.prepend(actionBox);
			} else {
				inventoryModule->setInventory(client_inventory);
			}

			populate(client_inventory);
		}
	}

	void InventoryTab::initAction(Gtk::Image &action, const Glib::ustring &icon, const Glib::ustring &tooltip, std::function<void(Slot, Modifiers)> function) {
		action.set_margin_start(5);
		action.set_margin_end(5);
		action.set_from_icon_name(icon);
		action.set_icon_size(Gtk::IconSize::LARGE);
		action.set_tooltip_text(tooltip);

		auto target = Gtk::DropTarget::create(Glib::Value<DragSource>::value_type(), Gdk::DragAction::MOVE);
		target->signal_drop().connect([this, function = std::move(function)](const Glib::ValueBase &base, double, double) {
			if (base.gobj()->g_type != Glib::Value<DragSource>::value_type())
				return false;

			const auto &value = static_cast<const Glib::Value<DragSource> &>(base);
			const DragSource source = value.get();
			if (auto game = lastGame.load(); game && game->getPlayer() && *source.inventory == *game->getPlayer()->getInventory(0))
				function(source.slot, lastModifiers);
			return true;
		}, false);

		action.add_controller(target);
		actionBox.append(action);
	}
}
