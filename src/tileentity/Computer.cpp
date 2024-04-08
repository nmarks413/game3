#include "entity/Player.h"
#include "game/ClientGame.h"
#include "packet/OpenModuleForAgentPacket.h"
#include "pipes/DataNetwork.h"
#include "realm/Realm.h"
#include "scripting/ObjectWrap.h"
#include "scripting/ScriptError.h"
#include "scripting/ScriptUtil.h"
#include "tileentity/Computer.h"
#include "ui/module/ComputerModule.h"
#include "util/Concepts.h"

namespace Game3 {
	Computer::Computer(Identifier tile_id, Position position_):
		TileEntity(std::move(tile_id), ID(), position_, true) {}

	Computer::Computer(Position position_):
		Computer("base:tile/computer"_id, position_) {}

	void Computer::init(Game &game) {
		TileEntity::init(game);
		context = std::make_shared<Context>(std::static_pointer_cast<Computer>(getSelf()));
		engine = std::make_unique<ScriptEngine>(game.shared_from_this(), [&](v8::Isolate *isolate, v8::Local<v8::ObjectTemplate> global) {
			tileEntityTemplate = makeTileEntityTemplate(isolate);
			global->Set(isolate, "TileEntity", tileEntityTemplate.Get(isolate));
		});

		v8::Isolate *isolate = engine->getIsolate();
		v8::Locker locker(isolate);
		v8::Isolate::Scope isolate_scope(isolate);
		v8::HandleScope handle_scope(isolate);

		v8::Local<v8::ObjectTemplate> instance = tileEntityTemplate.Get(isolate)->InstanceTemplate();

		instance->SetAccessor(engine->string("gid"), [](v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Value> &info) {
			auto &wrapper = WeakObjectWrap<TileEntity>::unwrap("TileEntity", info.This());
			auto locked = wrapper.object.lock();
			if (!locked) {
				info.GetReturnValue().SetNull();
			} else {
				info.GetReturnValue().Set(v8::BigInt::New(info.GetIsolate(), locked->getGID()));
			}
		});

		instance->SetAccessor(engine->string("realm"), [](v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Value> &info) {
			auto &wrapper = WeakObjectWrap<TileEntity>::unwrap("TileEntity", info.This());
			auto locked = wrapper.object.lock();
			if (!locked) {
				info.GetReturnValue().SetNull();
			} else {
				info.GetReturnValue().Set(v8::BigInt::New(info.GetIsolate(), locked->getRealm()->getID()));
			}
		});
	}

	namespace {
		/** Iterates all unique adjacent data networks. */
		template <typename Fn>
		requires Returns<Fn, void, DataNetworkPtr>
		void visitNetworks(const Place &place, Fn &&visitor) {
			std::unordered_set<DataNetworkPtr> visited_networks;

			for (const Direction direction: ALL_DIRECTIONS) {
				auto network = std::static_pointer_cast<DataNetwork>(PipeNetwork::findAt(place + direction, Substance::Data));
				if (!network || visited_networks.contains(network))
					continue;

				visited_networks.insert(network);
				visitor(network);
			}
		}

		/** Iterates all unique adjacent data networks until the given function returns true. */
		template <typename Fn>
		requires Returns<Fn, bool, DataNetworkPtr>
		void visitNetworks(const Place &place, Fn &&visitor) {
			std::unordered_set<DataNetworkPtr> visited_networks;

			for (const Direction direction: ALL_DIRECTIONS) {
				auto network = std::static_pointer_cast<DataNetwork>(PipeNetwork::findAt(place + direction, Substance::Data));
				if (!network || visited_networks.contains(network))
					continue;

				visited_networks.insert(network);
				if (visitor(network))
					return;
			}
		}

		template <typename Fn>
		requires Returns<Fn, void, TileEntityPtr>
		void visitNetwork(const DataNetworkPtr &network, Fn &&visitor) {
			RealmPtr realm = network->getRealm();
			assert(realm);

			std::unordered_set<TileEntityPtr> visited;

			auto visit = [&](const auto &set) {
				auto lock = set.sharedLock();
				for (const auto &[position, direction]: set) {
					TileEntityPtr member = realm->tileEntityAt(position);
					if (!member || visited.contains(member))
						continue;
					visited.insert(member);
					visitor(member);
				}
			};

			visit(network->getInsertions());
			visit(network->getExtractions());
		}

