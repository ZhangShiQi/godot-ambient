/**************************************************************************/
/*  limbo_console.h                                                       */
/**************************************************************************/

#pragma once

#include "core/object/object.h"
#include "core/templates/hash_map.h"
#include "core/variant/callable.h"
#include "scene/main/canvas_layer.h"

class Control;
class LineEdit;
class PanelContainer;
class RichTextLabel;

class ZConsole : public CanvasLayer {
	GDCLASS(ZConsole, CanvasLayer);

	struct CommandData {
		Callable callable;
		MethodInfo method_info;
		String description;
	};

	static ZConsole *singleton;

	bool enabled = true;
	bool print_to_stdout = false;
	bool pause_when_open = true;
	bool persist_history = false;
	bool disable_in_release_build = false;
  StringName toggle_action_name = "limbo_console_toggle";
	String toggle_shortcut = "QuoteLeft";
	bool attached_to_root = false;
	bool runtime_initialized = false;
	bool is_open = false;
	bool silent = false;
	bool was_already_paused = false;
	float height_ratio = 0.5f;
	float open_speed = 5.0f;
	float opacity = 0.96f;
	float open_t = 0.0f;
	int font_size = 14;

	Control *input_blocker = nullptr;
	PanelContainer *panel = nullptr;
    PanelContainer *autocomplete_panel = nullptr;
	RichTextLabel *output = nullptr;
  RichTextLabel *autocomplete_output = nullptr;
	LineEdit *entry = nullptr;
	Control *previous_gui_focus = nullptr;

	Color output_command_color;
	Color output_command_mention_color;
	Color output_error_color;
	Color output_warning_color;
	Color output_text_color;
	Color output_debug_color;

	HashMap<StringName, CommandData> commands;
	HashMap<StringName, PackedStringArray> aliases;
	HashMap<String, Callable> argument_autocomplete_sources;
	HashMap<StringName, Variant> eval_inputs;
	PackedStringArray history_entries;
	PackedStringArray autocomplete_matches;
	PackedStringArray search_matches;
	int history_index = -1;
	int search_index = -1;
	bool history_dirty = false;
	ObjectID eval_base_instance;

	static constexpr int MAX_SUBCOMMANDS = 4;
	static constexpr const char *HISTORY_FILE = "user://limbo_console_history.log";

	static void _bind_methods();
	void _notification(int p_what);
    void input(const Ref<InputEvent> &p_event) override;

	void _initialize_runtime();
  void _load_project_settings();
	void _build_gui();
	void _attach_signals();
	void _apply_visuals();
	void _show_console();
	void _hide_console();
	void _fill_entry(const String &p_text);
	void _clear_suggestions();
	void _rebuild_autocomplete();
 void _update_autocomplete_output();
 void _update_autocomplete_position();
	void _autocomplete(bool p_reverse = false);
	void _search_history();
	void _handle_entry_input(const Ref<InputEvent> &p_event);
	void _on_entry_text_submitted(const String &p_text);
	void _on_entry_text_changed(const String &p_text);

	void _history_load();
	void _history_save();
	void _history_push_entry(const String &p_entry);
	void _history_trim(int p_max_size);
	PackedStringArray _history_fuzzy_match(const String &p_query) const;

