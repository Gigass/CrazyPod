# CrazyPod LCD 撕裂：根因、最终方案与维护约束

状态：2026-08-22 已在一台 iPod Classic 6G、type 1 面板上完成实机验证。
播放音乐时持续转动 Home CoverFlow、保持手指静止在转盘上以及松手恢复播放
动画，肉眼均未再发现撕裂。该结果只证明这台实机和当前代码路径；其他面板批次仍需
重复验证。

## 结论

撕裂的根因不是 CoverFlow 动画速度，也不是 LVGL 动画是否足够平滑。根因是 LCD DMA
提交时机与面板扫描不同步，以及同步运动区域被普通 LVGL 脏区污染。

最终方案由四层组成：

1. 驱动运行时识别并验证面板 TE 信号。
2. 根据更新矩形的纵向位置，在扫描越过该区域后启动 DMA。
3. 提交调度器把 Home、Music CoverFlow 和普通 LVGL 更新分开排队。
4. 手指接触 Home 转盘期间，只提交 TE 同步的 Home 帧；普通局部更新延迟到松手后。

任何一层缺失，都可能重新暴露撕裂。

## 问题如何暴露

早期 Home CoverFlow 在不播放音乐时较稳定，播放音乐后明显撕裂。播放状态增加了多个
独立刷新源：

- Home 胶囊音波约每 100 ms 更新一次；
- 歌曲名和歌手名跑马灯由 LVGL 动画持续推进；
- 播放进度每 250 ms 检查并在宽度变化时失效；
- 状态栏、通知和触摸前已经排队的脏区也可能产生普通局部更新。

这些区域与 Home 图标共享同一个 LCD 和 DMA 提交通道。Home 图标停止运动，不代表
播放页面停止产生更新。手指仍放在转盘上时，普通局部 DMA 一旦恢复，就可能撞上面板
正在扫描的区域。

## 硬件事实

iPod Classic LCD 面板支持 TE/FMARK。公开逆向测量给出的典型周期约为 14.72 ms，
即约 67.9 Hz，脉冲宽度约 60 µs。CrazyPod 实机探测确认当前设备上的有效输入为
`PDAT(6)` bit 7。

驱动不会只凭引脚名称相信该信号。启动探测会采样 LCD 状态和 GPIO 输入，并验证：

- 周期位于 12–18 ms；
- 至少出现多个稳定周期和边沿；
- 周期抖动在允许范围内；
- 高、低电平占空比符合窄脉冲特征。

只有验证通过后，局部同步路径才使用 GPIO TE。公开资料主要记录 type 2 面板；当前
type 1 实机验证是必要证据，不能据此假设所有面板批次完全相同。

参考：

