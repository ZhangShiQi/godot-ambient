/**************************************************************************/
/*  limbo_console.cpp                                                     */
/**************************************************************************/

#include "limbo_console.h"

#include "core/config/project_settings.h"
#include "core/error/error_macros.h"
#include "core/input/input.h"
#include "core/input/input_event.h"
#include "core/input/input_map.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/math/expression.h"
#include "core/os/keyboard.h"
#include "core/os/os.h"
#include "core/string/print_string.h"
#include "core/string/translation.h"
#include "scene/gui/box_container.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/rich_text_label.h"
#include "scene/main/scene_tree.h"
#include "scene/main/viewport.h"
#include "scene/main/window.h"
#include "servers/display/display_server.h"

ZConsole *ZConsole::singleton = nullptr;

static String _autocomplete_key(const StringName &p_command, int p_argument) {
	return String(p_command) + "|" + itos(p_argument);
}

static bool _is_debug_build() {
#ifdef DEBUG_ENABLED
	return true;
#else
	return false;
#endif
}

void ZConsole::_bind_methods() {
	ClassDB::bind_method(D_METHOD("attach_to_root"), &ZConsole::attach_to_root);
	ClassDB::bind_method(D_METHOD("set_enabled", "enabled"), &ZConsole::set_enabled);
	ClassDB::bind_method(D_METHOD("is_enabled"), &ZConsole::is_enabled);
	ClassDB::bind_method(D_METHOD("open_console"), &ZConsole::open_console);
	ClassDB::bind_method(D_METHOD("close_console"), &ZConsole::close_console);
	ClassDB::bind_method(D_METHOD("is_open"), &ZConsole::get_is_open);
	ClassDB::bind_method(D_METHOD("toggle_console"), &ZConsole::toggle_console);
	ClassDB::bind_method(D_METHOD("clear_console"), &ZConsole::clear_console);
	ClassDB::bind_method(D_METHOD("erase_history"), &ZConsole::erase_history);
	ClassDB::bind_method(D_METHOD("info", "line"), &ZConsole::info);
	ClassDB::bind_method(D_METHOD("error", "line"), &ZConsole::error);
	ClassDB::bind_method(D_METHOD("warn", "line"), &ZConsole::warn);
	ClassDB::bind_method(D_METHOD("debug", "line"), &ZConsole::debug);
	ClassDB::bind_method(D_METHOD("print_line", "line", "stdout"), &ZConsole::print_line, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("register_command", "callable", "name", "description"), &ZConsole::register_command, DEFVAL(StringName()), DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("unregister_command", "callable_or_name"), &ZConsole::unregister_command);
	ClassDB::bind_method(D_METHOD("has_command", "name"), &ZConsole::has_command);
	ClassDB::bind_method(D_METHOD("get_command_names", "include_aliases"), &ZConsole::get_command_names, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("get_command_description", "name"), &ZConsole::get_command_description);
	ClassDB::bind_method(D_METHOD("add_alias", "alias", "command_to_run"), &ZConsole::add_alias);
	ClassDB::bind_method(D_METHOD("remove_alias", "name"), &ZConsole::remove_alias);
	ClassDB::bind_method(D_METHOD("has_alias", "name"), &ZConsole::has_alias);
	ClassDB::bind_method(D_METHOD("get_aliases"), &ZConsole::get_aliases);
	ClassDB::bind_method(D_METHOD("get_alias_argv", "alias"), &ZConsole::get_alias_argv);
	ClassDB::bind_method(D_METHOD("add_argument_autocomplete_source", "command", "argument", "source"), &ZConsole::add_argument_autocomplete_source);
	ClassDB::bind_method(D_METHOD("execute_command", "command_line", "silent"), &ZConsole::execute_command, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("execute_script", "file", "silent"), &ZConsole::execute_script, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("usage", "command"), &ZConsole::usage);
	ClassDB::bind_method(D_METHOD("add_eval_input", "name", "value"), &ZConsole::add_eval_input);
	ClassDB::bind_method(D_METHOD("remove_eval_input", "name"), &ZConsole::remove_eval_input);
	ClassDB::bind_method(D_METHOD("get_eval_input_names"), &ZConsole::get_eval_input_names);
	ClassDB::bind_method(D_METHOD("get_eval_inputs"), &ZConsole::get_eval_inputs);
	ClassDB::bind_method(D_METHOD("set_eval_base_instance", "object"), &ZConsole::set_eval_base_instance);
	ClassDB::bind_method(D_METHOD("get_eval_base_instance"), &ZConsole::get_eval_base_instance);
	ClassDB::bind_method(D_METHOD("format_tip", "text"), &ZConsole::format_tip);
	ClassDB::bind_method(D_METHOD("format_name", "name"), &ZConsole::format_name);

	ADD_SIGNAL(MethodInfo("toggled", PropertyInfo(Variant::BOOL, "is_shown")));
}

void ZConsole::_initialize_runtime() {
	if (runtime_initialized) {
		return;
	}

	_build_gui();
	_register_builtin_commands();
	_register_default_input_actions();

	if (persist_history) {
		_history_load();
	}

	runtime_initialized = true;
}

ZConsole::ZConsole() {
	ERR_FAIL_COND_MSG(singleton != nullptr, "ZConsole singleton already exists.");
	singleton = this;

	set_name("ZConsole");
	set_layer(9999);
	set_process_mode(Node::PROCESS_MODE_ALWAYS);
	set_process_input(true);
	set_process(false);

	output_command_color = Color(0.58, 0.87, 1.0);
	output_command_mention_color = Color(0.9, 0.96, 0.4);
	output_error_color = Color(1.0, 0.43, 0.43);
	output_warning_color = Color(1.0, 0.78, 0.36);
	output_text_color = Color(0.92, 0.94, 0.98);
	output_debug_color = Color(0.66, 0.76, 0.92);

	if (disable_in_release_build && !_is_debug_build()) {
		enabled = false;
	}
}

ZConsole::~ZConsole() {
	if (persist_history) {
		_history_trim(1000);
		_history_save();
	}
	if (singleton == this) {
		singleton = nullptr;
	}
}

