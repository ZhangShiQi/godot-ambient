/**************************************************************************/
/*  global_var.cpp                                                         */
/**************************************************************************/

#include "global_var.h"

#include "core/error/error_macros.h"
#include "core/io/json.h"
#include "core/object/class_db.h"

ZGlobalVar *ZGlobalVar::singleton = nullptr;

static constexpr const char *ZGLOBALVAR_JSON_VALUE_KEY = "__zgv_value__";

static bool _coerce_value_to_type(Variant::Type p_expected_type, Variant &r_value) {
	if (r_value.get_type() == p_expected_type) {
		return true;
	}

	if (p_expected_type == Variant::FLOAT && r_value.get_type() == Variant::INT) {
		r_value = (double)(int64_t)r_value;
		return true;
	}

	if (p_expected_type == Variant::INT && r_value.get_type() == Variant::FLOAT) {
		r_value = (int64_t)(double)r_value;
		return true;
	}

	return false;
}

static Ref<ZVariant> _make_zvariant(const StringName &p_name, const Variant &p_value) {
	Ref<ZVariant> zvar;
	zvar.instantiate();
    zvar->set_name(p_name);
	zvar->set_value(p_value);
	return zvar;
}

static bool _is_valid_hierarchy_path(const String &p_path) {
	if (p_path.is_empty() || p_path.begins_with(".") || p_path.ends_with(".") || p_path.contains("..")) {
		return false;
	}
	PackedStringArray parts = p_path.split(".", false);
	for (int i = 0; i < parts.size(); i++) {
		if (parts[i].is_empty()) {
			return false;
		}
	}
	return true;
}

static bool _matches_scope(const String &p_name, const String &p_scope) {
	if (p_scope.is_empty()) {
		return true;
	}
	return p_name == p_scope || p_name.begins_with(p_scope + ".");
}

static String _join_scope(const String &p_scope, const String &p_name) {
	if (p_scope.is_empty()) {
		return p_name;
	}
	if (p_name.is_empty()) {
		return p_scope;
	}
	return p_scope + "." + p_name;
}

static bool _has_hierarchy_conflict(const HashMap<StringName, Ref<ZVariant>> &p_global_vars, const String &p_name) {
	for (const KeyValue<StringName, Ref<ZVariant>> &E : p_global_vars) {
		const String existing_name = String(E.key);
		if (existing_name == p_name) {
			continue;
		}
		if (existing_name.begins_with(p_name + ".") || p_name.begins_with(existing_name + ".")) {
			return true;
		}
	}
	return false;
}

static void _erase_scope(HashMap<StringName, Ref<ZVariant>> &r_global_vars, const String &p_scope) {
	if (p_scope.is_empty()) {
		r_global_vars.clear();
		return;
	}

	PackedStringArray names_to_remove;
  for (const KeyValue<StringName, Ref<ZVariant>> &E : r_global_vars) {
		if (_matches_scope(String(E.key), p_scope)) {
			names_to_remove.push_back(String(E.key));
		}
	}
	for (int i = 0; i < names_to_remove.size(); i++) {
		r_global_vars.erase(names_to_remove[i]);
	}
}

static Error _insert_json_value(Dictionary &r_node, const PackedStringArray &p_parts, int p_index, const Variant &p_value) {
	ERR_FAIL_INDEX_V(p_index, p_parts.size(), ERR_INVALID_PARAMETER);

	const String part = p_parts[p_index];
	if (p_index == p_parts.size() - 1) {
		Dictionary wrapped_value;
		wrapped_value[ZGLOBALVAR_JSON_VALUE_KEY] = JSON::from_native(p_value, false);
		r_node[part] = wrapped_value;
		return OK;
	}

	Dictionary child = r_node.has(part) ? Dictionary(r_node[part]) : Dictionary();
	Error err = _insert_json_value(child, p_parts, p_index + 1, p_value);
	if (err != OK) {
		return err;
	}
	r_node[part] = child;
	return OK;
}

static Dictionary _build_scope_json_tree(const Dictionary &p_values, const String &p_scope) {
	Dictionary result;
	Array keys = p_values.keys();
	for (int i = 0; i < keys.size(); i++) {
		String full_name = String(keys[i]);
		String relative_name;
		if (p_scope.is_empty()) {
			relative_name = full_name;
		} else if (full_name == p_scope) {
			relative_name = String();
		} else {
			relative_name = full_name.trim_prefix(p_scope + ".");
		}

		if (relative_name.is_empty()) {
			result[ZGLOBALVAR_JSON_VALUE_KEY] = JSON::from_native(p_values[keys[i]], false);
			continue;
		}

		PackedStringArray parts = relative_name.split(".", false);
		if (parts.is_empty()) {
			continue;
		}
		_insert_json_value(result, parts, 0, p_values[keys[i]]);
	}
	return result;
}

