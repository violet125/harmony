# 验证记录

## 已完成的静态验证（2026-10-14）

- `hello_c.cbp` 与 `hello_cpp.cbp` 通过 XML 解析，示例文件存在。
- `CMakeLists.txt` 包含 Qt Core / Gui / Widgets / Xml、CTest、真实子进程 fixture 与 offscreen GUI smoke test 配置。
- 依赖探测发现本机有 DevEco CMake/Ninja、MSVC、鸿蒙 Clang 与 MinGW；当前环境未提供可用 Qt 开发包和可直接调用的完整构建工具链。
- 已尝试取得 Qt 6.8.3 `qtbase`，下载镜像在传输中超时，未将未完整的 SDK 纳入交付；因此没有声称本机编译通过。
- 当前环境用 DevEco CMake 配置时因缺少 `nmake` / C++ 工具链而停止；这不是源码测试结果。

## 需要在 Qt 开发环境执行

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH="<Qt>/6.8.3/<kit>"
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

测试设计包括：

- 工程创建、`.cbp` 读写、未知节点保留、Windows 分隔符、空格路径、重复添加、移除不删除源文件。
- 非法 XML/DTD 拒绝且不替换当前工程、目标归属过滤、宏/自定义命令拒绝、C/C++ 混合目标拒绝。
- 成功/失败退出、启动失败、可重启、UTF-8 分块输出、标准输入、超时、主动停止、输出上限和重复启动。
- Linux 无 bubblewrap 时不回退；鸿蒙模式始终拒绝，直到显式接入设备侧服务。
- Qt offscreen 主窗口构造 smoke test。

## 鸿蒙真机验证尚未完成

按 [鸿蒙适配说明](OHOS_PORTING.md) 补齐目标 Qt/QPA、SDK、执行服务和应用打包后，必须在实际设备上执行验收清单并记录版本、设备、日志和复现步骤。当前交付不包含真机测试结论。
