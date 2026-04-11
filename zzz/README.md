# ZZZ Godot 扩展工具库

这是一个 Godot 引擎的 C++ 扩展模块，提供两个实用的运行时工具。

---

## 模块列表

| 模块 | 文档 | 描述 |
|------|------|------|
| 全局变量 | [docs/global_var.md](docs/global_var.md) | 运行时全局变量存储 |
| 控制台 | [docs/console.md](docs/console.md) | 类 DOS/Quake 风格的运行时控制台 |

---

## 安装

```bash
# 使用 SCons 编译
scons -j$(nproc)
```

编译后会生成 `.gdextension` 文件或动态库，复制到你的项目目录即可。

---

## 快速开始

```gdscript
# 全局变量 - 任何脚本随时调用
ZGlobalVar.set_value("score", 0)
var score = ZGlobalVar.get_value("score", 0)

# 控制台 - 打印日志
ZConsole.info("游戏开始")
ZConsole.warn("警告信息")
ZConsole.error("错误信息")

# 控制台 - 注册命令
ZConsole.register_command(callable(self, "_cmd_heal"), "heal", "治愈玩家")

# 控制台 - 执行命令
ZConsole.execute_command("set score 100")
```

---

## 特性概览

### ZGlobalVar
- 任意 Variant 类型支持
- 单例模式，随时访问
- 无需初始化

### ZConsole
- 命令注册与别名
- 自动补全
- 命令历史
- 脚本执行（`.lcs` 文件）
- Eval 表达式计算
- 内置调试命令

---

## AI 集成

详见各模块文档。推荐 Prompt 模板：

```
请使用 ZZZ Godot 扩展库帮我实现：
1. 使用 ZGlobalVar 存储游戏分数
2. 注册一个 "score" 命令显示分数
3. 添加 "add_score" 命令增加分数
```