void ZConsole::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			_attach_signals();
			_apply_visuals();
			if (panel) {
				panel->hide();
			}
			if (input_blocker) {
				input_blocker->hide();
			}
		} break;
		case NOTIFICATION_PROCESS: {
			const double delta = get_process_delta_time();
			const double time_scale = MAX(Engine::get_singleton()->get_time_scale(), 0.001);
			bool done_sliding = false;
			if (is_open) {
				open_t = Math::move_toward(open_t, 1.0f, open_speed * (float)delta * (1.0f / (float)time_scale));
				done_sliding = Math::is_equal_approx(open_t, 1.0f);
			} else {
				open_t = Math::move_toward(open_t, 0.0f, open_speed * 1.5f * (float)delta * (1.0f / (float)time_scale));
				done_sliding = Math::is_zero_approx(open_t);
			}

			if (panel) {
				const float eased = Math::ease(open_t, -1.75f);
				const float new_y = Math::remap(eased, 0.0f, 1.0f, -panel->get_size().y, 0.0f);
				Vector2 pos = panel->get_position();
				pos.y = new_y;
				panel->set_position(pos);
			}

			if (done_sliding) {
				set_process(false);
				if (!is_open) {
					_hide_console();
				}
			}
		} break;
		case NOTIFICATION_EXIT_TREE: {
			if (persist_history) {
				_history_trim(1000);
				_history_save();
			}
		} break;
	}
}

void ZConsole::_build_gui() {
	input_blocker = memnew(Control);
	input_blocker->set_anchors_preset(Control::PRESET_FULL_RECT);
	input_blocker->set_mouse_filter(Control::MOUSE_FILTER_STOP);
	add_child(input_blocker);

	panel = memnew(PanelContainer);
	panel->set_anchor(Side::SIDE_LEFT, 0.0);
	panel->set_anchor(Side::SIDE_TOP, 0.0);
	panel->set_anchor(Side::SIDE_RIGHT, 1.0);
	panel->set_anchor(Side::SIDE_BOTTOM, height_ratio);
	panel->set_offset(Side::SIDE_LEFT, 0.0);
	panel->set_offset(Side::SIDE_TOP, 0.0);
	panel->set_offset(Side::SIDE_RIGHT, 0.0);
	panel->set_offset(Side::SIDE_BOTTOM, 0.0);
	add_child(panel);

	VBoxContainer *vbox = memnew(VBoxContainer);
	vbox->set_anchors_preset(Control::PRESET_FULL_RECT);
	panel->add_child(vbox);

	output = memnew(RichTextLabel);
	output->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	output->set_selection_enabled(true);
	output->set_scroll_follow(true);
	output->set_use_bbcode(true);
	output->set_focus_mode(Control::FOCUS_CLICK);
	vbox->add_child(output);

	entry = memnew(LineEdit);
	entry->set_clear_button_enabled(true);
	entry->set_shortcut_keys_enabled(true);
	vbox->add_child(entry);
}

void ZConsole::_attach_signals() {
	if (!entry) {
		return;
	}
	if (!entry->is_connected("text_submitted", callable_mp(this, &ZConsole::_on_entry_text_submitted))) {
		entry->connect("text_submitted", callable_mp(this, &ZConsole::_on_entry_text_submitted));
	}
	if (!entry->is_connected("text_changed", callable_mp(this, &ZConsole::_on_entry_text_changed))) {
		entry->connect("text_changed", callable_mp(this, &ZConsole::_on_entry_text_changed));
	}
	if (!entry->is_connected("gui_input", callable_mp(this, &ZConsole::_handle_entry_input))) {
		entry->connect("gui_input", callable_mp(this, &ZConsole::_handle_entry_input));
	}
}

void ZConsole::_apply_visuals() {
	if (panel) {
		panel->set_self_modulate(Color(1.0, 1.0, 1.0, opacity));
	}
	if (output) {
		output->add_theme_color_override("default_color", output_text_color);
	}
}

void ZConsole::attach_to_root() {
	if (attached_to_root) {
		return;
	}
	_initialize_runtime();
	SceneTree *tree = SceneTree::get_singleton();
	if (!tree || !tree->get_root()) {
		return;
	}
	tree->get_root()->add_child(this);
	attached_to_root = true;
	if (!disable_in_release_build || _is_debug_build()) {
		info(format_tip("Type " + format_name("help") + " to inspect available commands."));
	}
}

void ZConsole::set_enabled(bool p_enabled) {
	enabled = p_enabled;
	set_process_input(enabled);
	if (!enabled && is_open) {
		is_open = false;
		set_process(false);
		_hide_console();
	}
}

void ZConsole::open_console() {
	if (!enabled) {
		return;
	}
	is_open = true;
	set_process(true);
	_show_console();
}

void ZConsole::close_console() {
	if (!enabled) {
		return;
	}
	is_open = false;
	set_process(true);
	if (persist_history) {
		_history_save();
	}
}

void ZConsole::toggle_console() {
	if (is_open) {
		close_console();
	} else {
		open_console();
	}
}

void ZConsole::_show_console() {
	if (!panel || !input_blocker || !enabled) {
		return;
	}
	panel->show();
	input_blocker->show();
	if (pause_when_open && get_tree()) {
		was_already_paused = get_tree()->is_paused();
		if (!was_already_paused) {
			get_tree()->set_pause(true);
		}
	}
	if (Window *root_window = get_tree() ? get_tree()->get_root() : nullptr) {
		previous_gui_focus = root_window->gui_get_focus_owner();
	}
	if (entry) {
		entry->grab_focus();
	}
	emit_signal("toggled", true);
}

void ZConsole::_hide_console() {
	if (panel) {
		panel->hide();
	}
	if (input_blocker) {
		input_blocker->hide();
	}
	if (pause_when_open && get_tree() && !was_already_paused) {
		get_tree()->set_pause(false);
	}
	if (previous_gui_focus && Object::cast_to<Control>(previous_gui_focus)) {
		previous_gui_focus->grab_focus();
	}
	emit_signal("toggled", false);
}

void ZConsole::clear_console() {
	if (output) {
		output->clear();
	}
}

void ZConsole::erase_history() {
	history_entries.clear();
	history_dirty = true;
	history_index = -1;
	search_matches.clear();
	search_index = -1;
	Ref<FileAccess> file = FileAccess::open(HISTORY_FILE, FileAccess::WRITE);
	if (file.is_valid()) {
		file->store_string(String());
	}
}

void ZConsole::info(const String &p_line) {
	print_line(p_line, print_to_stdout);
}

void ZConsole::error(const String &p_line) {
	print_line("[color=" + output_error_color.to_html() + "]ERROR:[/color] " + p_line, print_to_stdout);
}

void ZConsole::warn(const String &p_line) {
	print_line("[color=" + output_warning_color.to_html() + "]WARNING:[/color] " + p_line, print_to_stdout);
}

void ZConsole::debug(const String &p_line) {
	print_line("[color=" + output_debug_color.to_html() + "]DEBUG:[/color] " + p_line, print_to_stdout);
}

void ZConsole::print_line(const String &p_line, bool p_stdout) {
	if (silent) {
		return;
	}
	if (output) {
		output->append_text(p_line + "\n");
	}
	if (p_stdout) {
		::print_line(_bbcode_strip(p_line));
	}
}

String ZConsole::format_tip(const String &p_text) const {
	return "[i][color=" + output_debug_color.to_html() + "]" + p_text + "[/color][/i]";
}

