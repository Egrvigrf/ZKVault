# Architecture

## Mainline

ZKVault 的当前主线是“Markdown 私密文章分发的加密协议核心”。

默认构建只围绕这条主线组织：

- `zkvault_core`
  - `src/content/`
  - `src/crypto/`
  - `src/storage/atomic_file.*`
- `zkvault_terminal_support`
  - `src/terminal/prompt.*`
- `fixtures/private-post/`
  - 固定协议向量
  - 跨语言兼容性对照样本
- `zkvault`
  - `src/tools/private_post_cli.cpp`

对应职责分别是：

- `core`：协议模型、版本校验、加解密、bundle 文件读写
- `terminal_support`：开发工具所需的安全输入能力
- `fixtures`：协议真源样本，供 Halo 等适配层做一致性验证
- `zkvault`：协议验证、加密、解密预览等作者侧最小工具

## Layer Boundary

推荐的分层边界如下：

1. Protocol Core
   - `PrivatePostDocument`
   - `EncryptedPrivatePostBundle`
   - schema validation
   - crypto implementation
2. Developer Tools
   - `encrypt-post`
   - `decrypt-post-preview`
   - `validate-document`
   - `validate-bundle`
3. Platform Adapters
   - Halo plugin
   - future adapters for other static/blog platforms
4. Legacy Exploration
   - old local vault CLI
   - shell runtime
   - TUI prototype

关键原则是：平台适配层可以围绕协议做 UI 和集成，但不能把平台逻辑反灌回协议核心。

## Data Split

协议当前显式区分：

- public metadata
  - `slug`
  - `title`
  - `excerpt`
  - `published_at`
- secret payload
  - `markdown`

这样做的目的不是做一个通用内容系统，而是服务私密 Markdown 文章的分发场景：

- 平台可在未解密前显示标题和摘要
- 浏览器端或插件端只在需要时解密正文
- bundle 本身可以被 Halo 之外的适配层复用

## Versioning Rule

`v1` 当前固定以下契约：

- payload format: `markdown`
- cipher: `aes-256-gcm`
- kdf: `scrypt`

任何下面这些变化都应通过显式版本升级处理：

- 更换算法
- 引入 wrapped DEK
- 调整 public metadata 语义
- 修改 secret payload 结构

不要在同一版本下做“看起来兼容”的隐式扩展。

## Halo Boundary

对于 Halo，推荐边界是：

- `ZKVault`
  - bundle 协议
  - 参考实现
  - fixture 与兼容性测试
  - 可选作者侧工具
- `halo-private-posts`
  - 编辑器集成
  - 文章保存钩子
  - 浏览器端解密与渲染
  - 插件安装与社区发布

这意味着 Halo 插件不应依赖本地守护进程或本地 CLI 才能工作。运行时需要的协议逻辑，应以内嵌库或前端实现的方式进入插件交付物。

## Legacy Handling

`src/app/`、`src/model/`、`src/service/`、`src/shell/`、`src/tui/` 仍然保留，但只作为 legacy 资产存在。

它们的处理原则是：

- 不删除历史
- 不再作为默认构建
- 不再作为 README 主叙事
- 需要时通过 `ZKVAULT_BUILD_LEGACY=ON` 显式启用

详见 [`docs/PROTOCOL.md`](docs/PROTOCOL.md) 和 [`docs/LEGACY.md`](docs/LEGACY.md)。
