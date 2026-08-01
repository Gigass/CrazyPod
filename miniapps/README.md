# CrazyPod Mini Apps

当前唯一实现是 CPK5、Native ABI 1、React Profile v1：

```text
TypeScript/TSX → AOT C → app.arm/app.dylib → Native ABI → host-owned LVGL
```

设备端不运行 JavaScript 引擎、React、Solid 或 Virtual DOM。

文档：

- [Native AOT 架构](../docs/CRAZYPOD_MINIAPP_NATIVE_AOT_ARCHITECTURE.zh-CN.md)
- [开发指南](../docs/CRAZYPOD_MINIAPP_NATIVE_AOT_DEVELOPMENT.zh-CN.md)
- [CPK5 格式](../docs/CRAZYPOD_MINIAPP_CPK5_FORMAT.zh-CN.md)
- [验证流程](../docs/CRAZYPOD_MINIAPP_NATIVE_AOT_VERIFICATION.zh-CN.md)

仓库应用：

- `native-reference`：最小状态、事件和原生加载参考；
- `capability-lab`：组件、Flex、资源、条件子树和生命周期；
- `game2048`：Click Wheel 2048、原生状态持久化和平台内建领域逻辑。

常用命令：

```sh
npm ci --prefix tools/miniapp-builder
npm test --prefix tools/miniapp-builder
node tools/miniapp-builder/src/cli.mjs test miniapps/capability-lab
./build-sim.sh --incremental
sh tests/run-miniapp-simulator-tests.sh
```

React Profile v1 是明确受限的源码子集，不是任意 TypeScript-to-C
编译器。支持和拒绝项必须以开发指南和编译器测试为准。
