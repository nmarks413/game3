#include "game/HasPlace.h"
#include "game/TileProvider.h"
#include "realm/Realm.h"

namespace Game3 {
	ChunkRange HasPlace::getRange() const {
		auto top_left = getChunkPosition(getPosition());
		auto bottom_right = getChunkPosition(getPosition());
		const auto distance = REALM_DIAMETER / 2;
		top_left.x -= distance;
		top_left.y -= distance;
		bottom_right.x += distance;
		bottom_right.y += distance;
		return {top_left, bottom_right};
	}
}
