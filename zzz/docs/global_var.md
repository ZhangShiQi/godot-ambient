# ZGlobalVar 全局变量管理器

> 运行时全局变量存储系统，支持任意 Variant 类型、作用域（命名空间）和 JSON 持久化。

---

## 快速开始

```gdscript
# 任何脚本随时调用，无需初始化
ZGlobalVar.set_value("score", 0)
var current = ZGlobalVar.get_value("score", 0)

# 使用作用域组织变量
ZGlobalVar.set_value("player.health", 100)
ZGlobalVar.set_value("player.pos", Vector2(10, 20))

# JSON 持久化
var json_str = ZGlobalVar.export_json()
ZGlobalVar.import_json(json_str)
```

---

## 作用域系统

变量名支持点分隔的层级路径，类似于命名空间：

```gdscript
# 顶级变量
ZGlobalVar.set_value("score", 0)

# player 作用域下的变量
ZGlobalVar.set_value("player.health", 100)
ZGlobalVar.set_value("player.pos", Vector2(10, 20))
ZGlobalVar.set_value("player.name", "Hero")

# enemy 作用域下的变量
ZGlobalVar.set_value("enemy.health", 50)
ZGlobalVar.set_value("enemy.type", "goblin")

# settings 作用域
ZGlobalVar.set_value("settings.volume", 0.8)
ZGlobalVar.set_value("settings.music", true)
```

### 作用域规则

- 变量名不能以点开头或结尾
- 变量名不能包含 `..`
- 不能同时存在 `player` 和 `player.health`（层级冲突）
- 空字符串表示全局作用域（匹配所有变量）

### 作用域查询

```gdscript
# 获取所有变量
var all = ZGlobalVar.get_names()

# 获取 player 作用域下的所有变量
var player_vars = ZGlobalVar.get_names_in_scope("player")
# 结果: ["player.health", "player.name", "player.pos"]

# 获取所有变量（返回字典）
var all_data = ZGlobalVar.get_all()

# 获取 player 作用域下的所有变量（返回字典）
var player_data = ZGlobalVar.get_all_in_scope("player")
```

### 清除作用域

```gdscript
# 通过 import_json 的 clear_scope 参数清除
ZGlobalVar.import_json("{}", "player", true)  # 清除 player 作用域下的所有变量
```

---

## API 参考

### 基础操作

#### `void set_value(StringName name, Variant value)`

设置变量。变量存在则覆盖。

```gdscript
ZGlobalVar.set_value("player_name", "Hero")
ZGlobalVar.set_value("health", 100)
ZGlobalVar.set_value("position", Vector2(100, 200))
ZGlobalVar.set_value("inventory", ["sword", "shield", "potion"])

# 使用作用域
ZGlobalVar.set_value("player.health", 100)
ZGlobalVar.set_value("player.weapon.damage", 50)
```

> **注意**：变量名必须符合层级路径规范，不能以点开头/结尾，不能包含 `..`。

#### `Variant get_value(StringName name, Variant default_value = Variant())`

获取变量值。变量不存在时返回默认值。

```gdscript
var name = ZGlobalVar.get_value("player_name", "Unknown")
var health = ZGlobalVar.get_value("health", 0)
var pos = ZGlobalVar.get_value("position", Vector2.ZERO)
```

#### `bool has_value(StringName name)`

检查变量是否存在。

```gdscript
if ZGlobalVar.has_value("save_data"):
    load_game()
```

#### `void remove_value(StringName name)`

删除指定变量。

```gdscript
ZGlobalVar.remove_value("temp_data")
```

#### `void clear()`

清空所有变量。

```gdscript
ZGlobalVar.clear()  # ⚠️ 谨慎使用
```

### 变量查询

#### `PackedStringArray get_names()`

获取所有变量名（按字母排序）。

```gdscript
var all_vars = ZGlobalVar.get_names()
for var_name in all_vars:
    print("%s = %s" % [var_name, ZGlobalVar.get_value(var_name)])
```

#### `PackedStringArray get_names_in_scope(String scope = "")`

获取指定作用域下的所有变量名。

```gdscript
# 获取 player 作用域下的所有变量
var player_vars = ZGlobalVar.get_names_in_scope("player")

# 空字符串获取所有变量（等同于 get_names()）
var all_vars = ZGlobalVar.get_names_in_scope("")
```

