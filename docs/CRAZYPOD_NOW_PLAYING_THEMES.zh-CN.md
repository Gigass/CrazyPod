# CrazyPod 正在播放主题

正在播放主题能力始于 Native ABI 1.4；ABI 1.5 提供
`Text numberOfLines` 动态元数据裁切，ABI 1.6 提供双声道播放峰值，
ABI 1.7 提供播放队列、明确的收藏/播放模式写入、歌词窗口、相对跳转和
系统跑马灯复用；ABI 1.8 增加可识别的原生 `Modal`、弹窗内 Menu 返回和
合并后的滚轮 `steps` 增量；ABI 1.9 增加平台统一封面提交和自动刷新，ABI 1.10
增加主题状态栏，当前 ABI 1.11 增加字体角色与按主题声明的原图封面解码尺寸。
可安装 CPK5 能替换“正在播放”页面，
默认主题仍是固件现有 C 页面；安装主题不会自动启用，用户必须在“自定义 > 主题”中选择。
选择“默认”后立即恢复固件页面。

## 开发边界

开发者维护 TSX，不手写页面 C。Devtool 把 React Profile TSX 生成 C，再分别
编译为模拟器 `app.dylib` 和真机 `app.arm`。生成 C 和原生二进制是构建产物，
“纯 TSX”不代表设备运行 JavaScript。

创建项目：

```sh
create-crazypod-app neon-player --type now-playing-theme
cd neon-player
npm install
crazypod test
crazypod dev --once
crazypod build
```

manifest 必须包含 `"kind": "now-playing-theme"`；新主题还必须用
`"artworkSourceSize": 16..320` 声明布局实际需要的最大封面边长。全屏封面用
320，小封面使用真实显示尺寸；ABI 1.11 主题缺失该字段会被拒绝。普通 Mini App 不能导入
`@crazypod/now-playing`，也不能创建主题专用的封面和声波对象。

## 平台能力

`refreshNowPlaying` 返回标题、歌手、专辑和固定十项状态：

| 下标 | 内容 |
| ---: | --- |
| 0 | 播放 revision，供主题自己的派生状态使用 |
| 1 | 已播放毫秒数 |
| 2 | 总时长毫秒数 |
| 3 | 0 停止、1 暂停、2 播放 |
| 4 | Rockbox 音量值 |
| 5 | 重复模式 |
| 6 | 随机播放标记 |
| 7 | 收藏标记 |
| 8 | 左声道播放峰值，0–1000 |
| 9 | 右声道播放峰值，0–1000 |

平台提供播放/暂停、上一首、下一首、调整音量、绝对/相对跳转进度、收藏/取消
收藏、四种播放模式、读取播放队列、播放指定队列项和读取当前歌词窗口。平台不
提供固定弹窗；弹窗是否打开、页面结构、选中项、按键映射和关闭方式全部由主题
TSX 定义。

主题拥有 Select、Play、Left、Right 和滚轮。主题可以声明根节点 `onMenu`，但
固件只在场景中存在原生 `Modal` 时派发 Menu：子面板返回哪一层由 TSX 决定；
最外层正在播放页的 Menu 仍由固件直接退出。持续按住 Menu 的 Repeat 始终强制
退出，主题不能拦截，因此错误的弹窗逻辑不会把用户困住。

滚轮首次事件和后续连续滚动事件会按显示帧合并成一个事件，`event.steps` 保留
累计增量和加速度。主题必须使用 `steps` 更新选中项、音量或进度，不能假设每次
handler 都只代表一格。普通 Mini App 的既有输入路径不变。