String ZConsole::format_name(const String &p_name) const {
	return "[color=" + output_command_mention_color.to_html() + "]" + p_name + "[/color]";
}

void ZConsole::register_command(const Callable &p_callable, const StringName &p_name, const String &p_desc) {
	if (!p_name.is_empty()) {
		PackedStringArray parts = String(p_name).split(" ", false);
		ERR_FAIL_COND_MSG(parts.size() > MAX_SUBCOMMANDS, "ZConsole: Command name supports up to 4 space-separated identifiers.");
		for (int i = 0; i < parts.size(); i++) {
			ERR_FAIL_COND_MSG(!parts[i].is_valid_ascii_identifier(), "ZConsole: Invalid command identifier: " + parts[i]);
		}
	}

	ERR_FAIL_COND_MSG(!_validate_callable(p_callable), "ZConsole: Failed to register command.");

	StringName name = p_name;
	if (name.is_empty()) {
		ERR_FAIL_COND_MSG(p_callable.is_custom(), "ZConsole: Custom Callable requires an explicit command name.");
		String method = String(p_callable.get_method());
		if (method.begins_with("_")) {
			method = method.substr(1);
		}
		if (method.begins_with("cmd_")) {
			method = method.substr(4);
		}
		name = method;
	}

	if (!_is_debug_build()) {
		if (name == StringName("eval")) {
			return;
		}
	}

	ERR_FAIL_COND_MSG(commands.has(name), "ZConsole: Command already registered: " + String(name));

	CommandData command_data;
	command_data.callable = p_callable;
	command_data.method_info = _get_callable_method_info(p_callable);
	command_data.description = p_desc;
	commands.insert(name, command_data);
}

void ZConsole::unregister_command(const Variant &p_callable_or_name) {
	StringName cmd_name;
	if (p_callable_or_name.get_type() == Variant::CALLABLE) {
		Callable target = p_callable_or_name;
		for (const KeyValue<StringName, CommandData> &E : commands) {
			if (E.value.callable == target) {
				cmd_name = E.key;
				break;
			}
		}
	} else if (p_callable_or_name.get_type() == Variant::STRING || p_callable_or_name.get_type() == Variant::STRING_NAME) {
		cmd_name = p_callable_or_name;
	}
	ERR_FAIL_COND_MSG(cmd_name.is_empty() || !commands.has(cmd_name), "ZConsole: Command not found.");
	commands.erase(cmd_name);
	for (int i = 0; i < 5; i++) {
		argument_autocomplete_sources.erase(_autocomplete_key(cmd_name, i));
	}
}

bool ZConsole::has_command(const StringName &p_name) const {
	return commands.has(p_name);
}

PackedStringArray ZConsole::get_command_names(bool p_include_aliases) const {
	PackedStringArray names;
	for (const KeyValue<StringName, CommandData> &E : commands) {
		names.push_back(String(E.key));
	}
	if (p_include_aliases) {
		for (const KeyValue<StringName, PackedStringArray> &E : aliases) {
			names.push_back(String(E.key));
		}
	}
	names.sort();
	return names;
}

String ZConsole::get_command_description(const StringName &p_name) const {
	const CommandData *command = commands.getptr(p_name);
	return command ? command->description : String();
}

void ZConsole::add_alias(const StringName &p_alias, const String &p_command_to_run) {
	aliases.insert(p_alias, _parse_command_line(p_command_to_run));
}

void ZConsole::remove_alias(const StringName &p_name) {
	aliases.erase(p_name);
}

bool ZConsole::has_alias(const StringName &p_name) const {
	return aliases.has(p_name);
}

PackedStringArray ZConsole::get_aliases() const {
	PackedStringArray names;
	for (const KeyValue<StringName, PackedStringArray> &E : aliases) {
		names.push_back(String(E.key));
	}
	names.sort();
	return names;
}

PackedStringArray ZConsole::get_alias_argv(const StringName &p_alias) const {
	const PackedStringArray *argv = aliases.getptr(p_alias);
	return argv ? *argv : PackedStringArray();
}

void ZConsole::add_argument_autocomplete_source(const StringName &p_command, int p_argument, const Callable &p_source) {
	ERR_FAIL_COND_MSG(!p_source.is_valid(), "ZConsole: Autocomplete source Callable is invalid.");
	ERR_FAIL_COND_MSG(!has_command(p_command), "ZConsole: Command does not exist: " + String(p_command));
	ERR_FAIL_COND_MSG(p_argument < 0 || p_argument > 4, "ZConsole: Argument index out of bounds.");
	Variant result = p_source.callv(Array());
	ERR_FAIL_COND_MSG(!_validate_autocomplete_result(result, String(p_command)), "ZConsole: Autocomplete source must return an array.");
	argument_autocomplete_sources.insert(_autocomplete_key(p_command, p_argument), p_source);
}

void ZConsole::execute_command(const String &p_command_line, bool p_silent) {
	String command_line = p_command_line.strip_edges();
	if (command_line.is_empty() || command_line.begins_with("#")) {
		return;
	}

	PackedStringArray argv = _parse_command_line(command_line);
	if (argv.is_empty()) {
		return;
	}
	PackedStringArray expanded_argv = _join_subcommands(_expand_alias(argv));
	if (expanded_argv.is_empty()) {
		return;
	}
	StringName command_name = expanded_argv[0];
	Array command_args;

	silent = p_silent;
	if (!p_silent) {
		_history_push_entry(_join_argv(argv));
		print_line("[color=" + output_command_color.to_html() + "][b]>[/b] " + argv[0] + "[/color]" + (argv.size() > 1 ? " " + _join_argv(argv, 1) : String()));
	}

	const CommandData *command = commands.getptr(command_name);
	if (!command) {
		error("Unknown command: " + String(command_name));
		_suggest_similar_command(expanded_argv);
		silent = false;
		return;
	}

	bool valid = _parse_argv(expanded_argv, *command, command_args);
	if (valid) {
		Variant ret = command->callable.callv(command_args);
		if (ret.get_type() == Variant::INT && int(ret) > 0) {
			_suggest_argument_corrections(expanded_argv);
		}
	} else {
		usage(argv[0]);
	}
	silent = false;
}

void ZConsole::execute_script(const String &p_file, bool p_silent) {
	if (!FileAccess::exists(p_file)) {
		error("File not found: " + p_file.trim_prefix("user://"));
		return;
	}
	Ref<FileAccess> file = FileAccess::open(p_file, FileAccess::READ);
	ERR_FAIL_COND(file.is_null());
	if (!p_silent) {
		info("Executing " + p_file);
	}
	while (!file->eof_reached()) {
		execute_command(file->get_line(), p_silent);
	}
}

