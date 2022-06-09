#include "ui/tab/Tab.h"

namespace Game3 {
	class MainWindow;

	class InventoryTab: public Tab {
		public:
			MainWindow &mainWindow;

			InventoryTab() = delete;
			InventoryTab(MainWindow &);

			InventoryTab(const InventoryTab &) = delete;
			InventoryTab(InventoryTab &&) = delete;

			InventoryTab & operator=(const InventoryTab &) = delete;
			InventoryTab & operator=(InventoryTab &&) = delete;

			Gtk::Widget & getWidget() override { return scrolled; }
			Glib::ustring getName() override { return "Inventory"; }
			void onResize(const std::shared_ptr<Game> &) override;
			void update(const std::shared_ptr<Game> &) override;
			void reset(const std::shared_ptr<Game> &) override;
			void setExternalInventory(const Glib::ustring &name, const std::shared_ptr<Inventory> &);
			std::shared_ptr<Inventory> getExternalInventory() const { return externalInventory; }

		private:
			constexpr static int TILE_MARGIN = 2;
			constexpr static int TILE_SIZE = 100 - 2 * TILE_MARGIN;

			Gtk::ScrolledWindow scrolled;
			Gtk::Grid playerGrid;
			Gtk::Grid externalGrid;
			Gtk::Label externalLabel;
			Gtk::Box vbox {Gtk::Orientation::VERTICAL};
			Gtk::Box hbox {Gtk::Orientation::HORIZONTAL};
			Gtk::PopoverMenu popoverMenu;
			std::vector<std::unique_ptr<Gtk::Widget>> gridWidgets;
			int lastGridWidth = 0;
			std::shared_ptr<Inventory> externalInventory;
			Glib::ustring externalName;

			/** We can't store state in a popover, so we have to store it here. */
			std::shared_ptr<Game> lastGame;
			Slot lastSlot = -1;
			bool lastExternal = false;

			int gridWidth() const;
			void leftClick(const std::shared_ptr<Game> &, Gtk::Label &, int click_count, Slot, bool external, double x, double y);
			void rightClick(const std::shared_ptr<Game> &, Gtk::Label &, int click_count, Slot, bool external, double x, double y);
			Gtk::Label & getDraggedItem();
	};
}
