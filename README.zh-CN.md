<h1 align="center">
  <a href="https://ultrapod.gigassbox.com/crazypod/">▶ 查看 CrazyPod 在线演示</a>
</h1>

<p align="center"><strong>打开完整的交互式产品演示。</strong></p>

<p align="center"><a href="README.md">English</a> · <a href="README.zh-CN.md">简体中文</a></p>

> **项目来源：** CrazyPod 源自
> [Poorfocus/Rockbox-UI-UX-Overhaul](https://github.com/Poorfocus/Rockbox-UI-UX-Overhaul)，
> 该项目基于 [nuxcodes/rockpod](https://github.com/nuxcodes/rockpod)，而
> Rockpod 本身基于 [Rockbox](https://www.rockbox.org/)。所有继承代码的版权声明
> 均予以保留。完整来源与许可信息见 [NOTICE](NOTICE) 和 [LICENSE](LICENSE)。

# CrazyPod

CrazyPod V1.0 是面向 iPod Classic 6G 硬件系列的实验性独立固件。它使用
320×240 LVGL 应用轮播界面替换 Rockbox 原界面，同时保留 Rockbox 的编解码器、
播放引擎、存储、电源、USB 和设备驱动。

![CrazyPod 主屏幕](screenshots/crazypod-home.png)

> [!WARNING]
> CrazyPod 是实验性固件。V1.0 可以完整编译和打包，早期开发版本已在
> iPod Classic 6G 上安装并检查文件完整性，但 V1.0 尚未完成完整的真机回归测试。
> 安装前必须保留备份，并准备好经过验证的 Apple 磁盘模式或 DFU 恢复路径。

## 支持范围

| 项目 | 当前支持 |
| --- | --- |
| 设备 | iPod Classic 6G 目标系列（第 6、6.5 和 7 代） |
| 屏幕 | 320×240 RGB565 |
| 界面 | LVGL 9.5.0；滚轮导航 |
| 语言 | 英文、简体中文、繁体中文、日文、韩文、德文、法文、西班牙文和巴西葡萄牙文 |
| 媒体 | 仅本地文件 |
| USB | 可选仅充电或大容量存储模式 |
| 网络服务 | 无 |

CrazyPod 是固件产品，不是 Rockbox 主题。构建产物不包含 Rockbox 菜单、文件浏览器、
WPS、皮肤引擎、主题系统、插件界面、录音流程、USB Audio、HID 或 iPod 配件协议。

## 主要功能

### 音乐与媒体

- 扫描 `/Music`；启用“设置 → 播放设置 → 原系统音乐”后，同时扫描
  `/iPod_Control/Music`。新安装默认启用；从 V1.0 升级的设备保持原来的关闭状态，
  直到用户手动启用。
- 提供艺术家、专辑、歌曲、M3U/M3U8 播放列表、收藏、搜索、动态队列、随机播放、
  循环、断点续播、本地 LRC 歌词和 Cover Flow。
- 支持播客、照片浏览、图片收藏、缩放和平移。
- 支持预转码 MPEG-1/2 视频、海报图、播放控制、10 秒快进/快退和断点续播。

### 设备应用

- **备忘录：** 草稿、置顶、搜索、复制、废纸篓和恢复。
- **图书：** EPUB、TXT 和 Markdown，支持进度、书签、收藏、章节、字号和纸张主题。
- **日程：** 本地日历事件、只读 ICS 导入和 VCF 联系人。
- **Mini Apps：** React 风格 TypeScript/TSX 经 AOT 编译为 C，再编译为原生 `app.arm`；
  设备不运行 JavaScript 引擎。内置 2048 和 Capability Lab 示例。
- **运动：** 20 种计时活动，支持暂停、恢复、历史和摘要。CrazyPod 只记录时间，
  不虚构距离、步数或卡路里。
- **系统：** 电池和时钟状态、USB 模式、自动关机、睡眠定时、锁屏、电源菜单、
  减少动态效果和 16 应用主菜单排序。

### 本地化

- 设置中的语言切换立即生效，并在重启后保持。
- 固件目录包含 839 个翻译键。
- 8、10、12、14 和 16px 字体子集覆盖当前 CJK、韩文和拉丁扩展字符。

### 外观

- 16 套图标主题，支持缩放、辉光和高亮设置。
- 主屏幕与菜单壁纸、独立上下圆角设置。
- 可验证导入与导出的版本化 `.upodtheme` 外观预设。
- Music、Media、Notes 和 Books 使用对象化拟物预览转场。
- “减少动态效果”会把位移和交错动画替换为短暂淡入淡出。

## 构建与运行

工具链要求和详细构建参数见 [BUILD.md](BUILD.md)。脚本已在 macOS 上验证。

构建模拟器：

```sh
git clone https://github.com/Gigass/CrazyPod.git
cd CrazyPod
./build-sim.sh
cd build-sim
./rockboxui
```

构建 iPod 6G 固件：

```sh
./build-hw.sh
```

两个脚本默认执行干净构建。传入 `--incremental` 可复用现有构建目录。

| 产物 | 路径 |
| --- | --- |
| 模拟器 | `build-sim/rockboxui` |
| 固件 | `build-hw-ipod6g/rockbox.ipod` |
| 安装包 | `build-hw-ipod6g/CrazyPod-6G.zip` |
| V1.0 发布包 | `build-hw-ipod6g/CrazyPod-V1.0-iPod6G.zip` |
| 可选引导程序 | `build-bootloader-ipod6g/bootloader-ipod6g.ipod` |
| 2048 软件包 | `dist/miniapps/game2048-5.0.1.cpk` |

## 安装

安装会改写设备固件。请先阅读英文主文档中的
[完整 V1.0 安装步骤](README.md#install-crazypod-v10)，并严格区分以下两种设备状态：

1. 仅有 Apple 原厂固件：先安装双启动引导程序。
2. 已安装 Rockbox 或 CrazyPod：保留现有引导程序，只替换固件资源。

不要只复制 `rockbox.ipod`。发布包还包含必需的编解码器、字体、图标和原生 Mini App。
安装前请确认设备型号、磁盘格式、备份和恢复路径。

## 设备内容

- 音乐：`/Music`；启用“原系统音乐”后读取 `/iPod_Control/Music`
- 播客：`/Podcasts`
- 图片：`/Pictures`
- 视频：`/Videos`
- 图书：`/Books`
- Mini Apps：`/MiniApps`

## 控制方式

- 滚轮：移动焦点或调整数值。
- 中键：确认或打开。
- Menu：返回；长按可执行页面定义的快捷操作。
- Play/Pause：播放控制；长按约三秒打开电源菜单。
- Previous/Next：曲目切换或页面定义操作。

## 已知限制

- V1.0 尚未完成完整真机回归测试。
- 仅支持本地媒体，不提供网络服务。
- 视频需要预转码为设备可解码的 MPEG-1/2。
- 运动应用只记录计时，不提供传感器数据。
- 锁屏停止界面与后台媒体工作，但不等同于整机挂起。

完整限制、安装细节和验证证据以英文主文档、[PROJECT_STATUS.md](PROJECT_STATUS.md)
和 [NOTICE](NOTICE) 为准。

## 许可

仓库包含 GPL 许可的 Rockbox 及其衍生代码。详情见 [LICENSE](LICENSE) 和
[NOTICE](NOTICE)。重新分发时必须保留上游版权和许可声明。