int ZConsole::usage(const StringName &p_command) {
	StringName command_name = p_command;
	if (aliases.has(command_name)) {
		PackedStringArray alias_argv = aliases[command_name];
		if (!alias_argv.is_empty()) {
			print_line("Alias of: " + format_name(alias_argv[0]) + (alias_argv.size() > 1 ? " " + _join_argv(alias_argv, 1) : String()));
			command_name = alias_argv[0];
		}
	}

	const CommandData *command = commands.getptr(command_name);
	if (!command) {
		error("Command not found: " + String(command_name));
		return ERR_INVALID_PARAMETER;
	}

	const MethodInfo &method_info = command->method_info;
	String usage_line = "Usage: " + String(command_name);
	String arg_lines;
	String values_lines;
	const int required_args = method_info.arguments.size() - method_info.default_arguments.size();
	for (int i = 0; i < method_info.arguments.size() - command->callable.get_bound_arguments_count(); i++) {
		const PropertyInfo &arg = method_info.arguments[i];
		String arg_name = arg.name.trim_prefix("p_");
		if (i < required_args) {
			usage_line += " " + arg_name;
		} else {
			usage_line += " [lb]" + arg_name + "[rb]";
		}
		String def_spec;
		if (i >= required_args) {
			Variant def_value = method_info.default_arguments[i - required_args];
			def_spec = " = " + def_value.stringify();
		}
		arg_lines += "  " + arg_name + ": " + _format_type(arg.type) + def_spec + "\n";
		String key = _autocomplete_key(command_name, i);
		const Callable *source = argument_autocomplete_sources.getptr(key);
		if (source) {
			PackedStringArray values = _variant_to_string_array(source->callv(Array()));
			if (!values.is_empty()) {
				values_lines += " " + arg_name + ": " + _join_argv(values) + "\n";
			}
		}
	}

	print_line(usage_line);
	if (!command->description.is_empty()) {
		String desc = command->description;
		if (!desc.ends_with(".")) {
			desc += ".";
		}
		print_line(desc.capitalize());
	}
	if (!arg_lines.is_empty()) {
		print_line("Arguments:");
		print_line(arg_lines.trim_suffix("\n"));
	}
	if (!values_lines.is_empty()) {
		print_line("Values:");
		print_line(values_lines.trim_suffix("\n"));
	}
	return OK;
}

void ZConsole::add_eval_input(const StringName &p_name, const Variant &p_value) {
	eval_inputs.insert(p_name, p_value);
}

void ZConsole::remove_eval_input(const StringName &p_name) {
	eval_inputs.erase(p_name);
}

PackedStringArray ZConsole::get_eval_input_names() const {
	PackedStringArray names;
	for (const KeyValue<StringName, Variant> &E : eval_inputs) {
		names.push_back(String(E.key));
	}
	return names;
}

Array ZConsole::get_eval_inputs() const {
	Array values;
	for (const KeyValue<StringName, Variant> &E : eval_inputs) {
		values.push_back(E.value);
	}
	return values;
}

void ZConsole::set_eval_base_instance(Object *p_object) {
	eval_base_instance = p_object ? p_object->get_instance_id() : ObjectID();
}

Object *ZConsole::get_eval_base_instance() const {
	return eval_base_instance.is_valid() ? ObjectDB::get_instance(eval_base_instance) : nullptr;
}

void ZConsole::_fill_entry(const String &p_text) {
	if (!entry) {
		return;
	}
	entry->set_text(p_text);
	entry->set_caret_column(p_text.length());
}

void ZConsole::_clear_suggestions() {
	autocomplete_matches.clear();
	search_matches.clear();
	search_index = -1;
}

void ZConsole::_rebuild_autocomplete() {
	autocomplete_matches.clear();
	if (!entry) {
		return;
	}
	String text = entry->get_text();
	if (text.is_empty()) {
		return;
	}
	PackedStringArray argv = _expand_alias(_parse_command_line(text));
	if (text.ends_with(" ") || argv.is_empty()) {
		argv.push_back(String());
	}
	PackedStringArray matches;
	if (argv.size() == 1 && !argv[0].contains(" ")) {
		_add_first_token_autocomplete(argv[0], matches);
	} else {
		_add_argument_autocomplete(argv, matches);
		_add_subcommand_autocomplete(text, matches);
		_add_history_autocomplete(text, matches);
	}
	matches.sort();
	autocomplete_matches = matches;
}

void ZConsole::_autocomplete(bool p_reverse) {
	if (autocomplete_matches.is_empty()) {
		_rebuild_autocomplete();
	}
	if (autocomplete_matches.is_empty()) {
		return;
	}
	if (p_reverse) {
		String match = autocomplete_matches[autocomplete_matches.size() - 1];
		autocomplete_matches.remove_at(autocomplete_matches.size() - 1);
		autocomplete_matches.insert(0, match);
	}
	_fill_entry(autocomplete_matches[0]);
	String match = autocomplete_matches[0];
	autocomplete_matches.remove_at(0);
	autocomplete_matches.push_back(match);
}

void ZConsole::_search_history() {
	if (!entry) {
		return;
	}
	String text = entry->get_text();
	if (search_matches.is_empty()) {
		search_matches = _history_fuzzy_match(text);
		search_index = 0;
	} else if (search_matches.size() > 0) {
		search_index = (search_index + 1) % search_matches.size();
	}
	if (!search_matches.is_empty()) {
		_fill_entry(search_matches[search_index]);
	}
}

void ZConsole::_handle_entry_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventKey> key_event = p_event;
	if (key_event.is_null() || !key_event->is_pressed() || key_event->is_echo()) {
		return;
	}
	Window *root_window = get_tree() ? get_tree()->get_root() : nullptr;

	Key keycode = key_event->get_keycode();
	if (keycode == Key::UP) {
		if (!history_entries.is_empty()) {
			history_index = history_index < 0 ? history_entries.size() - 1 : MAX(0, history_index - 1);
			_fill_entry(history_entries[history_index]);
		}
		if (root_window) {
			root_window->set_input_as_handled();
		}
		return;
	}
	if (keycode == Key::DOWN) {
		if (!history_entries.is_empty()) {
			history_index = history_index < 0 ? history_entries.size() - 1 : MIN(history_entries.size() - 1, history_index + 1);
			_fill_entry(history_entries[history_index]);
		}
		if (root_window) {
			root_window->set_input_as_handled();
		}
		return;
	}
	if (keycode == Key::TAB) {
		_autocomplete(key_event->is_shift_pressed());
		if (root_window) {
			root_window->set_input_as_handled();
		}
		return;
	}
	if (key_event->is_action_pressed("limbo_console_search_history")) {
		_search_history();
		if (root_window) {
			root_window->set_input_as_handled();
		}
		return;
	}
}

void ZConsole::_on_entry_text_submitted(const String &p_text) {
	_clear_suggestions();
	history_index = -1;
	_fill_entry(String());
	execute_command(p_text);
}

