# ZConsole 运行时控制台

> 类 DOS/Quake 风格的命令行控制台，支持命令注册、别名、历史记录、自动补全。

---

## 快速开始

```gdscript
# 任何脚本随时调用，无需初始化

# 打印日志
ZConsole.info("游戏开始")
ZConsole.warn("内存不足")
ZConsole.error("加载失败")

# 注册命令
ZConsole.register_command(callable(self, "_cmd_heal"), "heal", "治愈玩家")

# 执行命令
ZConsole.execute_command("set score 100")
```

---

## 快捷键

| 按键 | 功能 |
|------|------|
| `` ` `` (反引号) | 打开/关闭控制台 |
| `↑` / `↓` | 浏览命令历史 |
| `Tab` / `Shift+Tab` | 自动补全 |
| `Ctrl+R` | 搜索历史记录 |
| `Esc` | 关闭控制台 |

---

## 配置（ProjectSettings）

| 设置路径 | 类型 | 默认值 | 描述 |
|----------|------|--------|------|
| `zzz/console/enabled` | bool | `true` | 是否响应 UI 快捷键（不影响 API） |
| `zzz/console/persist_history` | bool | `true` | 是否持久化命令历史 |
| `zzz/console/pause_when_open` | bool | `true` | 打开时是否暂停游戏 |
| `zzz/console/height_ratio` | float | `0.5` | 高度比例 (0.2-1.0) |
| `zzz/console/open_speed` | float | `5.0` | 打开动画速度 |
| `zzz/console/opacity` | float | `0.96` | 透明度 (0.1-1.0) |
| `zzz/console/font_size` | int | `14` | 字体大小 (8-64) |
| `zzz/console/toggle_action` | String | `"limbo_console_toggle"` | 切换操作名称 |
| `zzz/console/toggle_shortcut` | String | `"QuoteLeft"` | 切换快捷键 |

> **注意**：`enabled` 仅影响控制台 UI 是否响应输入，API 调用始终可用。

---

## API 参考

### 日志输出

```gdscript
ZConsole.info("这是一条普通信息")
ZConsole.warn("这是一条警告")
ZConsole.error("这是一条错误信息")
ZConsole.debug("这是一条调试信息")
ZConsole.print_line("普通文本", false)  # 第二个参数：是否同时输出到 stdout
```

### 命令注册

#### `void register_command(Callable callable, StringName name = "", String desc = "")`

注册控制台命令。

```gdscript
# 自动命名（使用函数名，_cmd_ 前缀和下划线会被移除）
ZConsole.register_command(callable(self, "_cmd_heal"))
# 函数名 "_cmd_heal" 注册后命令名为 "heal"

# 显式命名和描述
ZConsole.register_command(callable(self, "_cmd_teleport"), "teleport", "瞬间移动到指定位置")

# 匿名 Callable
ZConsole.register_command(Callable(), lambda: ZConsole.info("Hello!"))
```

**支持的参数类型**：bool, int, float, String, Vector2, Vector2i, Vector3, Vector3i, Vector4, Vector4i

```gdscript
func _cmd_teleport(pos: Vector2, instant: bool = false):
    $Player.global_position = pos
    $Player.instant_move = instant
    ZConsole.info("已传送到 %s" % pos)
```

#### `void unregister_command(Variant callable_or_name)`

注销命令。

```gdscript
# 通过名称注销
ZConsole.unregister_command("god")

# 通过 Callable 注销
ZConsole.unregister_command(callable(self, "_cmd_god_mode"))
```

#### `bool has_command(StringName name)`

检查命令是否存在。

#### `PackedStringArray get_command_names(bool include_aliases = false)`

获取所有命令名。

#### `String get_command_description(StringName name)`

获取命令描述。

### 命令别名

#### `void add_alias(StringName alias, String command_to_run)`

添加命令别名。

```gdscript
ZConsole.add_alias("q", "quit")
ZConsole.add_alias("h", "help")
ZConsole.add_alias("cls", "clear")
ZConsole.add_alias("g", "get")

