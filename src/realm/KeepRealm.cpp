#include "realm/KeepRealm.h"

namespace Game3 {
	KeepRealm::KeepRealm(RealmID id_, const Position &parent_origin, Index parent_width, Index parent_height, const std::shared_ptr<Tilemap> &tilemap1_, const std::shared_ptr<Tilemap> &tilemap2_,
	                     const std::shared_ptr<Tilemap> &tilemap3_):
		Realm(id_, Realm::KEEP, tilemap1_, tilemap2_, tilemap3_), parentOrigin(parent_origin), parentWidth(parent_width), parentHeight(parent_height) {}

	KeepRealm::KeepRealm(RealmID id_, const Position &parent_origin, Index parent_width, Index parent_height, const std::shared_ptr<Tilemap> &tilemap1_):
		Realm(id_, Realm::KEEP, tilemap1_), parentOrigin(parent_origin), parentWidth(parent_width), parentHeight(parent_height) {}

	void KeepRealm::absorbJSON(const nlohmann::json &json) {
		Realm::absorbJSON(json);
		parentOrigin = json.at("parent").at("origin");
		parentWidth  = json.at("parent").at("width");
		parentHeight = json.at("parent").at("height");
	}

	void KeepRealm::toJSON(nlohmann::json &json) const {
		Realm::toJSON(json);
		json["parent"]["origin"] = parentOrigin;
		json["parent"]["width"]  = parentWidth;
		json["parent"]["height"] = parentHeight;
	}
}
