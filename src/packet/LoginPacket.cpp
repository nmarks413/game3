#include "Log.h"
#include "game/ServerGame.h"
#include "net/LocalServer.h"
#include "net/RemoteClient.h"
#include "packet/LoginPacket.h"
#include "packet/LoginStatusPacket.h"
#include "packet/PacketError.h"

namespace Game3 {
	void LoginPacket::handle(ServerGame &game, RemoteClient &client) {
		if (!client.getPlayer()) {
			if (auto display_name = game.server->authenticate(username, token)) {
				auto player = game.server->loadPlayer(username, *display_name);
				client.setPlayer(player);
				client.send(LoginStatusPacket(true, username, *display_name, player));
				game.server->setupPlayer(client);
				return;
			}
		}

		client.send(LoginStatusPacket(false));
	}
}
