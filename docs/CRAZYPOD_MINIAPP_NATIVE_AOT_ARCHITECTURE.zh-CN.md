# CrazyPod Mini App Native AOT 架构

## 1. 最终结论

当前 Mini App 唯一生产路径是：

```text
React 风格 TypeScript/TSX
        ↓ 构建机 TypeScript AST 编译
受限 React Profile v1
        ↓ AOT
确定性 C
        ↓ 同一份 C 分别交叉编译
app.arm（iPod 6G） / app.dylib（模拟器）
        ↓
CPK5
        ↓ 固件严格校验、安装、原生加载
Native ABI 1.9
        ↓ 句柄和纯数据调用
宿主持有的 LVGL 9.5
```

设备端不存在 JavaScript 引擎、React、Solid、Virtual DOM、JS 字节码、
JS 源码解析或 UI 命令批处理解释器。`app.arm` 直接执行生成的 ARM
机器码，应用只能通过版本化 Native ABI 请求宿主创建和更新 LVGL 对象。

## 2. 版本锁定

| 合约 | 当前值 |
| --- | --- |
| 包格式 | CPK5 |
| Native ABI | major 1, minor 5 |
| React Profile | 1 |
| 硬件目标 | iPod Classic 6G，target id 71 |
| UI | LVGL 9.5，宿主持有 |
| 模拟器载荷 | `app.dylib` |
| 真机载荷 | `app.arm` |

Major 不匹配直接拒绝加载。Minor 和三个结构体大小用于向后兼容扩展；
应用不得假设宿主结构体尾部一定存在。

## 3. 二进制边界

唯一共享头文件是
`miniapps/sdk/crazypod_miniapp_native.h`。合约只包含固定宽度整数、
结构体大小、函数指针、句柄和调用结果，不暴露：

- `lv_obj_t *`；
- Rockbox API；
- 固件全局变量；
- C++ ABI；
- 构建器内部状态。

应用入口返回 `cp_native_app_ops`，生命周期为：

1. 宿主校验 CPK5、manifest、profile 和二进制头；
2. 加载 `app.arm` 或 `app.dylib`；
3. 调用 `cp_native_miniapp_entry(host)`；
4. 调用 `mount()`；
5. 同步投递 `input()`、`ui_event()` 和 `tick()`；
6. 退出时调用 `unmount()`；
7. 删除所有宿主 UI、释放资源并卸载原生载荷。

任何一次只允许一个 Mini App 活跃。应用切换前必须完成上一应用的
`unmount`、场景树销毁和加载器关闭。

## 4. UI 模型

Native UI vtable 提供：

- `begin_update` / `end_update`；
- `create` / `insert` / `remove`；
- `set_i32` / `set_color` / `set_string` / `set_bytes`；
- `listen`；
- `animate`；
- `commit_drawing`。

句柄由索引和 generation 组成。宿主拒绝失效 generation，因此删除后的
旧句柄不能误操作新对象。字符串和字节数据在 ABI 调用返回前由宿主复制。

AOT 产物保留节点句柄。只有 JSX 条件导致结构签名变化时才重建子树；
普通状态变化只更新受影响的动态属性。这个约束是 2048 连续输入不再迟缓
的关键，禁止退回“每次状态变化删除并重建整棵 LVGL 树”。

动态更新有两层去重：编译器不把 `styles.title` 之类样式属性名误判为同名
state，只生成值确实可变的属性，并在处理器没有改变任何状态时跳过渲染；
固件场景层拒绝同值属性写入，只在插入/删除或可见性、禁用、可聚焦、焦点监听
发生变化后重新计算焦点。播放主题的定时刷新不能反复提交静态面板布局。

Mini App 包目录在启动 Logo 已显示后统一预热。对于版本号、入口尺寸和 CRC
记录均未变化的包，固件只校验已安装文件一次，不再完整读取 CPK 后又把 CPK
与安装目录逐字节比较。新安装、升级、记录不匹配或安装文件损坏时仍执行完整
CPK 校验和原子发布。具体 App 的机器码仍在打开时载入共享 plugin buffer。

首页长按 Menu 打开正在播放后，该次按键手势一直由首页持有到 release；加载
期间积压的 repeat 不会被新页面误认为强制退出。释放后开始的新 Menu 手势仍
保留主题面板返回和固件安全退出语义。

## 5. 布局和组件

React Native 风格属性在构建期映射到 LVGL：

