# CrazyPod Mini App Native AOT 开发指南

## 1. 环境

在仓库根目录安装构建器依赖：

```sh
npm ci --prefix tools/miniapp-builder
```

创建项目：

```sh
node tools/miniapp-builder/src/create-app.mjs miniapps/example
```

生成的项目包含：

```text
App.tsx
crazypod.config.json
assets/
```

## 2. 开发代码

代码使用真实的 React/React Native import 形式：

```tsx
import React, { useState } from "react";
import {
  Pressable,
  SafeAreaView,
  StyleSheet,
  Text,
  View,
} from "react-native";

export default function App() {
  const [count, setCount] = useState(0);

  return (
    <View style={styles.screen}>
      <SafeAreaView style={styles.content}>
        <Text style={styles.value}>{count}</Text>
        <Pressable
          style={styles.button}
          onPress={() => setCount((previous) => previous + 1)}
        >
          <Text>INCREMENT</Text>
        </Pressable>
      </SafeAreaView>
    </View>
  );
}

const styles = StyleSheet.create({
  screen: {
    width: 320,
    height: 240,
    backgroundColor: "#15181e",
  },
  content: {
    width: 320,
    height: 208,
    alignItems: "center",
  },
  value: {
    width: 100,
    height: 32,
    color: "#f5f5f7",
    fontSize: 24,
    fontFamily: "system",
    textAlign: "center",
  },
  button: {
    width: 180,
    height: 36,
    marginTop: 12,
    backgroundColor: "#ff9f43",
    borderRadius: 8,
  },
});
```

`TextStyle.fontFamily` 只提供 `system`、`serif`、`mono` 三个内置
语义字体，未指定时使用 `system`。`fontSize` 是 6–48 的任意整数，
`fontWeight` 支持 `normal`、`bold` 以及 100–900，还可设置 `lineHeight`
和整数 `letterSpacing`。`fontSize` 必须是静态值，Devtool 会在安装前按
实际 `family/weight/size` 组合生成精确的地区化 RB12 字体，不对外公开
字号档位，也不让 iPod 在运行时栅格化轮廓。字号动画使用 `scaleX/scaleY`。

系统根据语言自动选择 Noto CJK 的 SC、TC、JP 或 KR 字形，主题和
MiniApp 不得按语言分支选字体。同一文本的回退字形继承相同字号、
目标字重和行框。Noto CJK 只有 7 个实体字重，其余请求映射到最近字重。
`fontStyle: "italic"` 因 Noto CJK 没有真斜体而直接构建报错。

CPK 清单会自动记录使用到的字体组合。`crazypod dev` 和
`crazypod install` 先生成并安装缺失字体，再放入 CPK；固件发现清单声明
的字体不完整时会拒绝安装，不会静默换成别的字号或字重。

| `fontFamily` | 实际字号 | 风格与建议用途 |
| --- | ---: | --- |
| `system` | 声明的 `fontSize` | Noto Sans UI 与媒体元数据 |
| `serif` | 声明的 `fontSize` | Noto Serif 编辑感标题 |
| `mono` | 声明的 `fontSize` | Noto Sans Mono 数据与技术文本 |

```tsx
const styles = StyleSheet.create({
  title: { fontFamily: "serif", fontSize: 20, fontWeight: "700" },
  metadata: { fontFamily: "system", fontSize: 16, lineHeight: 20 },
  meter: { fontFamily: "mono", fontSize: 16, letterSpacing: 1 },
});
```

### 包内外挂字体

主题和普通 MiniApp 都可在自己的 CPK 中携带字体。源码必须是标准
`TTF`、`OTF`、`TTC` 或 `BDF`，不能直接放 Rockbox `.fnt`。在项目的
`assets/assets.json` 声明资源：

```json
[
  {
    "id": "album-title",
    "type": "font",
    "file": "AlbumTitle.otf",
    "size": 20
  },
  {
    "id": "pixel-label",
    "type": "font",
    "file": "PixelLabel.bdf"
  }
]
```

`TTF`/`OTF` 必须设置 6 到 48 的整数像素 `size`。`TTC` 还可设置从 0
开始的 `face`；`BDF` 使用文件自带的像素尺寸，不能设置 `size` 或 `face`。
Devtool 在构建时调用 Rockbox `convttf`/`convbdf`，将源字体转换为 BMP 范围的
RB12 资源；iPod 不解析轮廓字体。

TSX 用资源 ID 引用：

```tsx
const styles = StyleSheet.create({
  albumTitle: { fontFamily: "asset:album-title" },
  label: { fontFamily: "asset:pixel-label" },
});
```

同一个 CPK 会话最多加载 4 个包字体，每个转换产物最大 4 MiB；CPK 全部资源
仍受 8 MiB 总上限约束。包字体实际包含的字形由源文件决定，缺失字形逐字回退
到同高度的 Noto `system` 字体。RTL 与阿拉伯文塑形不在契约内。

替换字体时保留 `id` 和 `fontFamily: "asset:<id>"`，替换源文件并提高包的
`version`/`versionCode` 后重新构建、安装。设备按资源 CRC 生成私有字体文件，
不会误用同 ID 的旧版本，也不会与其他主题或 MiniApp 的字体冲突。

