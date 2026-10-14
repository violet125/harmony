# 第三方与来源说明

- **Code::Blocks**：工程概念及 `.cbp` XML 互通对象，官网 https://www.codeblocks.org/ 。本原型独立实现，不包含或链接 Code::Blocks 上游代码，不隶属 Code::Blocks 项目。若后续复用其源码，应按对应版本许可审查义务。
- **Qt 6**：本项目依赖 Qt Core / Gui / Widgets / Xml / Test。请根据使用的模块和发行方式遵循其 LGPL/GPL 或商业许可。源码包不内嵌 Qt SDK；如分发动态库，需要附许可与满足相应义务。https://www.qt.io/licensing/
- **picoc**：可选外部 C 解释器，参考上游 https://github.com/jpoirier/picoc 。源码包未内嵌 picoc，不由 CMake 自动联网下载；请从可信上游构建并遵循所选版本的 LICENSE。原型命令调用方式为 `picoc main.c`。
- **bubblewrap**：可选 Linux 运行隔离工具，https://github.com/containers/bubblewrap 。源码包未内嵌。

本交付不为用户选择整个项目的开源许可证；正式发布前应由项目所有者确定自有代码许可并补充版权归属。