		template <typename Fn>
		requires Returns<Fn, bool, TileEntityPtr>
		bool visitNetwork(const DataNetworkPtr &network, Fn &&visitor) {
			RealmPtr realm = network->getRealm();
			assert(realm);

			std::unordered_set<TileEntityPtr> visited;

			auto visit = [&](const auto &set) {
				auto lock = set.sharedLock();
				for (const auto &[position, direction]: set) {
					TileEntityPtr member = realm->tileEntityAt(position);
					if (!member || visited.contains(member))
						continue;
					visited.insert(member);
					if (visitor(member))
						return true;
				}

				return false;
			};

			if (visit(network->getInsertions()))
				return true;

			return visit(network->getExtractions());
		}
	}

	void Computer::handleMessage(const std::shared_ptr<Agent> &source, const std::string &name, std::any &data) {
		if (name == "RunScript") {

			auto *buffer = std::any_cast<Buffer>(&data);
			assert(buffer);

			Token token = buffer->take<Token>();
			std::string javascript = buffer->take<std::string>();

			std::function<void(std::string_view)> print = [&](std::string_view text) {
				sendMessage(source, "ModuleMessage", ComputerModule::ID(), "ScriptPrint", token, text);
			};

			assert(engine);

			std::swap(print, engine->onPrint);

			try {
				Context context{std::static_pointer_cast<Computer>(getSelf())};

				auto result = engine->execute(javascript, true, [&](v8::Local<v8::Context> script_context) {
					v8::Local<v8::Object> g3 = engine->object({
						{"findAll", engine->makeValue(+[](const v8::FunctionCallbackInfo<v8::Value> &info) {
							auto &context = getExternal<Context>(info);
							ComputerPtr computer = context.computer.lock();
							if (!computer) {
								info.GetIsolate()->ThrowError("Computer pointer expired");
								return;
							}
							ScriptEngine &engine = *computer->engine;
							v8::Local<v8::Array> found = v8::Array::New(engine.getIsolate());
							std::unordered_set<TileEntityPtr> tile_entities;
							RealmPtr realm = computer->getRealm();
							uint32_t index = 0;

							std::unordered_set<GlobalID> gids;

							auto templ = computer->tileEntityTemplate.Get(info.GetIsolate());
							v8::Local<v8::Context> engine_context = engine.getContext();
							v8::Local<v8::Function> function = templ->GetFunction(engine_context).ToLocalChecked();

							std::function<bool(const TileEntityPtr &)> filter;
							if (info.Length() == 1 && info[0]->IsString()) {
								filter = [name = engine.string(info[0])](const TileEntityPtr &tile_entity) {
									return tile_entity->getName() == name;
								};
							} else {
								filter = [](const TileEntityPtr &) { return true; };
							}

							visitNetworks(computer->getPlace(), [&](DataNetworkPtr network) {
								visitNetwork(network, [&](const TileEntityPtr &member)  {
									GlobalID gid = member->getGID();
									if (!gids.contains(gid) && filter(member)) {
										v8::Local<v8::BigInt> gid_bigint = v8::BigInt::New(engine.getIsolate(), static_cast<int64_t>(gid));
										v8::Local<v8::Value> argv[] {gid_bigint};
										found->Set(engine.getContext(), index++, function->CallAsConstructor(engine_context, 1, argv).ToLocalChecked()).Check();
										gids.insert(gid);
									}
								});
							});

							info.GetReturnValue().Set(found);
						}, engine->wrap(&context))},
					});

					script_context->Global()->Set(script_context, engine->string("g3"), g3).Check();
				});

				if (result)
					sendMessage(source, "ModuleMessage", ComputerModule::ID(), "ScriptResult", token, engine->string(result.value()));
			} catch (const ScriptError &err) {
				sendMessage(source, "ModuleMessage", ComputerModule::ID(), "ScriptError", token, err.what(), err.line, err.column);
			}

			std::swap(print, engine->onPrint);

		} else if (name == "Echo") {

			return;

		} else {
			TileEntity::handleMessage(source, name, data);
		}
	}

	// void Computer::toJSON(nlohmann::json &json) const {
	// 	TileEntity::toJSON(json);
	// }

	bool Computer::onInteractNextTo(const PlayerPtr &player, Modifiers modifiers, const ItemStackPtr &, Hand) {
		if (modifiers.onlyAlt()) {
			RealmPtr realm = getRealm();
			realm->queueDestruction(getSelf());
			player->give(ItemStack::create(realm->getGame(), "base:item/computer"_id));
			return true;
		}

		player->send(OpenModuleForAgentPacket(ComputerModule::ID(), getGID()));

		return false;
	}

	// void Computer::absorbJSON(const GamePtr &game, const nlohmann::json &json) {
	// 	TileEntity::absorbJSON(game, json);
	// }

	void Computer::encode(Game &game, Buffer &buffer) {
		TileEntity::encode(game, buffer);
	}

	void Computer::decode(Game &game, Buffer &buffer) {
		TileEntity::decode(game, buffer);
	}

