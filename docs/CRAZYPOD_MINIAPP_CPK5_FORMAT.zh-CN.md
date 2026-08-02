# CrazyPod CPK5 格式

CPK5 是确定性 ZIP STORE 包。模拟器和 iPod 包必须恰好包含五个条目：

```text
manifest.json
app.dylib 或 app.arm
profile.bin
assets.bin
icon.bin
```

不允许目录项、重复项、压缩、数据描述符、额外字段、未知文件、绝对路径、
`..` 或反斜杠。Central directory、local header、长度和 CRC32 必须一致。

## manifest.json

必要字段：

```json
{
  "format": 5,
  "kind": "miniapp",
  "id": "example",
  "name": "Example",
  "version": "1.0.0",
  "versionCode": 10000,
  "runtime": "native-aot",
  "abiMajor": 1,
  "abiMinor": 5,
  "reactProfile": 1,
  "target": "ipod6g",
  "entry": "app.arm",
  "icon": "icon.bin",
  "symbol": "EX",
  "summary": "Native AOT example",
  "accent": "#ff9f43"
}
```

模拟器目标必须是 `simulator` 且 entry 必须是 `app.dylib`；真机目标必须是
`ipod6g` 且 entry 必须是 `app.arm`。二者不可混装。

`kind` 可为 `miniapp` 或 `now-playing-theme`。缺少该字段的旧包按 Mini App
处理；正在播放主题必须显式声明该字段并至少要求 ABI 1.4。使用
`Text numberOfLines` 的主题要求 ABI 1.5；使用双声道播放峰值的主题要求 ABI 1.6；
使用队列、歌词、明确收藏/模式写入、相对跳转或 `MarqueeText` 的主题要求 ABI 1.7。

## profile.bin

16 字节小端 profile 固定描述 CPK5、Native ABI 1.9 和 React Profile 1。
固件在加载原生二进制前校验该文件。

## 原生载荷

`app.arm` 带 `cp_native_binary_header`，其中包含：

- CPK5 Native magic；
- Rockbox target id；
- ABI major；
- 加载区和 BSS 信息；
- Native 入口；
- Host/UI/App Ops 结构体大小；
- ABI minor 和 React Profile。

模拟器 `app.dylib` 导出同一头和同一入口。除装载格式外，应用逻辑来自同一
生成 C。

## 限制

- manifest 最大 8 KiB；
- 单个原生二进制最大 8 MiB；
- assets 容器最大 8 MiB；
- 包安装使用 staging、完整校验和原子发布；
- CPK4 及更早格式直接拒绝，不做运行时迁移或回退。

## 签名与安全

CPK5 没有可信签名。CRC32 用于发现传输和存储损坏，不能认证作者。包内载荷是
原生机器码，当前运行时没有恶意代码沙箱。安装器的格式、路径、target 和 ABI
校验是可靠性边界，不是“不可信代码可安全执行”的证明。
