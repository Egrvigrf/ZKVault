# ZKVault

ZKVault 现在收敛为一个面向 Markdown 私密文章分发的加密协议核心。

它负责的主线能力只有这些：

- 私密文章数据模型
- `EncryptedPrivatePostBundle` 协议
- 加解密参考实现
- 版本校验与兼容性边界
- fixture 与兼容性测试基础
- 可选的作者侧开发工具

它不是 Halo 上层产品本身，也不再把“本地终端保险库”作为仓库主线定位。

## 仓库边界

这个仓库当前的主线只关注协议核心，不负责：

- Halo 编辑器交互
- Halo 插件安装与分发
- 浏览器端解密页面体验
- 泛化的富文本或任意文件加密标准
- 多租户服务端或 HTTP API

与 Halo 的关系应当是：

- `ZKVault`：协议核心、参考实现、兼容性测试
- `halo-private-posts`：Halo 适配层、编辑器集成、锁定页渲染、浏览器端解密

## 当前协议模型

作者侧输入文档是 `PrivatePostDocument`：

```json
{
  "metadata": {
    "slug": "notes/private-post",
    "title": "Private Post",
    "excerpt": "Short preview",
    "published_at": "2026-04-23T00:00:00Z"
  },
  "markdown": "# Private\nSecret content"
}
```

分发侧输出是 `EncryptedPrivatePostBundle`：

```json
{
  "version": 1,
  "payload_format": "markdown",
  "cipher": "aes-256-gcm",
  "kdf": "scrypt",
  "salt": "...",
  "data_iv": "...",
  "ciphertext": "...",
  "auth_tag": "...",
  "metadata": {
    "slug": "notes/private-post",
    "title": "Private Post",
    "excerpt": "Short preview",
    "published_at": "2026-04-23T00:00:00Z"
  }
}
```

当前 `v1` 的实现是“访问密码直接派生内容密钥后加密 Markdown 负载”。严格来说，这更接近口令型内容加密，而不是带独立 DEK 包裹字段的完整信封加密协议。后续若要引入 wrapped key，应通过版本升级完成。

## 目录结构

```text
docs/                 # 协议、架构、legacy 说明
integrations/         # 平台适配边界说明，不放协议核心代码
src/
├── content/          # 私密文章模型、bundle 协议、文件读写
├── crypto/           # 随机数、KDF、AES-GCM、hex 编解码
├── storage/          # 当前仅保留协议所需的原子写文件支持
├── terminal/         # 开发工具用的终端输入支持
└── tools/            # 协议开发工具入口
tests/                # 协议与工具测试
```

下面这些目录保留为历史探索资产，但不再代表仓库主线：

- `src/app/`
- `src/model/`
- `src/service/`
- `src/shell/`
- `src/tui/`
- `src/storage/json_storage.*`
- `src/storage/master_key_storage.*`

详见 [`docs/LEGACY.md`](docs/LEGACY.md)。

## 默认开发工具

默认构建会生成一个最小协议工具 `zkvault`：

```text
zkvault encrypt-post <document-path> <bundle-path>
zkvault decrypt-post-preview <bundle-path>
zkvault validate-document <document-path>
zkvault validate-bundle <bundle-path>
```

这些命令用于：

- 验证协议输入文档
- 生成 bundle fixture
- 本地预览解密结果
- 做 Halo 适配前的协议联调

## 构建

### 环境要求

- Linux
- CMake 3.16+
- 支持 C++20 的编译器
- OpenSSL 开发库

### 默认构建

```bash
cmake -S . -B build
cmake --build build
```

### 启用 legacy 原型

如果你还需要编译旧的本地保险库 CLI / shell / TUI 原型：

```bash
cmake -S . -B build -DZKVAULT_BUILD_LEGACY=ON
cmake --build build
```

启用后会额外生成 `zkvault-legacy`，用于保留历史探索能力，不作为当前产品主线。

## 文档

- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)：当前主线架构
- [`docs/PROTOCOL.md`](docs/PROTOCOL.md)：私密文章协议细节
- [`docs/LEGACY.md`](docs/LEGACY.md)：旧 CLI/shell/TUI 原型的保留策略
- [`integrations/halo/README.md`](integrations/halo/README.md)：Halo 适配边界