	// GamePtr Computer::getGame() const {
	// 	return TileEntity::getGame();
	// }

	v8::Global<v8::FunctionTemplate> Computer::makeTileEntityTemplate(v8::Isolate *isolate) {
		v8::Locker locker(isolate);
		v8::Isolate::Scope isolate_scope(isolate);
		v8::HandleScope handle_scope(isolate);

		v8::Local<v8::External> external = v8::External::New(isolate, this);

		v8::Local<v8::FunctionTemplate> templ = v8::FunctionTemplate::New(isolate, [](const v8::FunctionCallbackInfo<v8::Value> &info) {
			v8::Isolate *isolate = info.GetIsolate();

			if (info.Length() != 1 || !info[0]->IsBigInt()) {
				isolate->ThrowError("Expected a BigInt argument");
				return;
			}

			const GlobalID gid = info[0].As<v8::BigInt>()->Uint64Value();
			Computer &computer = getExternal<Computer>(info);
			ScriptEngine &engine = *computer.engine;
			GamePtr game = engine.game.lock();

			TileEntityPtr tile_entity = game->getAgent<TileEntity>(gid);
			if (!tile_entity) {
				isolate->ThrowError("Tile entity not found");
				return;
			}

			v8::Local<v8::Object> this_obj = info.This();

			auto *wrapper = new WeakObjectWrap<TileEntity>(tile_entity);
			wrapper->wrap(engine.getIsolate(), "TileEntity", this_obj);
		}, external);

		v8::Local<v8::ObjectTemplate> instance = templ->InstanceTemplate();

		instance->SetInternalFieldCount(2);

		instance->Set(isolate, "tell", v8::FunctionTemplate::New(isolate, [](const v8::FunctionCallbackInfo<v8::Value> &info) {
			auto &wrapper = WeakObjectWrap<TileEntity>::unwrap("TileEntity", info.This());

			Computer &computer = getExternal<Computer>(info);
			ScriptEngine &engine = *computer.engine;

			TileEntityPtr tile_entity = wrapper.object.lock();
			if (!tile_entity) {
				info.GetIsolate()->ThrowError("Tile entity pointer expired");
				return;
			}

			if (info.Length() != 1 && info.Length() != 2) {
				info.GetReturnValue().Set(engine.string("Invalid number of arguments"));
				return;
			}

			v8::Local<v8::Value> message_name = info[0];
			const GlobalID gid = tile_entity->getGID();

			tile_entity = computer.searchFor(gid);
			if (!tile_entity) {
				info.GetIsolate()->ThrowError(engine.string("Couldn't find connected tile entity with GID " + std::to_string(gid)));
				return;
			}

			v8::Local<v8::Context> script_context = engine.getContext();
			Buffer buffer;

			if (info.Length() == 2) {
				v8::MaybeLocal<v8::Object> maybe_object = info[1]->ToObject(script_context);
				if (maybe_object.IsEmpty()) {
					engine.getIsolate()->ThrowError("Third argument isn't a Buffer object");
					return;
				}

				v8::Local<v8::Object> object = maybe_object.ToLocalChecked();

				bool is_buffer = false;

				if (object->InternalFieldCount() == 2) {
					v8::Local<v8::Data> internal = object->GetInternalField(0);
					is_buffer = internal->IsValue() && engine.string(internal.As<v8::Value>()) == "Buffer";
				}

				if (!is_buffer) {
					engine.getIsolate()->ThrowError("Third argument isn't a Buffer object");
					return;
				}

				buffer = *ObjectWrap<Buffer>::unwrap("Buffer", object).object;
			}

			std::any any(std::move(buffer));
			computer.sendMessage(tile_entity, engine.string(message_name), any);

			if (Buffer *new_buffer = std::any_cast<Buffer>(&any)) {
				v8::Local<v8::Object> retval;
				if (engine.getBufferTemplate()->GetFunction(script_context).ToLocalChecked()->NewInstance(script_context).ToLocal(&retval)) {
					auto *wrapper = ObjectWrap<Buffer>::make(std::move(*new_buffer));
					wrapper->object->context = computer.getGame();
					wrapper->wrap(engine.getIsolate(), "Buffer", retval);
					info.GetReturnValue().Set(retval);
				} else {
					info.GetReturnValue().SetNull();
				}
			}
		}, external));

		return v8::Global<v8::FunctionTemplate>(isolate, templ);
	}

	TileEntityPtr Computer::searchFor(GlobalID gid) {
		TileEntityPtr out;

		visitNetworks(getPlace(), [&](const DataNetworkPtr &network) {
			return visitNetwork(network, [&](const TileEntityPtr &member) {
				if (member->getGID() == gid) {
					out = member;
					return true;
				}

				return false;
			});
		});

		return out;
	}
}