- [iPod Classic 逆向测量](https://github.com/LXK-98/ipod-classic-reverse-engineering)
- [Rockbox S5L8702 LCD 驱动](https://github.com/Rockbox/rockbox/blob/master/firmware/target/arm/s5l8702/lcd-s5l8702.c)

## 最终实现

### 1. 按区域计算 TE 相位

直接在 TE 边沿启动局部 DMA 仍然会撕裂。面板从上向下扫描；如果 DMA 写入追上扫描，
裂线会稳定出现在屏幕某个高度。测试中曾出现固定在底部约五分之一处的裂线。

当前驱动使用更新矩形的 `y + height` 计算目标相位，等待扫描越过矩形底部及保护行后
再启动 DMA：

```text
target_phase = te_period × min(y + height + guard_lines, LCD_HEIGHT)
               / LCD_HEIGHT
```

Home 使用 12 行保护区，Music CoverFlow 使用 2 行保护区。两个值来自各自区域大小和
实机结果，不能在不了解传输时长与区域几何的情况下合并成一个常量。

实现位于
[`firmware/target/arm/s5l8702/lcd-s5l8702.c`](../firmware/target/arm/s5l8702/lcd-s5l8702.c)：

- `displaylcd_probe_te_inputs()`：扫描并验证 TE 输入；
- `displaylcd_wait_te_phase()`：按矩形底部计算安全相位；
- `lcd_update_rect_frame_sync()`：Home 同步提交；
- `lcd_update_rect_music_sync()`：Music CoverFlow 同步提交。

### 2. 分离同步区域与普通脏区

LVGL 会把同一轮刷新中的脏区合并。若把 Home 运动带和底部音波合并成一个大矩形，
原本正确的同步区域会被扩大，TE 相位和 DMA 时长随之改变。

[`apps/crazypod/crazypod_frameclock.c`](../apps/crazypod/crazypod_frameclock.c)
把提交分成三类：

| 类型 | 来源 | LCD 路径 |
| --- | --- | --- |
| `PRESENT_SYNC_HOME` | Home 原生图标带 | `lcd_update_rect_frame_sync()` |
| `PRESENT_SYNC_MUSIC` | Music CoverFlow 原生区域 | `lcd_update_rect_music_sync()` |
| `PRESENT_SYNC_NONE` | 普通 LVGL 脏区 | `lcd_update_rect()` |

同步请求拥有下一帧。普通脏区进入独立的 deferred 槽，不能与同步矩形合并。同步帧完成
后，调度器才提升延迟区域。

Home 原生区域由
[`apps/crazypod/ui/shell/crazypod_desktop_native.c`](../apps/crazypod/ui/shell/crazypod_desktop_native.c)
提交；Music CoverFlow 由
[`apps/crazypod/crazypod_coverflow.c`](../apps/crazypod/crazypod_coverflow.c)
提交。LVGL 的普通 flush 通过
[`apps/crazypod/platform/crazypod_platform_display.c`](../apps/crazypod/platform/crazypod_platform_display.c)
进入普通队列。

### 3. 使用真实触摸状态

`wheel_status()` 表示最近一次有效的转盘位置，不表示手指是否持续接触。非位置数据包
可能把位置写成 `-1`。用该值和 60 ms 超时推断触摸，会在手指静止时错误地判定松手，
从而恢复音波和跑马灯。

当前 click-wheel 驱动导出 `wheel_touch_status()`，直接返回驱动维护的真实触摸位。
Home 只用 `wheel_status()` 计算旋转增量，只用 `wheel_touch_status()` 判断接触与释放。

实现位于：

- [`firmware/target/arm/ipod/button-clickwheel.c`](../firmware/target/arm/ipod/button-clickwheel.c)
- [`apps/crazypod/ui/shell/crazypod_desktop.c`](../apps/crazypod/ui/shell/crazypod_desktop.c)

### 4. 触摸期间阻止所有普通局部提交

只暂停音波和跑马灯仍不完整。播放进度、状态栏、通知和旧脏区仍可能触发普通 DMA。

组合根在采样输入后调用 `crazypod_present_set_home_interaction()`。真实触摸保持期间：

- `PRESENT_SYNC_HOME` 正常提交，图标运动保持流畅；
- 非全屏的 `PRESENT_SYNC_NONE` 保留在队列；
- 全屏更新仍走驱动的帧起点同步路径；
- 松手后，延迟的普通区域恢复提交。

音波和跑马灯同时停止自身动画，减少无用渲染。提交层门控才是防止漏网刷新到达 LCD
的最终保证。

## 无效或不完整的方案

以下尝试曾改变症状，但没有解决根因：

- **限制为 33 FPS**：降低撕裂出现频率，不会让软件时钟与约 67.9 Hz 面板同相。
- **限制 CoverFlow 最大速度**：改变位移量，不改变 DMA 与扫描的相位关系。
- **改用 LVGL Animation**：改变动画计算者，不改变 LCD 提交时机。
- **取消音波帧率限制**：增加失效和 DMA 请求，撕裂风险更高。
- **只等待 TE 边沿**：忽略更新矩形位置和传输时间，可能形成固定裂线。
- **只暂停音波**：跑马灯、进度和其他普通脏区仍会提交。
- **暂停音波和跑马灯**：仍遗漏播放进度、状态栏、通知和已排队脏区。
- **用 `wheel_tracking` 代表触摸**：位置状态与真实接触状态不是同一信号。

这些方案可以作为诊断实验，不能作为生产修复。

## 维护约束

修改显示、Home 或 Music CoverFlow 时必须遵守以下规则：

1. 动画逻辑只负责生成画面；提交调度器决定画面何时到达面板。
2. 原生运动区域必须使用对应的 `crazypod_present_queue_*_rect()`，禁止直接调用
   `lcd_update_rect()`。
3. 同步运动区域与普通 LVGL 区域必须保留两个队列，禁止合并成一个包围矩形。
4. 不得用 FPS、速度、惯性或延时常量代替 TE 同步。
5. 不得用位置事件、最后活动时间或超时推断手指接触。
6. Home 触摸门控必须覆盖所有普通局部提交，而不是维护一份动画组件白名单。
7. 新增 Home 常驻动画、通知或播放组件时，先确认它进入哪种 present 类型。
8. 调整 Home/Music 区域高度后，必须重新检查 DMA 时长、目标相位和保护行。
9. TE 探测失败或超时时必须暴露诊断结果；不能静默声称局部同步仍然有效。
10. 音频负载会放大时序问题，但不能把 CPU 降频、动画降帧或音频缓冲调整当作
    撕裂修复。

## 验证要求

### 自动测试

至少运行：

```sh
sh tests/run-crazypod-ui-host-tests.sh
sh tests/check-crazypod-ui-architecture.sh
git diff --check
./build-hw.sh --incremental
```

[`tests/crazypod_frameclock_host_test.c`](../tests/crazypod_frameclock_host_test.c)
必须继续覆盖：

- Home/Music 请求进入各自同步路径；
- Home 同步矩形不与播放胶囊合并；
- 连续 Home 帧优先于 deferred LVGL 区域；
- 手指接触时普通局部更新不提交；
- Home TE 帧在接触期间仍可提交；
- 松手后 deferred 区域恢复提交。

### 实机矩阵

模拟器不能证明无撕裂。每次改动相关代码后，至少在真机执行：

| 场景 | 检查点 |
| --- | --- |
| Home，未播放，连续转动 | 图标带无移动裂线 |
| Home，播放中，连续转动 | 音频负载下无裂线 |
| Home，播放中，手指静止按住 | 音波和跑马灯停止；无普通局部提交造成的裂线 |
| Home，播放中，松手 | 音波等待一个完整周期后恢复；延迟区域只刷新一次 |
| Music CoverFlow，连续转动与松手 | CoverFlow 区域和上下元数据无裂线 |
| 通知、锁屏、解锁、切歌 | 全屏或覆盖层更新不破坏同步队列 |

测试应持续数十秒，覆盖短转、长转、反向转动、播放解码和存储读取。一次短暂观察
不能证明时钟不会漂移。

## 回归诊断顺序

出现撕裂时，不要先调整动画。按以下顺序检查：

1. 确认撕裂发生在 Home、Music 还是普通 LVGL 区域。
2. 记录提交类型、矩形坐标和是否为 deferred 请求。
3. 检查 TE 输入是否有效、周期是否稳定、是否发生 timeout。
4. 检查 DMA 起始相位、目标相位、结束相位和传输时长。
5. 检查普通脏区是否与同步矩形合并。
6. 检查真实 `wheel_touch_status()` 与 `home_interaction_active` 是否连续保持。
7. 最后再评估 CPU、音频解码和存储活动是否推迟 DMA。

临时诊断日志应包含 `sync mode`、矩形、TE 周期、目标相位、DMA 起止相位、等待时间、
timeout、deferred 状态和真实触摸位。确认问题后删除高频磁盘日志，避免日志本身改变
存储负载和时序。

## 代码考古

- `4876016cd3`：引入主要 TE、Home/Music 分离提交和相关 UI 优化。
- `fe74bacacd`：提交真实 click-wheel 触摸状态接口。
- `9cf1f5e74b`：触摸期间阻止所有普通局部 DMA，并增加回归测试。

提交号只用于追踪历史。本文的维护约束和当前源码行为才是长期合同。