static Error _collect_import_values(const Dictionary &p_node, const String &p_scope, HashMap<StringName, Ref<ZVariant>> &r_values) {
	Array keys = p_node.keys();
	for (int i = 0; i < keys.size(); i++) {
		Variant key_variant = keys[i];
		ERR_FAIL_COND_V_MSG(key_variant.get_type() != Variant::STRING && key_variant.get_type() != Variant::STRING_NAME, ERR_INVALID_DATA, "JSON scope keys must be strings.");

		String key = String(key_variant);
		ERR_FAIL_COND_V_MSG(!_is_valid_hierarchy_path(key), ERR_INVALID_DATA, "Invalid hierarchy key: " + key);

		String full_name = _join_scope(p_scope, key);
		Variant value = p_node[key_variant];
		if (value.get_type() == Variant::DICTIONARY) {
			Dictionary value_dict = value;
			if (value_dict.has(ZGLOBALVAR_JSON_VALUE_KEY)) {
				Variant native_value = JSON::to_native(value_dict[ZGLOBALVAR_JSON_VALUE_KEY], false);
               r_values.insert(full_name, _make_zvariant(full_name, native_value));
			} else {
				Error err = _collect_import_values(value_dict, full_name, r_values);
				if (err != OK) {
					return err;
				}
			}
		} else {
          r_values.insert(full_name, _make_zvariant(full_name, value));
		}
	}
	return OK;
}

void ZGlobalVar::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_value", "name", "value", "create_new"), &ZGlobalVar::set_value, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("get_value", "name", "default_value"), &ZGlobalVar::get_value, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("has_value", "name"), &ZGlobalVar::has_value);
	ClassDB::bind_method(D_METHOD("remove_value", "name"), &ZGlobalVar::remove_value);
	ClassDB::bind_method(D_METHOD("clear"), &ZGlobalVar::clear);
	ClassDB::bind_method(D_METHOD("get_names"), &ZGlobalVar::get_names);
	ClassDB::bind_method(D_METHOD("get_names_in_scope", "scope"), &ZGlobalVar::get_names_in_scope, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("get_all"), &ZGlobalVar::get_all);
	ClassDB::bind_method(D_METHOD("get_all_in_scope", "scope"), &ZGlobalVar::get_all_in_scope, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("export_json", "scope", "indent"), &ZGlobalVar::export_json, DEFVAL(String()), DEFVAL("\t"));
	ClassDB::bind_method(D_METHOD("import_json", "json", "scope", "clear_scope"), &ZGlobalVar::import_json, DEFVAL(String()), DEFVAL(false));
}

void ZVariant::_bind_methods() {
   ClassDB::bind_method(D_METHOD("get_name"), &ZVariant::get_name);
	ClassDB::bind_method(D_METHOD("get_value"), &ZVariant::get_value);
    ClassDB::bind_method(D_METHOD("get_type"), &ZVariant::get_type);
	ClassDB::bind_method(D_METHOD("get_type_name"), &ZVariant::get_type_name);
	ClassDB::bind_method(D_METHOD("set_name", "name"), &ZVariant::set_name);
	ClassDB::bind_method(D_METHOD("set_value", "value"), &ZVariant::set_value);
	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "name"), "set_name", "get_name");
	ADD_PROPERTY(PropertyInfo(Variant::NIL, "value"), "set_value", "get_value");
   ADD_PROPERTY(PropertyInfo(Variant::INT, "type"), StringName(), "get_type");
}

void ZVariant::set_value(const Variant &p_value) {
   Variant typed_value = p_value;
	
	ERR_FAIL_COND_MSG(!_coerce_value_to_type(value.get_type(), typed_value),
		   "ZVariant type mismatch for '" + String(name) + "': expected " + Variant::get_type_name(value.get_type()) + ", got " + Variant::get_type_name(p_value.get_type()) + ".");

	value = typed_value;
}

Variant ZVariant::get_value() const {
	return value;
}