构建机需要 `make`、C 编译器、`pkg-config` 和 FreeType 开发库来构建
`convttf`。仓库外单独安装 Devtool 时，可用 `CRAZYPOD_CONVTTF` 与
`CRAZYPOD_CONVBDF` 指向兼容的 Rockbox 转换器。

这些 import 只用于类型检查和源码语义识别，不会打进设备。设备端没有
React 包。

布局契约固定为 320×240。顶部 32 像素由宿主状态栏占用；普通页面必须放在
`SafeAreaView` 中，它由 AOT 固定为 `(0, 32, 320, 208)`。根 `View` 和
`Modal` 使用完整 320×240；`Modal` 会被提升到根级浮层，不受页面 padding
或 Flex 布局偏移。所有容器默认使用 React Native 的纵向 Flex，只有横向排列
时才需要写 `flexDirection: "row"`。

## 3. React Profile v1 的准确边界

当前支持：

- 一个默认导出的 `App`，以及构建期内联的有界自定义函数组件；
- 整数、布尔、字符串枚举、有界字符串、固定对象、固定数组和固定对象数组状态；
- `useFixedArray`、`useBoundedList`、固定 `.map()` 和有界 `FlatList`；
- 固定对象 reducer、标量 Context、`useEffect`、mount/unmount hook 和 interval；
- 有界本地 action、固定循环、持久化状态和应用私有文件；
- `delay` 降低得到的固定 async 状态机和有界 Promise continuation；
- 源码型 React Native npm 包；
- 条件 JSX、运行期整数布局值和两套完整静态样式之间的切换；
- 架构文档列出的控件、事件、物理输入和构建期转换资源；
- 通用有界 Host service dispatcher。

当前不支持：

- 任意堆对象、原型语义、无界数组和通用动态列表 diff；
- 可编辑 `TextInput`、`Picker`、`useMemo` 和完整 React reconciliation；
- 通用 Promise 构造/拒绝、任意 async I/O 和网络；
- 带 JavaScript 运行时语义或原生二进制的普通 npm 组件；
- 任意运行期样式对象、完整 CSS、DOM、Node API 或 Metro；
- 任意 TypeScript 自动翻译为 C。

违反子集时编译器必须报具体源码位置并停止，不能把代码留到设备端解释。

## 4. 配置

```json
{
  "runtime": "native-aot",
  "source": "App.tsx",
  "assets": "assets",
  "iconColor": "#ff9f43",
  "manifest": {
    "id": "example",
    "name": "Example",
    "version": "1.0.0",
    "versionCode": 10000,
    "symbol": "EX",
    "summary": "Native AOT example",
    "accent": "#ff9f43"
  }
}
```

`id` 必须匹配 `[a-z][a-z0-9_-]{0,31}`。`versionCode` 只能递增。

正在播放主题把 `manifest.kind` 设为 `now-playing-theme`。创建、API 和安装流程
见[正在播放主题](CRAZYPOD_NOW_PLAYING_THEMES.zh-CN.md)。普通 Mini App 的
`kind` 为 `miniapp`，也可省略该字段以兼容旧包。

## 5. 命令

严格类型检查并验证 AOT：

```sh
node tools/miniapp-builder/src/cli.mjs test miniapps/example
```

只生成 C：

```sh
node tools/miniapp-builder/src/cli.mjs generate \
  miniapps/example --out /tmp/example.c
```

构建模拟器包：

```sh
node tools/miniapp-builder/src/cli.mjs build \
  miniapps/example --target simulator
```

开发模式会重建 `app.dylib`、重新打 CPK5 并重启模拟器：

```sh
node tools/miniapp-builder/src/cli.mjs dev miniapps/example
```

真机 `app.arm` 由 Rockbox 交叉编译环境构建。标准入口是：

```sh
./build-hw.sh --incremental
```

不要用本机 `cc` 伪造真机二进制。

## 6. 资源

`assets/` 在构建机转换为 `assets.bin`。支持 PNG RGB565、动画精灵表、
Lottie 离屏栅格化、tileset、tone 和原始 blob。设备端只读取已转换的
确定格式，不解码 PNG/GIF/Lottie。

`Image source="logo"` 使用资源 id，不是文件路径。

## 7. 性能规则

- 状态更新不得默认重建整棵树；
- 静态结构必须复用句柄，只更新动态属性；
- JSX 条件改变结构签名时才允许重建；
- 连续输入必须进入 E2E 延迟测试；
- 资源转换只在构建机发生；
- 不在事件处理器中做无界循环或大量同步存储。

当前 2048 回归门禁是 5 轮完整流程、每轮 32 次移动，然后退出并打开
Capability Lab；交互 p95 必须不超过 180ms，最大不超过 300ms。

## 8. 调试原则

模拟器和真机使用同一份生成 C。模拟器专用代码只能存在于加载器和平台层，
不能维护另一套应用逻辑。发现真机问题时先在模拟器构造确定性输入、状态和
帧输出，再修改实现。
