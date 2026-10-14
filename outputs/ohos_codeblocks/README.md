# ohos_codeblocks

Qt Widgets / C++17 IDE 原型，围绕 Code::Blocks `.cbp` 工程文件实现「打开工程 → 编辑 → 保存 → 构建 → 运行 → 输出回显」。提供 picoc 进程接入、C 语法预检查以及 Linux bubblewrap 沙箱命令生成。

**交付定位：可编译的桌面原型。不是 Code::Blocks 官方移植版，也不是已经通过鸿蒙 PC 真机验收的产品。** 本项目独立重写 UI 和工程模型，不包含 Code::Blocks 或 picoc 上游源码。鸿蒙 Qt 平台插件、应用打包、文件授权、受限执行服务需要结合目标设备继续实现。

## 已实现

- Qt Widgets 主窗口：工程树、可停靠输出面板、多标签代码编辑、深色主题、行号、C/C++ 高亮、自动缩进、查找、撤销／重做。
- 工程管理：新建 C 控制台工程，读取和保存 `.cbp`，添加／移除文件引用，选择 Debug／Release 目标；保存保留未知 XML 节点及属性。
- 文件编辑：UTF-8 文本、修改标记、原子保存、关闭／切换工程时的未保存提示；4 MiB 文件限制。
- GCC／Clang 原生构建：支持纯 C 或纯 C++ 控制台目标、多源文件、头文件目录、链接库；成功后才能运行，避免运行旧产物。
- 进程管理：异步输出、标准输入、结束输入、停止、退出码、超时、输出上限、分块 UTF-8 解码。
- picoc 流程：当前 `.c` 文件快照 → GCC／Clang C99 语法检查 → picoc 解释执行 → 输出回显。picoc 单独安装并配置路径。
- 执行模式：Linux bubblewrap（默认，不可用即拒绝执行）、显式选择的可信本机模式、尚未接入的鸿蒙沙箱服务（始终拒绝执行）。

## 构建

依赖：CMake 3.21+、Qt 6.5+（Core / Gui / Widgets / Xml / Test）、支持 C++17 的编译器。推荐 Qt 6.8 LTS。Windows 上 Qt 构建套件与编译器 ABI 必须匹配，例如 Qt MSVC 2022 + MSVC。

在已配置编译器的终端中：

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH="你的Qt安装目录/6.8.3/msvc2022_64"
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Linux 示例：

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/opt/Qt/6.8.3/gcc_64
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/ohos_codeblocks --project examples/hello_c/hello_c.cbp
```

Windows 多配置构建启动：

```powershell
# 让动态链接器找到 Qt DLL；也可以使用 Qt 的 windeployqt 工具部署。
$env:PATH = "你的Qt目录\bin;$env:PATH"
.\build\Release\ohos_codeblocks.exe --project examples/hello_c/hello_c.cbp
```

如不需要测试：配置时增加 `-DBUILD_TESTING=OFF`。鸿蒙应用不可直接使用 Windows/Linux 的 Qt 二进制，请阅读 [鸿蒙适配说明](docs/OHOS_PORTING.md)。

## 体验流程

1. 打开 `examples/hello_c/hello_c.cbp`，或从菜单新建工程。
2. 在「工具 → 工具链与执行设置」填写 `gcc`、`g++` 和 `picoc` 的完整路径。路径不加引号，不附带命令参数。编译 IDE 可以用 MSVC，但原型内部构建用户代码使用 GCC/Clang 参数格式。
3. 修改并保存 `main.c`。F7 构建，F5 构建并运行。底部输入 `3` 回车，看到平方表。
4. 选择 `.c` 标签，按 F6 做语法检查并运行 picoc。Linux 需先安装 `bubblewrap`；Windows 如要体验，必须在设置中选择「本机执行：仅可信代码，无安全隔离」。
5. Shift+F5 停止。错误、退出码和超时原因显示在输出面板。
6. 用桌面 Code::Blocks 打开同一 `.cbp`。原型只修改工程引用，不生成专有工程格式。

快捷键：`Ctrl+O` 打开工程，`Ctrl+Shift+O` 打开源文件，`Ctrl+S` 保存，`Ctrl+Shift+S` 全部保存，`Ctrl+F` 查找。

## 兼容范围与限制

| 项目 | 当前行为 |
|---|---|
| `.cbp` XML | 支持 CodeBlocks_project_file / Project / Build / Target / Unit；未修改节点保留，缩进和属性顺序可能变化 |
| 路径 | 读取 Windows `\` 或 POSIX `/`；新增文件保存相对路径。跨系统绝对路径需要手动修正 |
| 构建目标 | 仅控制台 `type=1`；每次完整编译，无增量缓存 |
| 构建参数 | 每个 `<Add option="…">` 作为一个 argv 参数；请将多参数拆成多个 Add 节点 |
| 高级工程 | 不支持宏展开、自定义 Makefile、构建前后命令、混合 C/C++ 目标、资源编译、静态/动态库、逐文件编译参数、编译器继承策略；部分场景明确拒绝构建，完整兼容范围见架构说明 |
| picoc | 单 C 文件；不复制本地头文件、不支持 C++ 或工程链接；GCC 检查通过不保证 picoc 支持对应语法或库 |
| 运行限制 | 程序 10 秒、构建 120 秒、每次进程输出 1 MiB；界面保留最多 10,000 行；输入为按行管道，不是终端模拟器 |
| 沙箱 | bubblewrap 配置提供命名空间与受限文件视图；尚未完成安全审计或 Linux 实测；没有内存/CPU 配额，不适用于对外提供不可信代码执行服务 |
| 本机模式 | 编译器、语法检查、原生运行、可信 picoc 均无安全隔离；停止只保证终止直接子进程，不保证清理其所有后代进程 |
| 鸿蒙 PC | 桌面主流程代码与接口边界已提供，QPA/打包/执行服务/权限与真机验证尚待完成 |

原型编辑器不监控外部文件修改；同时使用其他编辑器修改同一文件时，请关闭后重新打开，避免覆盖。仅修改后保存的文件会规范化为 UTF-8、LF 换行。

## 项目结构

```text
src/
  mainwindow.*       菜单、工程树、编辑标签、构建／运行状态流
  codeeditor.*       代码编辑器、行号、语法高亮
  project.*          .cbp 读写、工程创建、构建参数生成
  runner.*           异步进程、输出限额、picoc 执行模式
resources/           主题资源
examples/            C 与 C++ 示例工程
tests/               工程与真实子进程测试
docs/                架构、鸿蒙适配、验收记录
```

更多信息：[架构与设计](docs/ARCHITECTURE.md)、[鸿蒙适配说明](docs/OHOS_PORTING.md)、[验证记录](docs/VALIDATION.md)、[第三方说明](THIRD_PARTY_NOTICES.md)。
