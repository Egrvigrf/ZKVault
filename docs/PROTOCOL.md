# Protocol

## Scope

ZKVault 当前协议只面向 Markdown 私密文章分发，不试图定义通用文档加密标准。

协议核心由两个对象组成：

- `PrivatePostDocument`
- `EncryptedPrivatePostBundle`

## PrivatePostDocument

作者侧明文文档结构：

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

约束：

- `metadata.slug` 不能为空
- `slug` 只允许字母、数字、`.`、`-`、`_` 和 `/`
- `slug` 不允许空 path segment，也不允许以 `/` 开头或结尾
- `metadata.title` 不能为空
- `markdown` 作为密文负载进入加密流程

## EncryptedPrivatePostBundle

分发侧 bundle 结构：

```json
{
  "version": 1,
  "payload_format": "markdown",
  "cipher": "aes-256-gcm",
  "kdf": "scrypt",
  "salt": "<hex>",
  "data_iv": "<hex>",
  "ciphertext": "<hex>",
  "auth_tag": "<hex>",
  "metadata": {
    "slug": "notes/private-post",
    "title": "Private Post",
    "excerpt": "Short preview",
    "published_at": "2026-04-23T00:00:00Z"
  }
}
```

其中：

- `metadata` 保持明文，供分发平台做路由、标题、摘要展示
- `markdown` 被序列化后进入密文负载
- `salt`、`data_iv`、`ciphertext`、`auth_tag` 都使用 hex 编码

## Version 1 Contract

当前代码会强校验以下字段：

- `version == 1`
- `payload_format == "markdown"`
- `cipher == "aes-256-gcm"`
- `kdf == "scrypt"`

这意味着：

- `v1` 不是“尽量兼容”的松散格式
- 任何参数切换都应该显式升级版本
- Halo 适配层不能绕过这些字段约束自行扩展

## Crypto Model

`v1` 的流程是：

1. 用户输入访问密码
2. 使用 `scrypt` 从访问密码和 `salt` 派生 32 字节密钥
3. 使用 `AES-256-GCM` 加密私密负载
4. 输出 public metadata 和密文 bundle

当前 bundle 内没有独立 DEK 的包裹字段，所以这不是严格意义上的“wrapped DEK envelope encryption”。如果后续要支持：

- 访问控制升级
- 多接收方
- 作者密钥轮换
- 平台侧只搬运密文、不直接依赖口令

应该通过新的 bundle 版本引入新的字段，而不是在 `v1` 上做隐式扩展。

## Compatibility Guidance

建议后续演进遵守以下规则：

- 新算法或新字段语义变化时升级 `version`
- 对旧版本只做显式读取和校验，不做静默修复
- fixture 目录按 `fixtures/private-post/v{n}/` 组织
- 适配层测试应至少覆盖一个跨仓库 fixture 解密用例
