/**************************************************************************/
/*  register_types.cpp                                                    */
/**************************************************************************/

#include "register_types.h"

#include "core/config/engine.h"
#include "zzz/console/limbo_console.h"
#include "zzz/global_var/global_var.h"

static ZConsole *limbo_console_singleton = nullptr;
static ZGlobalVar *global_var_singleton = nullptr;

void initialize_zzz_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	GDREGISTER_CLASS(ZConsole);
	GDREGISTER_CLASS(ZGlobalVar);

	global_var_singleton = memnew(ZGlobalVar);
	Engine::Singleton global_var_engine_singleton("ZGlobalVar", global_var_singleton, "ZGlobalVar");

	global_var_engine_singleton.user_created = true;
	Engine::get_singleton()->add_singleton(global_var_engine_singleton);

	limbo_console_singleton = memnew(ZConsole);
	Engine::Singleton singleton("ZConsole", limbo_console_singleton, "ZConsole");

	singleton.user_created = true;
	Engine::get_singleton()->add_singleton(singleton);
}

void uninitialize_zzz_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	if (global_var_singleton) {
		if (Engine::get_singleton()->has_singleton("ZGlobalVar")) {
			Engine::get_singleton()->remove_singleton("ZGlobalVar");
		}
		memdelete(global_var_singleton);
		global_var_singleton = nullptr;
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