String ZVariant::get_type_name() const {
	return Variant::get_type_name(value.get_type());
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

void ZGlobalVar::set_value(const StringName &p_name, const Variant &p_value, bool p_create_new) {
	ERR_FAIL_COND_MSG(p_name.is_empty(), "Global variable name cannot be empty.");
	const String name = String(p_name);
	ERR_FAIL_COND_MSG(!_is_valid_hierarchy_path(name), "Invalid global variable hierarchy path: " + name);

    Ref<ZVariant> *existing = global_vars.getptr(p_name);
	if (existing && existing->is_valid()) {
     (*existing)->set_name(p_name);
		(*existing)->set_value(p_value);
		return;
	}

	ERR_FAIL_COND_MSG(!p_create_new, "Global variable does not exist: " + name);
	ERR_FAIL_COND_MSG(_has_hierarchy_conflict(global_vars, name), "Global variable hierarchy conflicts with existing path: " + name);
	global_vars.insert(p_name, _make_zvariant(p_name, p_value));
}

Ref<ZVariant> ZGlobalVar::get_value(const StringName &p_name, const Variant &p_default) const {
	const HashMap<StringName, Ref<ZVariant>>::ConstIterator it = global_vars.find(p_name);
	if (!it) {
       return _make_zvariant(p_name, p_default);
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
	return get_names_in_scope(String());
}

PackedStringArray ZGlobalVar::get_names_in_scope(const String &p_scope) const {
	ERR_FAIL_COND_V_MSG(!p_scope.is_empty() && !_is_valid_hierarchy_path(p_scope), PackedStringArray(), "Invalid global variable scope: " + p_scope);

	PackedStringArray names;
   for (const KeyValue<StringName, Ref<ZVariant>> &E : global_vars) {
		const String name = String(E.key);
		if (_matches_scope(name, p_scope)) {
			names.push_back(name);
		}
	}
	names.sort();
	return names;
}

Dictionary ZGlobalVar::get_all() const {
	return get_all_in_scope(String());
}

Dictionary ZGlobalVar::get_all_in_scope(const String &p_scope) const {
	ERR_FAIL_COND_V_MSG(!p_scope.is_empty() && !_is_valid_hierarchy_path(p_scope), Dictionary(), "Invalid global variable scope: " + p_scope);

	Dictionary values;
   for (const KeyValue<StringName, Ref<ZVariant>> &E : global_vars) {
		const String name = String(E.key);
		if (_matches_scope(name, p_scope)) {
          values[E.key] = E.value.is_valid() ? E.value->get_value() : Variant();
		}
	}
	return values;
}

String ZGlobalVar::export_json(const String &p_scope, const String &p_indent) const {
	ERR_FAIL_COND_V_MSG(!p_scope.is_empty() && !_is_valid_hierarchy_path(p_scope), String(), "Invalid global variable scope: " + p_scope);

	Dictionary scoped_values = get_all_in_scope(p_scope);
	Dictionary json_tree = _build_scope_json_tree(scoped_values, p_scope);
	return JSON::stringify(json_tree, p_indent);
}

Error ZGlobalVar::import_json(const String &p_json, const String &p_scope, bool p_clear_scope) {
	ERR_FAIL_COND_V_MSG(!p_scope.is_empty() && !_is_valid_hierarchy_path(p_scope), ERR_INVALID_PARAMETER, "Invalid global variable scope: " + p_scope);

	JSON json;
	Error err = json.parse(p_json);
	if (err != OK) {
		return err;
	}

	Variant parsed = json.get_data();
	ERR_FAIL_COND_V_MSG(parsed.get_type() != Variant::DICTIONARY, ERR_INVALID_DATA, "Imported JSON root must be a dictionary.");

	Dictionary parsed_dict = parsed;
   HashMap<StringName, Ref<ZVariant>> imported_values;
	if (parsed_dict.has(ZGLOBALVAR_JSON_VALUE_KEY)) {
		ERR_FAIL_COND_V_MSG(p_scope.is_empty(), ERR_INVALID_DATA, "A root JSON value wrapper requires a non-empty import scope.");
        imported_values.insert(p_scope, _make_zvariant(p_scope, JSON::to_native(parsed_dict[ZGLOBALVAR_JSON_VALUE_KEY], false)));
	} else {
		err = _collect_import_values(parsed_dict, p_scope, imported_values);
		if (err != OK) {
			return err;
		}
	}

   HashMap<StringName, Ref<ZVariant>> target_values = global_vars;
	if (p_clear_scope) {
		_erase_scope(target_values, p_scope);
	}

   for (const KeyValue<StringName, Ref<ZVariant>> &E : imported_values) {
		const String name = String(E.key);
		ERR_FAIL_COND_V_MSG(_has_hierarchy_conflict(target_values, name) && !target_values.has(E.key), ERR_INVALID_DATA, "Imported hierarchy conflicts with existing path: " + name);
		ERR_FAIL_COND_V_MSG(_has_hierarchy_conflict(imported_values, name), ERR_INVALID_DATA, "Imported hierarchy contains conflicting paths: " + name);
	}

	if (p_clear_scope) {
		_erase_scope(global_vars, p_scope);
	}

   for (const KeyValue<StringName, Ref<ZVariant>> &E : imported_values) {
		global_vars.insert(E.key, E.value);
	}
	return OK;
}
