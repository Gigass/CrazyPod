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

这些 import 只用于类型检查和源码语义识别，不会打进设备。设备端没有
React 包。

布局契约固定为 320×240。顶部 32 像素由宿主状态栏占用；普通页面必须放在
`SafeAreaView` 中，它由 AOT 固定为 `(0, 32, 320, 208)`。根 `View` 和
`Modal` 使用完整 320×240；`Modal` 会被提升到根级浮层，不受页面 padding
或 Flex 布局偏移。所有容器默认使用 React Native 的纵向 Flex，只有横向排列
时才需要写 `flexDirection: "row"`。

## 3. React Profile v1 的准确边界

当前支持：

- 一个默认导出的 `App` 函数，且 JSX 根必须是 `View`；
- 顶层整数 `useState(initial)`；
- 函数式或直接 setter；
- 整数算术、比较、逻辑、位运算和三元表达式；
- `&&` 和三元 JSX 条件；
- 字符串、整数和条件文本；
- `StyleSheet.create` 的静态对象；
- 默认纵向 Flex、`SafeAreaView`、根级 `Modal`、`flexWrap`、absolute
  position、overflow 和构建期动画资源；
- `AnimatedImage` 播放构建期转换的 GIF/Lottie 精灵表；
- 本架构文档列出的组件、事件和物理输入；
- 同一事件中的多个 setter。

当前不支持：

- 任意 npm React 组件；
- 自定义函数组件和 props；
- 数组 `.map()`、动态列表 diff；
- `FlatList`、可编辑 `TextInput` 和 `Picker`；
- 对象/数组状态；
- `useEffect`、`useMemo`、`useReducer`、Context；
- Promise、async/await、网络；
- 运行期样式对象或完整 CSS；
- 任意 TypeScript 函数自动翻译为 C；
- 浏览器 DOM 或 React Native bridge。

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