void ZConsole::_on_entry_text_changed(const String &p_text) {
	_clear_suggestions();
	if (p_text.is_empty()) {
		history_index = -1;
		return;
	}
	_rebuild_autocomplete();
}

void ZConsole::_history_load() {
	Ref<FileAccess> file = FileAccess::open(HISTORY_FILE, FileAccess::READ);
	if (file.is_null()) {
		return;
	}
	while (!file->eof_reached()) {
		String line = file->get_line().strip_edges();
		if (!line.is_empty()) {
			history_entries.push_back(line);
		}
	}
	history_dirty = false;
	history_index = -1;
}

void ZConsole::_history_save() {
	if (!history_dirty) {
		return;
	}
	Ref<FileAccess> file = FileAccess::open(HISTORY_FILE, FileAccess::WRITE);
	if (file.is_null()) {
		return;
	}
	for (int i = 0; i < history_entries.size(); i++) {
		file->store_line(history_entries[i]);
	}
	history_dirty = false;
}

void ZConsole::_history_push_entry(const String &p_entry) {
	int existing = history_entries.find(p_entry);
	if (existing >= 0) {
		history_entries.remove_at(existing);
	}
	history_entries.push_back(p_entry);
	history_index = -1;
	history_dirty = true;
}

void ZConsole::_history_trim(int p_max_size) {
	while (history_entries.size() > p_max_size) {
		history_entries.remove_at(0);
	}
	history_index = -1;
}

PackedStringArray ZConsole::_history_fuzzy_match(const String &p_query) const {
	if (p_query.is_empty()) {
		PackedStringArray copy = history_entries;
		copy.reverse();
		return copy;
	}
	struct MatchScore {
		String entry;
		int score = 0;
	};
	Vector<MatchScore> scores;
	String query = p_query.to_lower();
	for (int i = 0; i < history_entries.size(); i++) {
		String target = history_entries[i].to_lower();
		int score = 0;
		int query_index = 0;
		if (query == target) {
			score = 99999;
		} else {
			for (int j = 0; j < target.length(); j++) {
				if (query_index < query.length() && target[j] == query[query_index]) {
					score += 10;
					if (j == 0 || target[j - 1] == ' ') {
						score += 5;
					}
					query_index++;
					if (query_index == query.length()) {
						break;
					}
				}
			}
			if (query_index != query.length()) {
				score = 0;
			}
		}
		if (score > 0) {
			MatchScore match;
			match.entry = history_entries[i];
			match.score = score;
			scores.push_back(match);
		}
	}
	struct MatchScoreComparator {
		bool operator()(const MatchScore &p_a, const MatchScore &p_b) const {
			return p_a.score > p_b.score;
		}
	};
	scores.sort_custom<MatchScoreComparator>();
	PackedStringArray result;
	for (int i = 0; i < scores.size(); i++) {
		result.push_back(scores[i].entry);
	}
	return result;
}

void ZConsole::_register_builtin_commands() {
	register_command(callable_mp(this, &ZConsole::_cmd_alias), "alias", "add command alias");
	register_command(callable_mp(this, &ZConsole::_cmd_aliases), "aliases", "list all aliases");
	register_command(callable_mp(this, &ZConsole::clear_console), "clear", "clear console screen");
	register_command(callable_mp(this, &ZConsole::_cmd_commands), "commands", "list all commands");
	register_command(callable_mp(this, &ZConsole::info), "echo", "display a line of text");
	register_command(callable_mp(this, &ZConsole::_cmd_eval), "eval", "evaluate an expression");
	register_command(callable_mp(this, &ZConsole::_cmd_exec), "exec", "execute commands from file");
	register_command(callable_mp(this, &ZConsole::_cmd_fps_max), "fps_max", "limit framerate");
	register_command(callable_mp(this, &ZConsole::_cmd_fullscreen), "fullscreen", "toggle fullscreen mode");
	register_command(callable_mp(this, &ZConsole::_cmd_help), "help", "show command info");
	register_command(callable_mp(this, &ZConsole::_cmd_log), "log", "show recent log entries");
	register_command(callable_mp(this, &ZConsole::_cmd_quit), "quit", "exit the application");
	register_command(callable_mp(this, &ZConsole::_cmd_unalias), "unalias", "remove command alias");
	register_command(callable_mp(this, &ZConsole::_cmd_vsync), "vsync", "adjust V-Sync");
	register_command(callable_mp(this, &ZConsole::erase_history), "erase_history", "erase persisted history");
	add_argument_autocomplete_source("help", 0, callable_mp(this, &ZConsole::get_command_names).bind(true));
}

void ZConsole::_register_default_input_actions() {
	struct DefaultAction {
		StringName name;
		Ref<InputEventKey> event;
	};
	Vector<DefaultAction> actions;

	DefaultAction toggle_action;
	toggle_action.name = "limbo_console_toggle";
	toggle_action.event = InputEventKey::create_reference(Key::QUOTELEFT);
	actions.push_back(toggle_action);

	DefaultAction reverse_action;
	reverse_action.name = "limbo_auto_complete_reverse";
	reverse_action.event = InputEventKey::create_reference(KeyModifierMask::SHIFT | Key::TAB);
	actions.push_back(reverse_action);

	DefaultAction history_action;
	history_action.name = "limbo_console_search_history";
	history_action.event = InputEventKey::create_reference(KeyModifierMask::CTRL | Key::R);
	actions.push_back(history_action);

	for (int i = 0; i < actions.size(); i++) {
		const DefaultAction &action = actions[i];
		if (!InputMap::get_singleton()->has_action(action.name)) {
			InputMap::get_singleton()->add_action(action.name, InputMap::DEFAULT_TOGGLE_DEADZONE);
		}
		if (!InputMap::get_singleton()->action_has_event(action.name, action.event)) {
			InputMap::get_singleton()->action_add_event(action.name, action.event);
		}
		if (!ProjectSettings::get_singleton()->has_setting("input/" + String(action.name))) {
			Dictionary input_action_data;
			input_action_data["deadzone"] = 0.5;
			Array events;
			events.push_back(action.event);
			input_action_data["events"] = events;
			ProjectSettings::get_singleton()->set_setting("input/" + String(action.name), input_action_data);
		}
	}
	InputMap::get_singleton()->load_from_project_settings();
}

MethodInfo ZConsole::_get_callable_method_info(const Callable &p_callable) const {
	MethodInfo info;
	Object *object = p_callable.get_object();
	if (object) {
		List<MethodInfo> methods;
		object->get_method_list(&methods);
		for (const MethodInfo &method : methods) {
			if (method.name == String(p_callable.get_method())) {
				info = method;
				break;
			}
		}
	}
	if (info.name.is_empty()) {
		bool is_valid = false;
		int arg_count = p_callable.get_argument_count(&is_valid);
		if (is_valid) {
			info.name = String(p_callable.get_method());
			for (int i = 0; i < arg_count; i++) {
				info.arguments.push_back(PropertyInfo(Variant::NIL, "arg" + itos(i)));
			}
		}
	}
	return info;
}