	void _register_builtin_commands();
	void _register_default_input_actions();
	MethodInfo _get_callable_method_info(const Callable &p_callable) const;
	bool _validate_callable(const Callable &p_callable) const;
	bool _validate_autocomplete_result(const Variant &p_result, const String &p_command) const;
	PackedStringArray _variant_to_string_array(const Variant &p_value) const;
	PackedStringArray _parse_command_line(const String &p_line) const;
	PackedStringArray _join_subcommands(const PackedStringArray &p_argv) const;
	PackedStringArray _expand_alias(const PackedStringArray &p_argv) const;
	bool _parse_argv(const PackedStringArray &p_argv, const CommandData &p_command, Array &r_args);
	bool _are_compatible_types(Variant::Type p_expected_type, Variant::Type p_parsed_type) const;
	Variant _parse_vector_arg(const String &p_text) const;
	String _join_argv(const PackedStringArray &p_argv, int p_from = 0, int p_to = -1) const;
	PackedStringArray _slice_argv(const PackedStringArray &p_argv, int p_from, int p_to = -1) const;
	void _add_first_token_autocomplete(const String &p_prefix, PackedStringArray &r_matches) const;
	void _add_argument_autocomplete(const PackedStringArray &p_argv, PackedStringArray &r_matches) const;
	void _add_subcommand_autocomplete(const String &p_text, PackedStringArray &r_matches) const;
	void _add_history_autocomplete(const String &p_text, PackedStringArray &r_matches) const;
	void _suggest_similar_command(const PackedStringArray &p_argv);
	void _suggest_argument_corrections(const PackedStringArray &p_argv);
	String _fuzzy_match_string(const String &p_string, int p_max_edit_distance, const PackedStringArray &p_array) const;
	int _calculate_osa_distance(const String &p_a, const String &p_b) const;
	String _format_type(Variant::Type p_type) const;
	String _bbcode_escape(const String &p_text) const;
	String _bbcode_strip(const String &p_text) const;

	void _cmd_alias(const String &p_alias, const String &p_command);
	void _cmd_aliases();
	void _cmd_commands();
	int _cmd_eval(const String &p_expression);
	void _cmd_exec(const String &p_file, bool p_silent = true);
	void _cmd_fps_max(int p_limit = -1);
	void _cmd_fullscreen();
 void _cmd_set(const String &p_name, const Variant &p_value);
	int _cmd_get(const String &p_name = String());
 int _cmd_get_all();
	int _cmd_help(const String &p_command_name = String());
	int _cmd_log(int p_num_lines = 10);
	void _cmd_quit();
	void _cmd_unalias(const String &p_alias);
	void _cmd_vsync(int p_mode = -1);
	PackedStringArray _get_global_var_names() const;

public:
	static ZConsole *get_singleton() { return singleton; }

	ZConsole();
	~ZConsole();

	void attach_to_root();

	void set_enabled(bool p_enabled);
	bool is_enabled() const { return enabled; }

	void open_console();
	void close_console();
	bool get_is_open() const { return is_open; }
	void toggle_console();

	void clear_console();
	void erase_history();
	void info(const String &p_line);
	void error(const String &p_line);
	void warn(const String &p_line);
	void debug(const String &p_line);
	void print_line(const String &p_line, bool p_stdout = false);

	void register_command(const Callable &p_callable, const StringName &p_name = StringName(), const String &p_desc = String());
	void unregister_command(const Variant &p_callable_or_name);
	bool has_command(const StringName &p_name) const;
	PackedStringArray get_command_names(bool p_include_aliases = false) const;
	String get_command_description(const StringName &p_name) const;

	void add_alias(const StringName &p_alias, const String &p_command_to_run);
	void remove_alias(const StringName &p_name);
	bool has_alias(const StringName &p_name) const;
	PackedStringArray get_aliases() const;
	PackedStringArray get_alias_argv(const StringName &p_alias) const;

	void add_argument_autocomplete_source(const StringName &p_command, int p_argument, const Callable &p_source);

	void execute_command(const String &p_command_line, bool p_silent = false);
	void execute_script(const String &p_file, bool p_silent = true);
	int usage(const StringName &p_command);

	void add_eval_input(const StringName &p_name, const Variant &p_value);
	void remove_eval_input(const StringName &p_name);
	PackedStringArray get_eval_input_names() const;
	Array get_eval_inputs() const;
	void set_eval_base_instance(Object *p_object);
	Object *get_eval_base_instance() const;

	String format_tip(const String &p_text) const;
	String format_name(const String &p_name) const;
};
