# Legacy

## Why Legacy Exists

仓库早期曾把 ZKVault 当作“本地终端保险库产品”来探索，因此积累了一批与当前主线不再一致的代码：

- CLI 条目管理
- 会话式 shell
- 全屏 TUI
- 本地主密码与条目存储模型

这些内容并不等于无效代码，但它们不应该继续定义 ZKVault 的产品定位。

## What Is Kept

以下目录保留在仓库中，作为历史探索和后续参考：

- `src/app/`
- `src/model/`
- `src/service/`
- `src/shell/`
- `src/tui/`
- `src/storage/json_storage.*`
- `src/storage/master_key_storage.*`
- `src/main.cpp`

保留这些代码的目的：

- 记录已经试过的交互和本地 runtime 设计
- 为未来作者侧工具提供可回收的实现素材
- 避免为了“主线收敛”而抹掉探索历史

## What Changes Now

从当前阶段开始：

- 默认文档不再把 legacy 代码当成主产品介绍
- 默认构建不再自动编译 legacy CLI/shell/TUI
- legacy 代码仅在显式开启 `ZKVAULT_BUILD_LEGACY=ON` 时参与构建
- legacy 二进制输出名为 `zkvault-legacy`

## When Legacy Can Be Reused

如果未来确实需要作者侧本地工具，可以从 legacy 中回收这些东西：

- 终端密码输入
- 会话状态机
- 交互确认流
- 本地预览或调试界面原型

但前提是它们服务于“协议开发工具”或“作者侧辅助工具”，而不是重新把仓库拉回一个独立保险库产品叙事。