bool ZConsole::_validate_callable(const Callable &p_callable) const {
	if (!p_callable.is_valid()) {
		return false;
	}
	MethodInfo info = _get_callable_method_info(p_callable);
	for (int i = 0; i < info.arguments.size(); i++) {
		Variant::Type type = info.arguments[i].type;
		if (type != Variant::NIL && type != Variant::BOOL && type != Variant::INT && type != Variant::FLOAT && type != Variant::STRING && type != Variant::VECTOR2 && type != Variant::VECTOR2I && type != Variant::VECTOR3 && type != Variant::VECTOR3I && type != Variant::VECTOR4 && type != Variant::VECTOR4I) {
			return false;
		}
	}
	return true;
}

bool ZConsole::_validate_autocomplete_result(const Variant &p_result, const String &p_command) const {
	Variant::Type type = p_result.get_type();
	if (type != Variant::ARRAY && type != Variant::PACKED_STRING_ARRAY) {
		ERR_PRINT("ZConsole: Autocomplete source must return an array for command: " + p_command);
		return false;
	}
	return true;
}

PackedStringArray ZConsole::_variant_to_string_array(const Variant &p_value) const {
	PackedStringArray result;
	if (p_value.get_type() == Variant::PACKED_STRING_ARRAY) {
		return p_value;
	}
	if (p_value.get_type() == Variant::ARRAY) {
		Array array = p_value;
		for (int i = 0; i < array.size(); i++) {
			result.push_back(String(array[i]));
		}
	}
	return result;
}

PackedStringArray ZConsole::_parse_command_line(const String &p_line) const {
	PackedStringArray argv;
	String line = p_line.strip_edges();
	bool in_quotes = false;
	bool in_brackets = false;
	int start = 0;
	for (int i = 0; i < line.length(); i++) {
		char32_t c = line[i];
		if (c == '"') {
			in_quotes = !in_quotes;
		} else if (c == '(') {
			in_brackets = true;
		} else if (c == ')') {
			in_brackets = false;
		} else if (c == ' ' && !in_quotes && !in_brackets) {
			if (i > start) {
				argv.push_back(line.substr(start, i - start));
			}
			start = i + 1;
		}
	}
	if (line.length() > start) {
		argv.push_back(line.substr(start, line.length() - start));
	}
	return argv;
}

PackedStringArray ZConsole::_join_subcommands(const PackedStringArray &p_argv) const {
	for (int count = MAX_SUBCOMMANDS; count > 1; count--) {
		if (p_argv.size() >= count) {
			String cmd = _join_argv(p_argv, 0, count);
			if (has_command(cmd) || has_alias(cmd)) {
				PackedStringArray argv;
				argv.push_back(cmd);
				PackedStringArray rest = _slice_argv(p_argv, count);
				for (int i = 0; i < rest.size(); i++) {
					argv.push_back(rest[i]);
				}
				return argv;
			}
		}
	}
	return p_argv;
}

PackedStringArray ZConsole::_expand_alias(const PackedStringArray &p_argv) const {
	PackedStringArray argv = p_argv;
	PackedStringArray result;
	int depth = 0;
	while (!argv.is_empty() && depth < 1000) {
		argv = _join_subcommands(argv);
		StringName current = argv[0];
		argv.remove_at(0);
		const PackedStringArray *alias_argv = aliases.getptr(current);
		depth++;
		if (alias_argv && !alias_argv->is_empty()) {
			PackedStringArray merged = *alias_argv;
			for (int i = 0; i < argv.size(); i++) {
				merged.push_back(argv[i]);
			}
			argv = merged;
		} else {
			result.push_back(String(current));
		}
	}
	return depth >= 1000 ? p_argv : result;
}

bool ZConsole::_parse_argv(const PackedStringArray &p_argv, const CommandData &p_command, Array &r_args) {
	const MethodInfo &method_info = p_command.method_info;
	if (method_info.name.is_empty()) {
		error("Couldn't determine method info for command: " + String(p_argv[0]));
		return false;
	}

	const int num_bound_args = p_command.callable.get_bound_arguments_count();
	const int max_args = method_info.arguments.size();
	const int required_args = max_args - method_info.default_arguments.size();
	const int num_args = p_argv.size() + num_bound_args - 1;

	if (max_args - num_bound_args == 1 && method_info.arguments[0].type == Variant::STRING) {
		String joined = _join_argv(p_argv, 1);
		if (joined.begins_with("\"") && joined.ends_with("\"")) {
			joined = joined.substr(1, joined.length() - 2);
		}
		r_args.push_back(joined);
		return true;
	}
	if (num_args < required_args) {
		error("Missing arguments.");
		return false;
	}
	if (num_args > max_args) {
		error("Too many arguments.");
		return false;
	}

	bool passed = true;
	for (int i = 1; i < p_argv.size(); i++) {
		String value = p_argv[i];
		Variant parsed = value.trim_prefix("\"").trim_suffix("\"");
		Variant::Type expected_type = method_info.arguments[i - 1].type;
		if (value.begins_with("(") && value.ends_with(")")) {
			Variant vec = _parse_vector_arg(value);
			if (vec.get_type() != Variant::NIL) {
				parsed = vec;
			}
		} else if (value.is_valid_float()) {
			parsed = value.to_float();
		} else if (value.is_valid_int()) {
			parsed = value.to_int();
		} else if (value == "true" || value == "yes") {
			parsed = true;
		} else if (value == "false" || value == "no") {
			parsed = false;
		}
		if (!_are_compatible_types(expected_type, parsed.get_type())) {
			error("Argument " + itos(i) + " expects " + _format_type(expected_type) + ", but got " + _format_type(parsed.get_type()) + ".");
			passed = false;
		}
		r_args.push_back(parsed);
	}
	return passed;
}

bool ZConsole::_are_compatible_types(Variant::Type p_expected_type, Variant::Type p_parsed_type) const {
	return p_expected_type == p_parsed_type || p_expected_type == Variant::NIL || p_expected_type == Variant::STRING || ((p_expected_type == Variant::BOOL || p_expected_type == Variant::INT || p_expected_type == Variant::FLOAT) && (p_parsed_type == Variant::BOOL || p_parsed_type == Variant::INT || p_parsed_type == Variant::FLOAT)) || ((p_expected_type == Variant::VECTOR2 || p_expected_type == Variant::VECTOR2I) && (p_parsed_type == Variant::VECTOR2 || p_parsed_type == Variant::VECTOR2I)) || ((p_expected_type == Variant::VECTOR3 || p_expected_type == Variant::VECTOR3I) && (p_parsed_type == Variant::VECTOR3 || p_parsed_type == Variant::VECTOR3I)) || ((p_expected_type == Variant::VECTOR4 || p_expected_type == Variant::VECTOR4I) && (p_parsed_type == Variant::VECTOR4 || p_parsed_type == Variant::VECTOR4I));
}

