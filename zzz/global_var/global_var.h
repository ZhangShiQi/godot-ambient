/**************************************************************************/
/*  global_var.h                                                           */
/**************************************************************************/

#pragma once

#include "core/object/object.h"
#include "core/object/ref_counted.h"
#include "core/templates/hash_map.h"
#include "core/variant/variant.h"


// 代表一个全局变量的类，包含变量的名称和值。
class ZVariant : public RefCounted {
	GDCLASS(ZVariant, RefCounted);

    StringName name;
	Variant value;

	static void _bind_methods();

public:
	void set_name(const StringName &p_name) { name = p_name; }
	StringName get_name() const { return name; }
	void set_value(const Variant &p_value);
	Variant get_value() const;

	_FORCE_INLINE_ int get_type() const { return value.get_type(); }
	String get_type_name() const;
};


class ZGlobalVar : public Object {
	GDCLASS(ZGlobalVar, Object);

	static ZGlobalVar *singleton;

	// 存储全局变量的哈希表，键为变量名称，值为 ZVariant 对象。
  HashMap<StringName, Ref<ZVariant>> global_vars;

	static void _bind_methods();

public:
	static ZGlobalVar *get_singleton() { return singleton; }

	ZGlobalVar();
	~ZGlobalVar();

	// 设置全局变量的值，如果变量不存在且 p_create_new 为 true，则创建一个新的变量; 如果变量不存在且 p_create_new 为 false，则报错。
	void set_value(const StringName &p_name, const Variant &p_value, bool p_create_new = true);
   Ref<ZVariant> get_value(const StringName &p_name, const Variant &p_default = Variant()) const;

	bool has_value(const StringName &p_name) const;
	void remove_value(const StringName &p_name);
	void clear();

	PackedStringArray get_names() const;
	PackedStringArray get_names_in_scope(const String &p_scope = String()) const;

	Dictionary get_all() const;
	Dictionary get_all_in_scope(const String &p_scope = String()) const;

	String export_json(const String &p_scope = String(), const String &p_indent = "\t") const;
	Error import_json(const String &p_json, const String &p_scope = String(), bool p_clear_scope = false);
};
