/**************************************************************************/
/*  register_types.cpp                                                    */
/**************************************************************************/

#include "register_types.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "zzz/console/limbo_console.h"
#include "zzz/global_var/global_var.h"

static ZConsole *global_console_singleton = nullptr;
static ZGlobalVar *global_var_singleton = nullptr;



void init_console_singleton() {
	GLOBAL_DEF("zzz/console/enabled", true);
	GLOBAL_DEF("zzz/console/persist_history", true);
	GLOBAL_DEF("zzz/console/pause_when_open", true);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "zzz/console/height_ratio", PROPERTY_HINT_RANGE, "0.2,1.0,0.01"), 0.5);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "zzz/console/open_speed", PROPERTY_HINT_RANGE, "0.1,20.0,0.1"), 5.0);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "zzz/console/opacity", PROPERTY_HINT_RANGE, "0.1,1.0,0.01"), 0.96);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "zzz/console/font_size", PROPERTY_HINT_RANGE, "8,64,1"), 14);
	GLOBAL_DEF("zzz/console/toggle_action", String("limbo_console_toggle"));
	GLOBAL_DEF("zzz/console/toggle_shortcut", String("QuoteLeft"));

	global_console_singleton = memnew(ZConsole);
	Engine::Singleton singleton("ZConsole", global_console_singleton, "ZConsole");
	singleton.user_created = true;
	Engine::get_singleton()->add_singleton(singleton);
}

// 
void init_global_var_singleton() {
	global_var_singleton = memnew(ZGlobalVar);
	Engine::Singleton global_var_engine_singleton("ZGlobalVar", global_var_singleton, "ZGlobalVar");
	global_var_engine_singleton.user_created = true;
	Engine::get_singleton()->add_singleton(global_var_engine_singleton);
}

void initialize_zzz_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	GDREGISTER_CLASS(ZConsole);
	GDREGISTER_CLASS(ZGlobalVar);

	init_console_singleton();
	init_global_var_singleton();
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

	if (global_console_singleton) {
		if (Engine::get_singleton()->has_singleton("ZConsole")) {
			Engine::get_singleton()->remove_singleton("ZConsole");
		}

		// 场景树的单例在销毁时会自动删除，所以这里不需要手动删除。
		//memdelete(limbo_console_singleton);
		global_console_singleton = nullptr;
	}
}