# 别名可以包含多个命令
ZConsole.add_alias("maxstats", "set health 999; set mana 999; set stamina 999")
```

#### `void remove_alias(StringName name)`
#### `bool has_alias(StringName name)`
#### `PackedStringArray get_aliases()`
#### `PackedStringArray get_alias_argv(StringName alias)`

### 参数自动补全

#### `void add_argument_autocomplete_source(StringName command, int argument, Callable source)`

为命令参数添加自动补全源。

```gdscript
# 为 teleport 命令的第 0 个参数添加补全
ZConsole.add_argument_autocomplete_source("teleport", 0, callable(self, "_get_location_names"))

func _get_location_names() -> PackedStringArray:
    return ["spawn", "town", "dungeon", "boss_room"]
```

### Eval 输入变量

允许在 `eval` 命令中使用预定义的变量。

```gdscript
# 设置基础实例，允许访问其属性
ZConsole.set_eval_base_instance(self)

# 添加变量
ZConsole.add_eval_input("health", 100)
ZConsole.add_eval_input("player_pos", Vector2.ZERO)

# 然后在控制台输入：eval health + 50
```

#### `void add_eval_input(StringName name, Variant value)`
#### `void remove_eval_input(StringName name)`
#### `PackedStringArray get_eval_input_names()`
#### `Array get_eval_inputs()`
#### `void set_eval_base_instance(Object object)`
#### `Object get_eval_base_instance()`

### 脚本执行

#### `void execute_command(String command_line, bool silent = false)`

以编程方式执行命令。

```gdscript
ZConsole.execute_command("set score 9999")
ZConsole.execute_command("give sword")
```

#### `void execute_script(String file, bool silent = true)`

执行脚本文件（`.lcs` 格式）。

```gdscript
ZConsole.execute_script("user://scripts/startup.lcs")
```

`.lcs` 脚本格式示例：

```
# 初始化脚本
set debug_mode true
echo Game initialized!
```

### 控制台 UI 控制

#### `void open_console()`
#### `void close_console()`
#### `void toggle_console()`
#### `bool get_is_open()`
#### `void clear_console()`

### 格式化

```gdscript
# 格式化提示文本（斜体蓝色）
var tip = ZConsole.format_tip("Press TAB to autocomplete")

# 格式化命令名称（黄色高亮）
var cmd_name = ZConsole.format_name("help")
```

### 信号

```gdscript
func _ready():
    ZConsole.toggled.connect(_on_console_toggled)

func _on_console_toggled(is_shown: bool):
    print("Console: %s" % ("opened" if is_shown else "closed"))
```

---

## 内置命令

| 命令 | 描述 | 示例 |
|------|------|------|
| `help [command]` | 显示帮助 | `help set` |
| `commands` | 列出所有命令 | `commands` |
| `aliases` | 列出所有别名 | `aliases` |
| `alias <name> <cmd>` | 添加别名 | `alias h help` |
| `unalias <name>` | 移除别名 | `unalias h` |
| `clear` | 清屏 | `clear` |
| `echo <text>` | 输出文本 | `echo Hello` |
| `get [name]` | 获取全局变量 | `get score` |
| `get_all` | 获取所有全局变量 | `get_all` |
| `set <name> <value>` | 设置全局变量 | `set health 100` |
| `eval <expr>` | 计算表达式 | `eval 2 + 2` |
| `exec <filename>` | 执行脚本文件 | `exec startup` |
| `fps_max [limit]` | 设置帧率上限 | `fps_max 60` |
| `fullscreen` | 切换全屏 | `fullscreen` |
| `vsync [mode]` | 垂直同步模式 | `vsync 0` |
| `log [lines]` | 显示最近日志 | `log 50` |
| `quit` | 退出游戏 | `quit` |
| `erase_history` | 清除历史记录 | `erase_history` |

---

## 命令参数类型

| 类型 | 示例 | 解析结果 |
|------|------|----------|
| bool | `true`, `false`, `yes`, `no` | `bool` |
| int | `42`, `-10` | `int` |
| float | `3.14`, `-0.5` | `float` |
| String | `"hello world"` | `String` |
| Vector2 | `(100, 200)` | `Vector2` |
| Vector3 | `(1.0, 2.0, 3.0)` | `Vector3` |
| Vector4 | `(1, 2, 3, 4)` | `Vector4` |

---

## 实用示例

### 调试命令

```gdscript
extends Node