Variant ZConsole::_parse_vector_arg(const String &p_text) const {
	String inner = p_text.substr(1, p_text.length() - 2).replace(",", " ").strip_edges();
	PackedStringArray parts = inner.split(" ", false);
	Vector<float> values;
	for (int i = 0; i < parts.size(); i++) {
		if (!parts[i].is_valid_float()) {
			return Variant();
		}
		values.push_back(parts[i].to_float());
	}
	if (values.size() == 2) {
		return Vector2(values[0], values[1]);
	}
	if (values.size() == 3) {
		return Vector3(values[0], values[1], values[2]);
	}
	if (values.size() == 4) {
		return Vector4(values[0], values[1], values[2], values[3]);
	}
	return Variant();
}

String ZConsole::_join_argv(const PackedStringArray &p_argv, int p_from, int p_to) const {
	if (p_to < 0 || p_to > p_argv.size()) {
		p_to = p_argv.size();
	}
	String result;
	for (int i = p_from; i < p_to; i++) {
		if (!result.is_empty()) {
			result += " ";
		}
		result += p_argv[i];
	}
	return result;
}

PackedStringArray ZConsole::_slice_argv(const PackedStringArray &p_argv, int p_from, int p_to) const {
	if (p_to < 0 || p_to > p_argv.size()) {
		p_to = p_argv.size();
	}
	PackedStringArray result;
	for (int i = p_from; i < p_to; i++) {
		result.push_back(p_argv[i]);
	}
	return result;
}

void ZConsole::_add_first_token_autocomplete(const String &p_prefix, PackedStringArray &r_matches) const {
	PackedStringArray names = get_command_names(true);
	HashMap<String, bool> unique;
	for (int i = 0; i < names.size(); i++) {
		String first = names[i].split(" ")[0];
		if (first.begins_with(p_prefix) && !unique.has(first)) {
			unique.insert(first, true);
			r_matches.push_back(first);
		}
	}
}

void ZConsole::_add_argument_autocomplete(const PackedStringArray &p_argv, PackedStringArray &r_matches) const {
	if (p_argv.is_empty()) {
		return;
	}
	StringName command = p_argv[0];
	int last_arg = p_argv.size() - 1;
	const Callable *source = argument_autocomplete_sources.getptr(_autocomplete_key(command, last_arg - 1));
	if (!source) {
		return;
	}
	PackedStringArray values = _variant_to_string_array(source->callv(Array()));
	String prefix = p_argv[last_arg];
	String head = _join_argv(_slice_argv(p_argv, 0, last_arg));
	if (!head.is_empty()) {
		head += " ";
	}
	for (int i = 0; i < values.size(); i++) {
		if (values[i].begins_with(prefix)) {
			r_matches.push_back(head + values[i]);
		}
	}
}

void ZConsole::_add_subcommand_autocomplete(const String &p_text, PackedStringArray &r_matches) const {
	PackedStringArray command_names = get_command_names(true);
	PackedStringArray typed_tokens = p_text.split(" ", false);
	HashMap<String, bool> unique;
	for (int i = 0; i < command_names.size(); i++) {
		PackedStringArray cmd_tokens = command_names[i].split(" ", false);
		if (cmd_tokens.size() < typed_tokens.size()) {
			continue;
		}
		int last_match = 0;
		for (int j = 0; j < typed_tokens.size(); j++) {
			if (cmd_tokens[j] != typed_tokens[j]) {
				break;
			}
			last_match++;
		}
		if (last_match < typed_tokens.size() - 1) {
			continue;
		}
		if (cmd_tokens[last_match].begins_with(typed_tokens[typed_tokens.size() - 1])) {
			String partial = _join_argv(_slice_argv(cmd_tokens, 0, last_match + 1));
			if (!unique.has(partial)) {
				unique.insert(partial, true);
				r_matches.push_back(partial);
			}
		}
	}
}

void ZConsole::_add_history_autocomplete(const String &p_text, PackedStringArray &r_matches) const {
	for (int i = history_entries.size() - 1; i >= 0; i--) {
		if (history_entries[i].begins_with(p_text)) {
			r_matches.push_back(history_entries[i]);
		}
	}
}

void ZConsole::_suggest_similar_command(const PackedStringArray &p_argv) {
	if (silent || p_argv.is_empty()) {
		return;
	}
	String fuzzy_hit = _fuzzy_match_string(p_argv[0], 2, get_command_names(true));
	if (!fuzzy_hit.is_empty()) {
		info(format_tip("Did you mean " + format_name(fuzzy_hit) + "? (TAB to fill)"));
		PackedStringArray corrected = p_argv;
		corrected.set(0, fuzzy_hit);
		autocomplete_matches.push_back(_join_argv(corrected));
	}
}

void ZConsole::_suggest_argument_corrections(const PackedStringArray &p_argv) {
	if (silent || p_argv.is_empty()) {
		return;
	}
	StringName command_name = p_argv[0];
	if (aliases.has(command_name) && !aliases[command_name].is_empty()) {
		command_name = aliases[command_name][0];
	}
	PackedStringArray corrected = p_argv;
	bool changed = false;
	for (int i = 1; i < p_argv.size(); i++) {
		const Callable *source = argument_autocomplete_sources.getptr(_autocomplete_key(command_name, i));
		if (!source) {
			continue;
		}
		PackedStringArray values = _variant_to_string_array(source->callv(Array()));
		String fuzzy_hit = _fuzzy_match_string(p_argv[i], 2, values);
		if (!fuzzy_hit.is_empty()) {
			corrected.set(i, fuzzy_hit);
			changed = true;
		}
	}
	if (changed) {
		String suggestion = _join_argv(corrected);
		info(format_tip("Did you mean \"" + suggestion + "\"? (TAB to fill)"));
		autocomplete_matches.push_back(suggestion);
	}
}

String ZConsole::_fuzzy_match_string(const String &p_string, int p_max_edit_distance, const PackedStringArray &p_array) const {
	int best_distance = INT32_MAX;
	String best_match;
	for (int i = 0; i < p_array.size(); i++) {
		int distance = _calculate_osa_distance(p_string, p_array[i]);
		if (distance < best_distance) {
			best_distance = distance;
			best_match = p_array[i];
		}
	}
	return best_distance <= p_max_edit_distance ? best_match : String();
}

