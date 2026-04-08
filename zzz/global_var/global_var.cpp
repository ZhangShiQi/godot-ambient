/**************************************************************************/
/*  global_var.cpp                                                         */
/**************************************************************************/

#include "global_var.h"

#include "core/error/error_macros.h"
#include "core/object/class_db.h"

ZGlobalVar *ZGlobalVar::singleton = nullptr;

void ZGlobalVar::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_value", "name", "value"), &ZGlobalVar::set_value);
    ClassDB::bind_method(D_METHOD("get_value", "name", "default_value"), &ZGlobalVar::get_value, DEFVAL(Variant()));
    ClassDB::bind_method(D_METHOD("has_value", "name"), &ZGlobalVar::has_value);
    ClassDB::bind_method(D_METHOD("remove_value", "name"), &ZGlobalVar::remove_value);
    ClassDB::bind_method(D_METHOD("clear"), &ZGlobalVar::clear);
    ClassDB::bind_method(D_METHOD("get_names"), &ZGlobalVar::get_names);
    ClassDB::bind_method(D_METHOD("get_all"), &ZGlobalVar::get_all);
}

ZGlobalVar::ZGlobalVar() {
    ERR_FAIL_COND_MSG(singleton != nullptr, "ZGlobalVar singleton already exists.");
    singleton = this;
}

ZGlobalVar::~ZGlobalVar() {
    if (singleton == this) {
        singleton = nullptr;
    }
}

void ZGlobalVar::set_value(const StringName &p_name, const Variant &p_value) {
    ERR_FAIL_COND_MSG(p_name.is_empty(), "Global variable name cannot be empty.");
    global_vars.insert(p_name, p_value);
}

Variant ZGlobalVar::get_value(const StringName &p_name, const Variant &p_default) const {
    const HashMap<StringName, Variant>::ConstIterator it = global_vars.find(p_name);
    if (!it) {
        return p_default;
    }

    return it->value;
}

bool ZGlobalVar::has_value(const StringName &p_name) const {
    return global_vars.has(p_name);
}

void ZGlobalVar::remove_value(const StringName &p_name) {
    if (p_name.is_empty()) {
        return;
    }

    global_vars.erase(p_name);
}

void ZGlobalVar::clear() {
    global_vars.clear();
}

PackedStringArray ZGlobalVar::get_names() const {
    PackedStringArray names;
    for (const KeyValue<StringName, Variant> &E : global_vars) {
        names.push_back(String(E.key));
    }
    names.sort();
    return names;
}

Dictionary ZGlobalVar::get_all() const {
    Dictionary values;
    for (const KeyValue<StringName, Variant> &E : global_vars) {
        values[E.key] = E.value;
    }
    return values;
}