| TSX | Native UI |
| --- | --- |
| `View` | Screen/View |
| `Text` | Label |
| `Text numberOfLines` | 固定 1–8 行并在溢出时显示省略号 |
| `Pressable` / `Button` | Button |
| `ScrollView` | Scroll，当前只支持状态驱动的 `scrollX` / `scrollY` |
| `Image` | Image |
| `AnimatedImage` | Animated Image，使用构建期转换的 GIF/Lottie 精灵表 |
| `ProgressBar` | Bar |
| `ActivityIndicator` | 由 `value` 驱动的确定性 Arc，不是无限旋转器 |
| `Slider` | Slider |
| `Switch` | Switch |
| `CheckBox` | Checkbox |
| `Modal` | Host-owned Modal subtree |
| `FlatList` | 构建期固定容量 Scroll subtree |
| `NowPlayingArtwork` | 主题专用的宿主动态封面 |
| `SoundWave` | 主题专用的宿主原生声波 |

`StyleSheet.create` 支持编译期常量样式。Flexbox 包含 row、column、wrap、
align、justify、grow、padding、margin、尺寸、颜色、边框、圆角和透明度；
`position: "absolute"` 会脱离父 Flex 布局，overflow 会映射到宿主裁剪。
它是面向 320×240 LVGL 的 React Native 风格布局子集，不是浏览器 CSS。
`Modal` 是随条件 JSX 创建和释放的宿主子树，Profile v1 不承诺焦点陷阱。
无限旋转视觉应使用构建期动画资源和 `AnimatedImage`。

`FlatList` 只接受固定数组或有界列表，最大行数在构建期确定；它不是通用动态
列表 diff。`TextInput` 和 `Picker` 仍会被编译器拒绝。Native ABI 保留的底层
Textarea/Dropdown 对象不是开发者 API，不能据此宣称支持。

## 6. 状态和事件

React Profile v1 状态落为生成 C 中的固定内存。支持整数、布尔、字符串枚举、
127 字节有界字符串、固定对象、固定数组、固定对象数组和有界列表；容量和字段
布局都必须在编译期确定。setter 保留 React 事件批处理语义：直接 setter 读取
本次 render 的闭包快照；同一处理器中的函数式更新按顺序读取前次结果；处理器
结束后统一刷新一次。

支持 UI 事件：

- press/select；
- long press；
- focus/blur；
- value change/change；
- scroll。

支持物理输入：

- Click Wheel 顺/逆时针；
- Left / Right；
- Select；
- Play；
- Menu。

宿主维护焦点顺序。滚轮移动焦点；Select 触发选择，Switch/CheckBox
同步切换值，Slider 用 Left/Right 调整。

## 7. Host 能力

ABI 1.9 当前承诺（播放主题能力始于 ABI 1.4，文本裁切始于 ABI 1.5，
双声道峰值始于 ABI 1.6）：

- 原子状态读写；
- 包资源 stat/read；
- 请求关闭；
- 日志；
- 私有沙箱文件；
- 有界 Host service dispatcher；
- 仅供正在播放主题使用的播放快照、归一化双声道峰值、控制、动态封面和原生声波；
- 平台统一的当前/下一首/下两首封面调度、已提交封面和主题节点自动原子更新；
- epoch 和 monotonic 时间；
- Native UI。

主题播放控制不是普通 Mini App 权限。文件选择、原始 PCM、音效、设备设置和闹钟
接口尚未进入当前 ABI。以后增加时只能提升 minor 或增加有界服务并同步固件、
SDK、编译器和模拟器，不能复活设备端脚本运行时。

## 8. 2048 的说明

当前 2048 使用通用的有界 action、固定数组、固定循环和持久化能力。编译器不再
包含游戏专用包名、状态名或 C 运行时。这证明固定内存业务逻辑可以直接 AOT，
仍不代表任意 TypeScript 都能翻译为 C。递归、无界循环、动态堆结构和通用
JavaScript 语义继续被拒绝。

## 9. 已删除路径

以下内容不再属于产品：

- QuickJS 和兼容层；
- Solid Universal Renderer；
- `app.js` / `app.qbc` / `styles.bin`；
- CPK4；
- ABI3 SDK；
- 序列化 UI command batch；
- install-time JS 编译和缓存重建。

CPK4/ABI3 包不会被当前固件加载。没有双运行时或自动回退。