int ZConsole::_calculate_osa_distance(const String &p_a, const String &p_b) const {
	String a = p_a.to_lower();
	String b = p_b.to_lower();
	const int a_len = a.length();
	const int b_len = b.length();
	PackedInt32Array row0;
	PackedInt32Array row1;
	PackedInt32Array row2;
	row0.resize(b_len + 1);
	row1.resize(b_len + 1);
	row2.resize(b_len + 1);
	for (int i = 0; i <= b_len; i++) {
		row1.set(i, i);
	}
	for (int i = 0; i < a_len; i++) {
		row2.set(0, i + 1);
		for (int j = 0; j < b_len; j++) {
			int deletion = row1[j + 1] + 1;
			int insertion = row2[j] + 1;
			int substitution = row1[j] + (a[i] == b[j] ? 0 : 1);
			int value = MIN(deletion, MIN(insertion, substitution));
			if (i > 0 && j > 0 && a[i] == b[j - 1] && a[i - 1] == b[j]) {
				value = MIN(value, row0[j - 1] + 1);
			}
			row2.set(j + 1, value);
		}
		PackedInt32Array tmp = row0;
		row0 = row1;
		row1 = row2;
		row2 = tmp;
	}
	return row1[b_len];
}

String ZConsole::_format_type(Variant::Type p_type) const {
	return p_type == Variant::NIL ? "Variant" : Variant::get_type_name(p_type);
}

String ZConsole::_bbcode_escape(const String &p_text) const {
	return p_text.replace("[", "[lb]").replace("]", "[rb]");
}

String ZConsole::_bbcode_strip(const String &p_text) const {
	String stripped;
	bool in_brackets = false;
	for (int i = 0; i < p_text.length(); i++) {
		char32_t c = p_text[i];
		if (c == '[') {
			in_brackets = true;
		} else if (c == ']') {
			in_brackets = false;
		} else if (!in_brackets) {
			stripped += c;
		}
	}
	return stripped;
}

void ZConsole::_cmd_alias(const String &p_alias, const String &p_command) {
	info("Adding " + format_name(p_alias) + " => " + p_command);
	add_alias(p_alias, p_command);
}

void ZConsole::_cmd_aliases() {
	PackedStringArray alias_names = get_aliases();
	for (int i = 0; i < alias_names.size(); i++) {
		PackedStringArray alias_argv = get_alias_argv(alias_names[i]);
		String description = alias_argv.is_empty() ? String() : get_command_description(alias_argv[0]);
		if (description.is_empty()) {
			info(format_name(alias_names[i]));
		} else {
			info(format_name(alias_names[i]) + " is alias of: " + _join_argv(alias_argv) + " " + format_tip("// " + description));
		}
	}
}

void ZConsole::_cmd_commands() {
	info("Available commands:");
	PackedStringArray names = get_command_names(false);
	for (int i = 0; i < names.size(); i++) {
		String description = get_command_description(names[i]);
		info(description.is_empty() ? format_name(names[i]) : format_name(names[i]) + " -- " + description);
	}
}

int ZConsole::_cmd_eval(const String &p_expression) {
	Expression expression;
	PackedStringArray names = get_eval_input_names();
	Array values = get_eval_inputs();
	int err = expression.parse(p_expression, names);
	if (err != OK) {
		error(expression.get_error_text());
		return err;
	}
	Variant result = expression.execute(values, get_eval_base_instance());
	if (!expression.has_execute_failed()) {
		if (!result.is_null()) {
			info(String(result));
		}
		return OK;
	}
	error(expression.get_error_text());
	return ERR_SCRIPT_FAILED;
}

void ZConsole::_cmd_exec(const String &p_file, bool p_silent) {
	String file = p_file;
	if (!file.ends_with(".lcs")) {
		file += ".lcs";
	}
	if (!FileAccess::exists(file)) {
		file = "user://" + file;
	}
	execute_script(file, p_silent);
}

void ZConsole::_cmd_fps_max(int p_limit) {
	if (p_limit < 0) {
		int current = Engine::get_singleton()->get_max_fps();
		info(current == 0 ? "Framerate is unlimited." : vformat("Framerate is limited to %d FPS.", current));
		return;
	}
	Engine::get_singleton()->set_max_fps(p_limit);
	if (p_limit > 0) {
		info(vformat("Limiting framerate to %d FPS.", p_limit));
	} else {
		info("Removing framerate limits.");
	}
}

void ZConsole::_cmd_fullscreen() {
	Window *window = get_window();
	if (!window) {
		return;
	}
	if (window->get_mode() == Window::MODE_WINDOWED) {
		window->set_mode(Window::MODE_FULLSCREEN);
		info("Window switched to fullscreen mode.");
	} else {
		window->set_mode(Window::MODE_WINDOWED);
		info("Window switched to windowed mode.");
	}
}

int ZConsole::_cmd_help(const String &p_command_name) {
	if (p_command_name.is_empty()) {
		print_line(format_tip("Type " + format_name("commands") + " to list all available commands."));
		print_line(format_tip("Type " + format_name("help command") + " to get more info about a command."));
		return OK;
	}
	return usage(p_command_name);
}

int ZConsole::_cmd_log(int p_num_lines) {
	String path = ProjectSettings::get_singleton()->get_setting("debug/file_logging/log_path");
	Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
	if (file.is_null()) {
		error("Can't open file: " + path);
		return ERR_CANT_OPEN;
	}
	Vector<String> lines;
	while (!file->eof_reached()) {
		lines.push_back(file->get_line());
	}
	int start = MAX(0, lines.size() - p_num_lines);
	for (int i = start; i < lines.size(); i++) {
		print_line(_bbcode_escape(lines[i]), false);
	}
	return OK;
}

void ZConsole::_cmd_quit() {
	if (get_tree()) {
		get_tree()->quit();
	}
}

void ZConsole::_cmd_unalias(const String &p_alias) {
	if (has_alias(p_alias)) {
		remove_alias(p_alias);
		info("Alias removed.");
	} else {
		warn("Alias not found.");
	}
}

void ZConsole::_cmd_vsync(int p_mode) {
	if (p_mode < 0) {
		DisplayServer::VSyncMode mode = DisplayServer::get_singleton()->window_get_vsync_mode(DisplayServer::MAIN_WINDOW_ID);
		info(vformat("Current V-Sync mode: %d.", int(mode)));
		info("Adjust V-Sync mode with an argument: 0 - disabled, 1 - enabled, 2 - adaptive.");
		return;
	}
	if (p_mode == DisplayServer::VSYNC_DISABLED || p_mode == DisplayServer::VSYNC_ENABLED || p_mode == DisplayServer::VSYNC_ADAPTIVE) {
		DisplayServer::get_singleton()->window_set_vsync_mode(DisplayServer::VSyncMode(p_mode), DisplayServer::MAIN_WINDOW_ID);
		info(vformat("Changed V-Sync mode to %d.", p_mode));
	} else {
		error("Invalid mode.");
	}
}