func _ready():
    _register_debug_commands()

func _register_debug_commands():
    ZConsole.register_command(callable(self, "_cmd_damage"), "damage", "对玩家造成伤害")
    ZConsole.register_command(callable(self, "_cmd_heal"), "heal", "治愈玩家")
    ZConsole.register_command(callable(self, "_cmd_god"), "god", "切换无敌模式")
    ZConsole.register_command(callable(self, "_cmd_give"), "give", "给予物品")

    # 添加参数补全
    ZConsole.add_argument_autocomplete_source("give", 0, _get_item_list)

    ZConsole.info("调试命令已注册")

func _cmd_damage(amount: int):
    var player = get_node_or_null("/root/Game/Player")
    if player:
        player.take_damage(amount)
        ZConsole.info("造成 %d 点伤害" % amount)

func _cmd_heal(amount: int = 100):
    var player = get_node_or_null("/root/Game/Player")
    if player:
        player.heal(amount)
        ZConsole.info("治愈 %d 点" % amount)

func _cmd_god():
    var player = get_node_or_null("/root/Game/Player")
    if player:
        player.god_mode = !player.god_mode
        ZConsole.info("无敌模式: %s" % ("开启" if player.god_mode else "关闭"))

func _cmd_give(item_name: String):
    var player = get_node_or_null("/root/Game/Player")
    if player:
        player.inventory.add(item_name)
        ZConsole.info("获得: %s" % item_name)

func _get_item_list() -> PackedStringArray:
    return ["sword", "shield", "potion", "key", "gem"]
```

### 作弊菜单

```gdscript
extends Node

func _ready():
    _register_cheats()

func _register_cheats():
    # 添加别名
    ZConsole.add_alias("money", "set coins 999999")
    ZConsole.add_alias("max", "set health 999; set mana 999; set stamina 999")

    # 注册作弊命令
    ZConsole.register_command(callable(self, "_cmd_speed"), "speed", "设置速度倍率")
    ZConsole.register_command(callable(self, "_cmd_fly"), "fly", "飞行模式")
    ZConsole.register_command(callable(self, "_cmd_time_scale"), "timescale", "时间缩放")

func _cmd_speed(mult: float = 1.0):
    ZGlobalVar.set_value("speed_mult", mult)
    ZConsole.info("速度倍率: %.1fx" % mult)

func _cmd_fly():
    var flying = ZGlobalVar.get_value("fly_mode", false)
    ZGlobalVar.set_value("fly_mode", !flying)
    ZConsole.info("飞行模式: %s" % ("开启" if !flying else "关闭"))

func _cmd_time_scale(scale: float = 1.0):
    Engine.time_scale = scale
    ZConsole.info("时间缩放: %.1fx" % scale)
```

---

## 注意事项

- ⚠️ **快捷键冲突**：默认反引号键可能与其他输入法冲突，可在 ProjectSettings 中修改 `toggle_shortcut`
- ⚠️ **Release 构建**：`eval` 命令在 Release 构建中不可用
- ⚠️ **别名递归**：别名展开最多支持 1000 层深度
- ⚠️ **命令名格式**：命令名最多 4 个空格分隔的标识符（如 `admin player list`）
- ⚠️ **Eval 安全性**：Eval 执行任意表达式，仅用于开发环境