`NowPlayingArtwork` 只需要尺寸、位置、圆角和可选 `variant`，新主题不再传
`revision`。固件立即切换标题、歌手等数据；新封面加载期间保持最后一次成功
提交的封面，完整解码和缩放后在同一个 LVGL 周期替换。冷启动或确认无封面时
显示统一静态无封面图。固件预取真实播放队列的下一首和下两首；浏览播放队列时，
当前选中项临时占用较远的预取位置，关闭后恢复下两首预取。下标 8、9
由固件从播放 mixer 计算，只暴露归一化峰值，不暴露 PCM 缓冲区；主题可以据此
用纯 TSX 绘制真实 VU 律动。`SoundWave` 是由 phase 驱动的原生装饰波形，不做
频谱分析。主题无权读取原始 PCM，也不自行解码封面。

媒体库和 Cover Flow 始终读写 128px 的 `CV10` 持久缓存。正在播放主题不读取、
不写入该缓存，而是按 `artworkSourceSize` 从嵌入或外置原图直接解码；因此全屏主题
不会再把 128px 缓存放大到 320px。

`MarqueeText` 复用固件默认正在播放页的跑马灯引擎，适合标题、歌手、专辑和
队列项。`refreshNowPlaying` 与 `refreshQueueItem` 仍返回原始文本，普通 `Text`
也保留；主题可以选择自定义裁切或动画，不被强制使用跑马灯。

ABI 1.7 起的固定播放数据布局：

- `refreshQueueState`：`[generation, count, currentIndex]`；
- `refreshQueueItem`：三段文本加 `[generation, index, isCurrent]`；
- `refreshLyricsWindow`：前/当前/后三行加 `[available, revision, currentLine]`；
- `setFavorite(boolean)`：幂等设置，不依赖界面缓存状态；
- `setPlaybackMode("normal" | "shuffle" | "repeat-all" | "repeat-one")`；
- `seekPlayback(ms)`、`seekBy(deltaMs)`、`playQueueItem(index)`。

全部读取都有固定结构和 128 字节单段文本上限，不把完整动态数组或歌词文件交给
主题。主题必须用队列 `count` 约束索引，并处理无歌曲、无歌词和总时长为 0。

完整纯 TSX 示例位于 `miniapps/themes/atelier-hifi/App.tsx`。仓库同时提交
Devtool 生成的 `generated/app.c`，固件构建只编译该产物，不维护第二份手写页面。

第二套完整示例位于 `miniapps/themes/signal-one/App.tsx`，安装后显示为
`Signal One`。它演示如何用静态 RGB565 机壳材质配合动态 TSX 封面、文本、进度、
PCM 电平和统一弹窗；功能与第一套示例相同，且不会替换系统默认主题。

## 安装与选择

将真机 CPK 复制到 `/MiniApps/Install/`，进入 Mini Apps 触发安装，再到“自定义 >
主题”选择。固件把选择保存到 `/.crazypod/now-playing-theme.cfg`。

主题打开、加载或 mount 失败时，本次进入回到默认页面。固件不自动改写主题包，
也不把失败主题冒充为成功运行。

## 安全事实

主题和 Mini App 都不要求可信签名。CPK5 校验 ZIP 结构、边界、target、ABI 和
CRC32，但不认证作者。原生 AOT 包也没有恶意代码沙箱。开放安装意味着用户必须
自行判断来源；CRC 只能发现损坏，不能证明安全。

ABI 约束普通生成代码只能使用宿主 UI、私有存储和已声明服务。这能减少误用，
不能把不可信原生机器码变成安全代码。

## 验证

```sh
sh tests/run-miniapp-host-tests.sh
./build-sim.sh --incremental
sh tests/run-miniapp-native-aot-simulator-test.sh
```

模拟器门禁覆盖主题包安装、类型隔离、原生加载、主题帧替换、恢复默认主题、真实
媒体目录扫描、主题自定义选项/队列面板、Menu 固件退出、全部主题控制、宿主重复
render、连续滚轮 Repeat 事件、逐首切换、进度增长、PCM 峰值驱动的格子变化以及
嵌入 JPEG 封面更新。
封面检查读取实际 framebuffer，不以缓存文件存在代替画面成功。真机发布仍必须
验证真实媒体库、连续切歌、暂停/继续、长时间音频和重启后的主题选择；模拟器通过
不等于真机通过。