#### `Dictionary get_all()`

获取所有变量（键值对字典）。

```gdscript
var all_data = ZGlobalVar.get_all()
for key in all_data:
    print("%s: %s" % [key, all_data[key]])
```

#### `Dictionary get_all_in_scope(String scope = "")`

获取指定作用域下的所有变量（键值对字典）。

```gdscript
var player_data = ZGlobalVar.get_all_in_scope("player")
for key in player_data:
    print("%s: %s" % [key, player_data[key]])
```

### JSON 持久化

#### `String export_json(String scope = "", String indent = "\t")`

将变量导出为 JSON 字符串。

```gdscript
# 导出所有变量
var all_json = ZGlobalVar.export_json()

# 导出指定作用域
var player_json = ZGlobalVar.export_json("player")

# 自定义缩进
var compact = ZGlobalVar.export_json("", "  ")
```

**导出的 JSON 格式**：

```json
{
    "score": {"__zgv_value__": 0},
    "player": {
        "health": {"__zgv_value__": 100},
        "pos": {"__zgv_value__": [10, 20]},
        "name": {"__zgv_value__": "Hero"}
    }
}
```

#### `Error import_json(String json, String scope = "", bool clear_scope = false)`

从 JSON 字符串导入变量。

```gdscript
# 导入到全局作用域（合并，不会清除现有变量）
var err = ZGlobalVar.import_json(save_json)

# 导入到指定作用域
var err = ZGlobalVar.import_json(player_json, "player")

# 导入前清除目标作用域
var err = ZGlobalVar.import_json(new_data, "player", true)

if err != OK:
    push_error("导入失败: " + str(err))
```

**导入规则**：
- JSON 根节点必须是对象（字典）
- 支持 `__zgv_value__` 包装的值
- 导入时不会覆盖已有变量（除非存在层级冲突）
- `clear_scope = true` 时会先清除目标作用域下的所有变量

---

## 使用场景

### 跨场景数据传递

```gdscript
# Scene1.gd
ZGlobalVar.set_value("current_level", 3)
ZGlobalVar.set_value("score", 0)

# Scene2.gd
var level = ZGlobalVar.get_value("current_level", 1)
```

### 游戏状态管理（推荐使用作用域）

```gdscript
func save_game_state():
    ZGlobalVar.set_value("game.health", $Player.health)
    ZGlobalVar.set_value("game.pos", $Player.global_position)
    ZGlobalVar.set_value("game.inventory", $Player.inventory)

func load_game_state():
    $Player.health = ZGlobalVar.get_value("game.health", 100)
    $Player.global_position = ZGlobalVar.get_value("game.pos", Vector2.ZERO)
    $Player.inventory = ZGlobalVar.get_value("game.inventory", [])

# 保存到文件
func save_to_file(path: String):
    var json_str = ZGlobalVar.export_json("game")
    FileAccess.open(path, FileAccess.WRITE).store_string(json_str)

func load_from_file(path: String):
    var file = FileAccess.open(path, FileAccess.READ)
    if file:
        ZGlobalVar.import_json(file.get_as_text(), "game", true)
```

### 模块化数据管理

```gdscript
# AudioManager.gd
func _ready():
    ZGlobalVar.set_value("audio.volume", 0.8)
    ZGlobalVar.set_value("audio.music", true)
    ZGlobalVar.set_value("audio.sfx", true)

# SettingsManager.gd
func _ready():
    ZGlobalVar.set_value("settings.difficulty", "normal")
    ZGlobalVar.set_value("settings.language", "en")

# 获取所有设置
var settings = ZGlobalVar.get_all_in_scope("settings")
```

---

## 注意事项

- ⚠️ **变量名规范**：不能以点开头/结尾，不能包含 `..`
- ⚠️ **层级冲突**：不能同时存在 `player` 和 `player.health`
- ⚠️ **无自动持久化**：变量仅存储在内存中，关闭游戏后丢失。使用 `export_json` / `import_json` 手动持久化
- ⚠️ **命名冲突**：避免与项目中其他全局系统使用相同名称
- ⚠️ **JSON 格式**：导出的 JSON 使用 `__zgv_value__` 包装值以保留类型信息
