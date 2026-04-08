/**************************************************************************/
/*  global_var.h                                                           */
/**************************************************************************/

#pragma once

#include "core/object/object.h"
#include "core/templates/hash_map.h"
#include "core/variant/variant.h"

class ZGlobalVar : public Object {
    GDCLASS(ZGlobalVar, Object);

    static ZGlobalVar *singleton;
    HashMap<StringName, Variant> global_vars;

    static void _bind_methods();

public:
    static ZGlobalVar *get_singleton() { return singleton; }

    ZGlobalVar();
    ~ZGlobalVar();

    void set_value(const StringName &p_name, const Variant &p_value);
    Variant get_value(const StringName &p_name, const Variant &p_default = Variant()) const;
    bool has_value(const StringName &p_name) const;
    void remove_value(const StringName &p_name);
    void clear();
    PackedStringArray get_names() const;
    Dictionary get_all() const;
};
