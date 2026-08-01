# CrazyPod Mini App Native AOT 验证

## 自动门禁

在仓库根目录依次执行：

```sh
npm ci --prefix tools/miniapp-builder
npm test --prefix tools/miniapp-builder
sh tests/run-miniapp-host-tests.sh
./build-sim.sh --incremental
sh tests/run-miniapp-native-aot-simulator-test.sh
sh tests/run-miniapp-simulator-matrix.sh
sh tests/run-miniapp-e2e-reproduction.sh
./build-hw.sh --incremental
sh tests/run-crazypod-ui-host-tests.sh
sh tests/run-epub-host-tests.sh
python3 tools/check-crazypod-l10n.py --strict-bare
sh tests/check-crazypod-ui-architecture.sh
git diff --check
```

真机自动复现使用独立产物，不能覆盖或冒充正式构建：

```sh
CRAZYPOD_REPRO_DIAGNOSTICS=1 ./build-hw.sh
python3 tools/crazypod_miniapp_certify.py preflight /Volumes/CRAZYPOD \
  --release build-hw-ipod6g-repro/CrazyPod-6G.zip
```

正式产物固定在 `build-hw-ipod6g/`；一次性复现产物固定在
`build-hw-ipod6g-repro/`。认证工具默认只使用正式产物，诊断刷入必须显式传
`--release`，避免两个变体互相污染。

具体脚本名称以仓库现有入口为准；任何一步失败都不能称为完成。

## 覆盖目标

构建器测试：

- TypeScript 严格类型检查；
- React Profile 支持项和拒绝项；
- 确定性 C；
- CPK5 确定性、条目、profile 和资源；
- 项目脚手架和 CLI test/build/install/dev。

C 宿主测试：

- Native ABI 结构体和生命周期；
- CPK5 reader/verifier/installer；
- 原子安装记录；
- 状态、资源和输入队列；
- 2048 合并、计分、生成、无可移动判断和持久化；
- 生成 C 使用 `-Wall -Wextra -Werror`。

模拟器矩阵：

- Native reference 初始/点击；
- 2048 home/game/move/pause；
- Capability Lab home/controls/assets/lifecycle/modal；
- 每个场景产生有效 320×240 framebuffer；
- 关键状态的 framebuffer hash 必须不同。

复现回归：

1. 打开 2048；
2. 开始新游戏；
3. 连续执行 32 次四方向操作；
4. 长按 Menu 并确认退出；
5. 立即打开 Capability Lab；
6. 滚轮、进入页面、返回；
7. 退出 Lab；
8. 重复 5 轮。

必须记录：

- 每次移动从 button queue 到可见帧的延迟；
- p95 和最大延迟；
- 每轮 32 次输入的 p95，以及五轮中的最坏值；
- UI heartbeat 最大间隔；
- 包含冷加载的全阶段 heartbeat 最大间隔（只记录，不混入交互门禁）；
- button queue 和 miniapp queue 峰值；
- 关键阶段和 framebuffer CRC；
- 完成轮数和可见移动样本。

交互门禁：

- 每轮移动 p95 ≤ 180ms；
- 移动最大值 ≤ 300ms；
- 非冷启动阶段 heartbeat ≤ 250ms；
- 5 轮全部完成；
- 至少每轮 8 个可见移动样本；
- 2048 退出后 Lab 必须出现首帧并可交互。

首次动态加载原生应用属于 cold start，单独记录，不混入持续交互 p95。

## 真机

硬件构建必须零 warning。刷入后至少执行：

- 冷启动 2048 和 Lab；
- 上述 5 轮切换；
- 2048 快速连续操作；
- USB 插拔、退出和重新进入；
- 播放音乐同时运行两个应用；
- 重启后 2048 状态恢复；
- 安装损坏/错 target/错 ABI CPK5 均被拒绝。

自动化模拟器通过不等于真机通过。若当前会话没有连接和刷入设备，状态必须
明确写“真机未执行”，不能推断成功。
