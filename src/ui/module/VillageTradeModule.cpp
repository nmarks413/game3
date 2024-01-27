#include "game/ClientGame.h"
#include "game/Village.h"
#include "item/Item.h"
#include "ui/gtk/UITypes.h"
#include "ui/gtk/Util.h"
#include "ui/module/VillageTradeModule.h"
#include "ui/tab/InventoryTab.h"
#include "ui/MainWindow.h"
#include "util/Util.h"

#include <format>

namespace Game3 {
	VillageTradeModule::VillageTradeModule(std::shared_ptr<ClientGame> game_, const std::any &argument):
	game(std::move(game_)),
	village(std::any_cast<VillagePtr>(argument)) {
		vbox.set_hexpand();
		villageName.set_xalign(0.5);
		villageName.set_hexpand(true);
		villageName.set_margin_top(10);
		villageName.set_margin_bottom(5);
	}

	Gtk::Widget & VillageTradeModule::getWidget() {
		return vbox;
	}

	void VillageTradeModule::reset() {
		removeChildren(vbox);
		vbox.append(villageName);
		widgets.clear();
		rows.clear();
		update();
	}

	void VillageTradeModule::update() {
		villageName.set_text(village->getName());
		populate();
	}

	std::optional<Buffer> VillageTradeModule::handleMessage(const std::shared_ptr<Agent> &, const std::string &name, std::any &data) {
		if (name == "VillageUpdate") {

			VillagePtr updated_village = std::any_cast<VillagePtr>(data);
			if (updated_village == village)
				update();

		}

		return {};
	}

	void VillageTradeModule::populate() {
		assert(village);

		const auto &resources = village->getResources();
		const auto lock = resources.sharedLock();

		for (auto iter = rows.begin(); iter != rows.end();) {
			const auto &[resource, row] = *iter;
			if (!resources.contains(resource)) {
				vbox.remove(*row);
				rows.erase(iter++);
			} else {
				++iter;
			}
		}

		for (const auto &[resource, amount]: resources) {
			if (auto iter = rows.find(resource); iter != rows.end()) {
				iter->second->update(amount);
			} else {
				auto row = std::make_unique<Row>(game, resource, amount);
				vbox.append(*row);
				rows[resource] = std::move(row);
			}
		}
	}

	VillageTradeModule::Row::Row(const ClientGamePtr &game, const Identifier &resource, double amount):
	Gtk::Box(Gtk::Orientation::HORIZONTAL), itemSlot(game, -1, nullptr) {
		itemSlot.setStack({*game, resource, ItemCount(-1)});
		update(amount);
		itemSlot.set_hexpand(false);
		quantityLabel.set_size_request(64, -1);
		quantityLabel.set_xalign(0.0);
		quantityLabel.set_margin_start(5);
		transferAmount.set_valign(Gtk::Align::CENTER);
		transferAmount.set_adjustment(Gtk::Adjustment::create(1.0, 1.0, 999.0));
		transferAmount.set_digits(0);
		buyButton.set_valign(Gtk::Align::CENTER);
		sellButton.set_valign(Gtk::Align::CENTER);
		buyButton.set_margin_start(5);
		sellButton.set_margin_start(5);
		sellButton.set_margin_end(5);
		buyButton.add_css_class("buy-sell-button");
		sellButton.add_css_class("buy-sell-button");
		set_margin_top(5);
		append(itemSlot);
		append(quantityLabel);
		append(transferAmount);
		append(buyButton);
		append(sellButton);
	}

	void VillageTradeModule::Row::update(double amount) {
		quantityLabel.set_text(std::format("× {:.2f}", amount));
	}
}
