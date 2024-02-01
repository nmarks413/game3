#pragma once

#include <functional>
#include <string>

#include <nlohmann/json_fwd.hpp>

namespace Gtk {
	class Window;
}

namespace Game3 {
	class ClientGame;
	class JSONDialog;

	struct ClientSettings {
		std::string hostname = "::1";
		uint16_t port = 12255;
		std::string username;
		bool alertOnConnection = true;
		double sizeDivisor = 1.0;

		void apply(ClientGame &) const;
		std::unique_ptr<JSONDialog> makeDialog(Gtk::Window &parent, std::function<void(const ClientSettings &)> submit) const;
	};

	void from_json(const nlohmann::json &, ClientSettings &);
	void to_json(nlohmann::json &, const ClientSettings &);
}
