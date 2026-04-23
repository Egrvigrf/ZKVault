# Halo Integration

这个目录只用于描述 Halo 适配边界，不承载协议核心实现。

## Boundary

应该留在 `ZKVault` 的内容：

- `PrivatePostDocument` 数据模型
- `EncryptedPrivatePostBundle` 协议
- 加解密参考实现
- 版本校验
- fixture 与兼容性测试
- 可选作者侧开发工具

应该留在 Halo 适配层的内容：

- 编辑器侧写作体验
- 保存前加密流程接入
- 文章元数据映射
- 锁定页渲染
- 浏览器端解密与自动重新锁定
- 一键安装和社区发布相关资产

## Delivery Constraint

如果目标是 Halo 社区可一键安装的插件，那么运行时不能依赖本地 `zkvault` 可执行文件、本地 sidecar 或作者机器上的额外服务。

更合理的交付方式是：

- `ZKVault` 提供协议文档、fixture 和参考实现
- Halo 插件把必要的协议能力打包进自身
- 浏览器端负责访问密码输入、解密和渲染

## Collaboration Rule

Halo 适配层可以复用 ZKVault 的协议设计，但不应把平台特有页面、组件或管理逻辑放回这个仓库。
