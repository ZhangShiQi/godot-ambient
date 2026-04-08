/**************************************************************************/
/*  register_types.cpp                                                    */
/**************************************************************************/

#include "register_types.h"

#include "core/config/engine.h"
#include "zzz/console/limbo_console.h"

static ZConsole *limbo_console_singleton = nullptr;

void initialize_zzz_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	GDREGISTER_CLASS(ZConsole);

	limbo_console_singleton = memnew(ZConsole);
	Engine::Singleton singleton("ZConsole", limbo_console_singleton, "ZConsole");

	singleton.user_created = true;
	Engine::get_singleton()->add_singleton(singleton);
}

void uninitialize_zzz_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	if (limbo_console_singleton) {
		if (Engine::get_singleton()->has_singleton("ZConsole")) {
			Engine::get_singleton()->remove_singleton("ZConsole");
		}
		if (limbo_console_singleton->get_parent()) {
			limbo_console_singleton->get_parent()->remove_child(limbo_console_singleton);
		}
		memdelete(limbo_console_singleton);
		limbo_console_singleton = nullptr;
	}
}
