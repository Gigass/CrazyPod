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
- [正在播放主题](../docs/CRAZYPOD_NOW_PLAYING_THEMES.zh-CN.md)

仓库源码按包类型分开：

- `apps/native-reference`：最小状态、事件和原生加载参考；
- `apps/capability-lab`：组件、Flex、资源、条件子树和生命周期；
- `apps/game2048`：Click Wheel 2048、原生状态持久化和平台内建领域逻辑。
- `themes/atelier-hifi`：纯 TSX 正在播放主题，使用平台播放桥接、动态封面、
  系统跑马灯和真实 PCM 峰值，并由 TSX 自行实现选项、队列、模式、歌词和进度
  调整面板。
- `themes/signal-one`：`Signal One` 完整视觉主题。主界面和全部弹窗使用统一
  的暖白工业产品语言，仍由纯 TSX 实现全部播放交互；默认主题不受影响。

常用命令：

```sh
npm ci --prefix tools/miniapp-builder
npm test --prefix tools/miniapp-builder
node tools/miniapp-builder/src/cli.mjs test miniapps/apps/capability-lab
./build-sim.sh --incremental
sh tests/run-miniapp-simulator-tests.sh
```

React Profile v1 是明确受限的源码子集，不是任意 TypeScript-to-C
编译器。支持和拒绝项必须以开发指南和编译器测试为准。
