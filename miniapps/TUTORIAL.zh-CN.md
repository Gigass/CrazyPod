# 从零开发 CrazyPod Mini App

本教程面向熟悉 C 语言、但第一次接触 CrazyPod Mini App SDK 的开发者。
完成后，你会得到一个可在模拟器运行、可签名为 `.cpk`、并可安装到
iPod Classic 6G 的 `hello-wheel` 计数器。

本文已按 2026-07-30 的 ABI 1、SDK revision 3 和 CPK format 2 源码核对。

最终产物：

```text
dist/miniapps/hello-wheel-1.0.0.cpk
```

## 1. 先确认当前边界

CrazyPod Mini App ABI 1 不是 Web、小程序脚本或 Rockbox 插件格式。

- 载荷是原生代码：真机使用 `app.arm`，模拟器使用 `app.dylib`。
- 当前硬件目标只有 `ipod6g`，target id 为 71。
- 应用通过语义化绘图命令输出界面，不能直接创建 LVGL 对象。
- 应用只能使用宿主明确提供的时间、状态、闹钟、格式化和受限 UI 接口；
  revision 3 的可选能力必须先检查结构大小和 capability。
- 真机载荷是 freestanding 程序，不能假定存在完整 libc、文件系统、
  音频或线程 API。
- Ed25519 签名验证包的来源和完整性，但原生载荷没有沙箱，运行时拥有
  固件权限。

当前构建工具仍是“仓库内集成”模式，不是独立的第三方 SDK。新增应用
需要在 `miniapps/miniapps.make` 和
`tools/build-miniapp-packages.py` 中登记。只把一个新目录放进
`miniapps/` 不会自动参与构建。

## 2. 准备环境

先完成 [BUILD.md](../BUILD.md) 中的模拟器和硬件依赖安装。Mini App
额外依赖：

- Python 3；
- OpenSSL 3；
- 真机构建所需的 `arm-none-eabi-gcc`、`objcopy` 和 `nm`。

确认 OpenSSL：

```sh
openssl version
```

输出必须以 `OpenSSL 3.` 开头。若系统默认版本不符合要求：

```sh
export CRAZYPOD_OPENSSL=/path/to/openssl
```

在仓库根目录执行后续命令。

## 3. 创建应用目录

创建两个文件：

```text
miniapps/hello-wheel/
├── app.c
└── manifest.ini.in
```

应用 id 使用 `hello-wheel`。合法 id 必须：

- 长度为 1–32 个字符；
- 第一个字符为小写字母；
- 后续字符只能是小写字母、数字、下划线或连字符。

## 4. 实现应用

将下面的完整代码保存为 `miniapps/hello-wheel/app.c`：

```c
#include "../sdk/crazypod_miniapp.h"

#define HELLO_STATE_MAGIC 0x48574c31u
#define HELLO_STATE_VERSION 1u
#define HELLO_ACTION_COUNT 3

struct hello_state {
    uint32_t magic;
    uint16_t version;
    uint16_t struct_size;
    int32_t value;
};

static const struct cp_host_api *hello_host;
static struct hello_state hello;
static int hello_focus;

static struct cp_draw_command *add_rect(
    struct cp_scene *scene, int x, int y, int width, int height,
    enum cp_color_token background, bool focused)
{
    struct cp_draw_command *command =
        cp_scene_add(scene, CP_DRAW_RECT);

    if(command == NULL)
        return NULL;
    command->x = (int16_t)x;
    command->y = (int16_t)y;
    command->width = (int16_t)width;
    command->height = (int16_t)height;
    command->background = (uint8_t)background;
    command->radius = 10;
    if(focused) {
        command->flags |= CP_DRAW_FOCUSED;
        command->border = CP_COLOR_ACCENT;
        command->border_opacity = 255;
        command->border_width = 2;
    }
    return command;
}

static void add_text(
    struct cp_scene *scene, int x, int y, int width, int height,
    enum cp_font_token font, enum cp_text_align align,
    enum cp_color_token foreground, const char *text)
{
    struct cp_draw_command *command =
        cp_scene_add(scene, CP_DRAW_TEXT);

    if(command == NULL)
        return;
    command->x = (int16_t)x;
    command->y = (int16_t)y;
    command->width = (int16_t)width;
    command->height = (int16_t)height;
    command->font = (uint8_t)font;
    command->align = (uint8_t)align;
    command->foreground = (uint8_t)foreground;
    cp_text_copy(command->text, sizeof(command->text), text);
}

static void save_state(void)
{
    hello.magic = HELLO_STATE_MAGIC;
    hello.version = HELLO_STATE_VERSION;
    hello.struct_size = sizeof(hello);
    (void)hello_host->state_write(&hello, sizeof(hello));
}

static void load_state(void)
{
    struct hello_state saved;
    int loaded = hello_host->state_read(&saved, sizeof(saved));

    hello.magic = HELLO_STATE_MAGIC;
    hello.version = HELLO_STATE_VERSION;
    hello.struct_size = sizeof(hello);
    hello.value = 0;
    if(loaded == (int)sizeof(saved) &&
       saved.magic == HELLO_STATE_MAGIC &&
       saved.version == HELLO_STATE_VERSION &&
       saved.struct_size == sizeof(saved))
        hello = saved;
}

static void move_focus(int direction)
{
    int next = hello_focus + direction;

    next %= HELLO_ACTION_COUNT;
    if(next < 0)
        next += HELLO_ACTION_COUNT;
    hello_focus = next;
}

static void activate_focus(void)
{
    if(hello_focus == 0 && hello.value > INT32_MIN)
        --hello.value;
    else if(hello_focus == 1)
        hello.value = 0;
    else if(hello_focus == 2 && hello.value < INT32_MAX)
        ++hello.value;
    save_state();
}

static void hello_open(void)
{
    load_state();
    hello_focus = 0;
}

static void hello_close(void)
{
}

static bool hello_event(const struct cp_input_event *event)
{
    if(event == NULL || event->struct_size < sizeof(*event))
        return false;

    switch((enum cp_input_type)event->type) {
    case CP_INPUT_WHEEL_CLOCKWISE:
    case CP_INPUT_RIGHT:
        move_focus(1);
        return true;
    case CP_INPUT_WHEEL_COUNTERCLOCKWISE:
    case CP_INPUT_LEFT:
        move_focus(-1);
        return true;
    case CP_INPUT_SELECT:
        if(!event->repeated)
            activate_focus();
        return true;
    case CP_INPUT_PLAY:
        if(!event->repeated) {
            hello.value = 0;
            save_state();
        }
        return true;
    case CP_INPUT_MENU:
        hello.value++;
        save_state();
        return true;
    default:
        return false;
    }
}

static bool hello_tick(uint32_t epoch_seconds,
                       uint32_t monotonic_ms)
{
    (void)epoch_seconds;
    (void)monotonic_ms;
    return false;
}

static void render_action(struct cp_scene *scene, int index,
                          int x, const char *label)
{
    bool focused = index == hello_focus;

    add_rect(scene, x, 160, 84, 48,
             focused ? CP_COLOR_ACCENT :
                       CP_COLOR_SURFACE_RAISED,
             focused);
    add_text(scene, x, 160, 84, 48, CP_FONT_LABEL,
             CP_ALIGN_CENTER,
             focused ? CP_COLOR_ACCENT_FOREGROUND :
                       CP_COLOR_WHITE,
             label);
}

static void hello_render(struct cp_scene *scene)
{
    char value[CP_MINIAPP_TEXT_SIZE];

    if(scene == NULL)
        return;
    cp_scene_reset(scene);
    scene->background = CP_COLOR_BACKGROUND;

    add_rect(scene, 12, 40, 296, 188, CP_COLOR_SURFACE, false);
    add_text(scene, 24, 55, 272, 24, CP_FONT_TITLE,
             CP_ALIGN_CENTER, CP_COLOR_WHITE, "Hello Wheel");

    hello_host->format_number((double)hello.value,
                              value, sizeof(value));
    value[sizeof(value) - 1] = '\0';
    add_text(scene, 24, 88, 272, 56, CP_FONT_DISPLAY,
             CP_ALIGN_CENTER, CP_COLOR_CYAN, value);

    render_action(scene, 0, 18, "-1");
    render_action(scene, 1, 110, "Reset");
    render_action(scene, 2, 202, "+1");
}

static const struct cp_miniapp_ops hello_ops = {
    CP_MINIAPP_ABI_VERSION,
    sizeof(struct cp_miniapp_ops),
    "hello-wheel",
    "Hello Wheel",
    "1.0.0",
    hello_open,
    hello_close,
    hello_event,
    hello_tick,
    hello_render
};

const struct cp_miniapp_ops *
cp_miniapp_entry(const struct cp_host_api *host)
{
    if(host == NULL ||
       host->abi_version != CP_MINIAPP_ABI_VERSION ||
       host->struct_size < CP_HOST_API_V1_SIZE ||
       host->state_read == NULL ||
       host->state_write == NULL ||
       host->format_number == NULL)
        return NULL;

    hello_host = host;
    return &hello_ops;
}

#ifdef CRAZYPOD_MINIAPP_PACKAGE
CP_MINIAPP_HEADER;
#endif
```

这个示例展示了 ABI 1 的完整最小闭环：

- `cp_miniapp_entry()` 校验宿主 ABI 并返回操作表；
- 入口只要求 ABI 1 稳定前缀；revision 3 尾部能力通过 `CP_HOST_HAS()`
  单独检查；
- `open()` 恢复状态，`close()` 结束前台会话；
- `event()` 处理转盘和按键；
- `tick()` 处理时间变化，本示例不需要定时刷新；
- `render()` 每次根据当前状态重建语义场景；
- `state_read()` 和 `state_write()` 保存每个应用独立的原子状态。

## 5. 正确处理转盘

离散焦点必须只看方向，不能把 `event->steps` 当成焦点跨度：

```c
case CP_INPUT_WHEEL_CLOCKWISE:
    move_focus(1);
    return true;
case CP_INPUT_WHEEL_COUNTERCLOCKWISE:
    move_focus(-1);
    return true;
```

CrazyPod 宿主会将滚轮事件放入有界队列，并在一个可呈现帧内最多消费一个
离散事件。反向滚动会清除旧方向的未显示输入。

只有连续量或数值编辑模式可以使用加速值：

```c
unsigned amount = event->steps == 0 ? 1u : event->steps;
value += amount;
```

不要在普通菜单、按钮网格或焦点列表中这样写，否则快速滚轮会跳过可操作
控件。

按键返回值也有语义：

- 已处理事件返回 `true`；
- 0.5 秒内释放的 `Menu` 会作为 `CP_INPUT_MENU` 交给应用，可用于
  应用内操作；
- 持续按住 `Menu` 满 0.5 秒会恒定打开宿主退出确认框，应用无法
  拦截；
- 对 `Select` 和 `Play` 应忽略 `event->repeated != 0` 的重复触发。

## 6. 定义 manifest

将下面内容保存为 `miniapps/hello-wheel/manifest.ini.in`：

```ini
format=2
id=hello-wheel
name=Hello Wheel
version=1.0.0
version_code=100000
abi=1
target=@TARGET@
binary=@BINARY@
icon=icon.bmp
resources=resources.bin
symbol=#
accent=26CFF5
summary=Wheel counter tutorial app
```

注意：

- `id` 必须与 `cp_miniapp_ops.id` 完全相同；
- `version` 必须与操作表中的版本文本一致；
- `version_code` 必须是非零整数，升级版本必须递增；
- `target` 和 `binary` 由打包器替换；
- `accent` 是六位大写或小写十六进制 RGB；
- 打包器会追加 `binary_sha256`、`icon_sha256` 和
  `resources_sha256`。

## 7. 注册编译目标

编辑 `miniapps/miniapps.make`。

先把应用加入名称列表并声明对象：

```make
MINIAPP_NAMES := calculator pomodoro hello-wheel

MINIAPP_HELLO_WHEEL_OBJ := \
	$(MINIAPP_BUILD)/hello-wheel/app.o
```

在 `ifdef APP_TYPE` 的模拟器载荷列表中追加：

```make
$(MINIAPP_BUILD)/hello-wheel/app.dylib
```

在真机分支中，让载荷包含 freestanding 内存运行时：

```make
MINIAPP_HELLO_WHEEL_OBJ += $(MINIAPP_NATIVE_RUNTIME_OBJ)
```

并在真机载荷列表中追加：

```make
$(MINIAPP_BUILD)/hello-wheel/app.arm
```

在 `OTHER_SRC` 中追加：

```make
$(MINIAPP_ROOT)/hello-wheel/app.c
```

最后分别注册真机和模拟器链接规则：

```make
$(eval $(call build_native_miniapp,hello-wheel,HELLO_WHEEL))
```

```make
$(eval $(call build_sim_miniapp,hello-wheel,HELLO_WHEEL))
```

两个 `eval` 必须放在各自现有 Calculator/Pomodoro 规则旁边，不能放错
条件分支。

## 8. 注册打包目标和图标

当前打包器只登记 Calculator 和 Pomodoro。编辑
`tools/build-miniapp-packages.py`：

```python
APP_IDS = ("calculator", "pomodoro", "hello-wheel")
```

在图标绘制函数旁增加：

```python
def draw_hello_wheel_icon(
    pixels: bytearray, width: int, height: int
) -> None:
    panel = (24, 24, 32)
    white = (244, 244, 248)
    cyan = (38, 207, 245)
    fill_rounded_rect(
        pixels, width, height, 25, 28, 110, 104, 20, panel
    )
    fill_rect(pixels, width, height, 42, 76, 28, 8, white)
    fill_rect(pixels, width, height, 90, 76, 28, 8, cyan)
    fill_rect(pixels, width, height, 100, 66, 8, 28, cyan)
```

把 `make_icon()` 中的应用分支改成显式分支：

```python
if app_id == "calculator":
    draw_calculator_icon(pixels, width, height)
elif app_id == "pomodoro":
    draw_pomodoro_icon(pixels, width, height)
elif app_id == "hello-wheel":
    draw_hello_wheel_icon(pixels, width, height)
else:
    raise SystemExit(f"missing icon renderer for {app_id}")
```

不要让未知应用落入 Pomodoro 图标分支。这样会生成格式合法但内容错误的包，
掩盖应用没有完成注册的问题。

打包器会从 manifest 读取 `version`，输出
`<id>-<version>.cpk`。发布新版本时仍必须同步修改 manifest 的
`version`、`version_code` 和操作表版本；不要手工重命名旧包。

## 9. 构建模拟器包

执行：

```sh
./build-sim.sh --incremental
```

成功后应生成：

```text
build-sim/miniapps/hello-wheel/app.dylib
build-sim/miniapps/packages/hello-wheel-1.0.0.cpk
```

当前 `build-sim.sh` 只自动复制两个参考包。将教程包放进模拟磁盘：

```sh
cp build-sim/miniapps/packages/hello-wheel-1.0.0.cpk \
  build-sim/simdisk/.rockbox/crazypod/miniapps/packages/
```

启动模拟器：

```sh
cd build-sim
./rockboxui
```

进入 Mini Apps 后验证：

1. 列表中出现 Hello Wheel；
2. 快速滚轮时三个按钮逐项高亮，不跨项；
3. Select 修改数值；
4. Play 清零；
5. Menu 返回列表；
6. 退出并重新进入后数值仍存在。

每次重新运行 `build-sim.sh` 都可能重建系统包目录。若需要长期集成，应把
`hello-wheel-1.0.0.cpk` 加入 `build-sim.sh` 的复制列表。

## 10. 构建真机包

执行：

```sh
./build-hw.sh --incremental
```

成功后应生成：

```text
build-hw-ipod6g/miniapps/hello-wheel/app.arm
dist/miniapps/hello-wheel-1.0.0.cpk
```

只通过 `/MiniApps/Install` 侧载时，不需要修改最终固件 ZIP 的资源列表。
如果要把应用作为系统包随固件发布，还要把
`hello-wheel-1.0.0.cpk` 加入 `build-hw.sh` 中复制到
`.rockbox/crazypod/miniapps/packages` 的文件列表。

硬件构建还会验证主、IRQ 和 FIQ 栈边界的 8 字节对齐。这个检查不能跳过，
因为 ARM EABI 的 `double` 和可变参数格式化依赖正确栈对齐。

## 11. 检查 CPK

先检查 ZIP：

```sh
unzip -tq dist/miniapps/hello-wheel-1.0.0.cpk
zipinfo -1 dist/miniapps/hello-wheel-1.0.0.cpk
```

必须恰好出现：

```text
manifest.ini
app.arm
icon.bmp
signature.ed25519
```

检查 manifest 和整体哈希：

```sh
unzip -p dist/miniapps/hello-wheel-1.0.0.cpk manifest.ini
shasum -a 256 dist/miniapps/hello-wheel-1.0.0.cpk
```

Loader 会拒绝压缩条目、目录、额外文件、重复名称、ZIP64、注释、错误 target、
错误 ABI、错误哈希、无效签名和不合法的原生载荷头。

## 12. 安装到 iPod

在 iPod 数据卷根目录创建：

```text
/MiniApps/Install
```

复制：

```text
hello-wheel-1.0.0.cpk
```

完整目标路径：

```text
/MiniApps/Install/hello-wheel-1.0.0.cpk
```

同步并安全弹出设备。重启后打开 Mini Apps，CrazyPod 会扫描导入目录，验证
包并以暂存目录完成原子安装。

安装规则：

- 相同且完整的版本保持不变；
- 相同版本但安装内容损坏时会修复；
- 已安装的更高 `version_code` 不会被低版本覆盖；
- 应用安装到 `/.crazypod/miniapps/<id>`；
- 应用状态和闹钟存放在 `/.crazypod/miniapp-data/<id>`。

不要直接手工构造 `/.crazypod/miniapps/<id>`。该目录包含由 Loader 生成的
安装记录，缺失或不匹配会导致应用不进入注册表。

## 13. 使用状态 API

每个应用最多保存 16 KiB 状态。宿主会添加版本、长度、CRC 和原子临时文件
替换；应用仍应给自己的结构加 magic 和版本字段。

推荐模式：

```c
struct app_state {
    uint32_t magic;
    uint16_t version;
    uint16_t struct_size;
    /* app fields */
};
```

加载时必须同时检查：

- `state_read()` 返回长度等于预期；
- magic 正确；
- 应用状态版本受支持；
- `struct_size` 与当前结构兼容。

结构升级时不要盲目读取旧版本为新结构。

## 14. 使用后台闹钟

ABI 1 的旧接口对应闹钟槽 0：

```c
host->alarm_set(deadline_epoch, token);
host->alarm_cancel();
host->alarm_fired(&token);
host->alarm_acknowledge();
```

SDK revision 2 为每个应用提供四个独立持久闹钟槽。使用前必须检查能力：

```c
if(CP_HOST_HAS(host, CP_CAP_MULTIPLE_ALARMS, alarm_set_slot)) {
    host->alarm_set_slot(1, deadline_epoch, token);
}
```

槽号范围是 `0` 到 `CP_MINIAPP_ALARM_SLOT_COUNT - 1`。对应的
`alarm_cancel_slot()`、`alarm_fired_slot()` 和
`alarm_acknowledge_slot()` 也必须通过 `CP_HOST_HAS()` 检查。槽 0
继续使用旧磁盘文件，因此旧 Calculator/Pomodoro 包在新固件上仍兼容。

基本流程：

1. 使用 `epoch_seconds()` 计算绝对截止时间；
2. 生成非零 token；
3. 保存应用状态；
4. 调用 `alarm_set()`；
5. 在 `tick()` 或 `open()` 中检查 `alarm_fired()`；
6. 处理匹配 token 的事件；
7. 调用 `alarm_acknowledge()`。

不要只保存剩余秒数。设备休眠或应用退出后，单调时钟和前台 tick 都不能
代表真实截止时间。完整实现参考
[Pomodoro app.c](pomodoro/app.c) 和
[Pomodoro engine.c](pomodoro/engine.c)。

Mini App 不直接控制音频。宿主负责提示音，因此闹钟可以与音乐播放共存。

## 15. 绘图约束

一个场景最多包含 64 条命令，每条文本最多 47 个可见字节加结尾 `NUL`。
支持：

- `CP_DRAW_RECT`；
- `CP_DRAW_TEXT`；
- `CP_DRAW_RING`；
- `CP_DRAW_DIVIDER`；
- `CP_DRAW_PROGRESS`。
- `CP_DRAW_BITMAP`。

Divider 和 Progress 属于 SDK revision 2，Bitmap 属于 revision 3。
它们复用原有固定大小的
`cp_draw_command`，不会改变 ABI 1 场景布局；应用仍应先检查
`CP_CAP_DRAW_DIVIDER` 或 `CP_CAP_DRAW_PROGRESS`。

使用 SDK 的颜色和字体 token，不要写死 LVGL 对象或字体地址。宿主会根据
当前外观解析 token。

每次 `render()`：

1. 检查 `scene != NULL`；
2. 调用 `cp_scene_reset(scene)`；
3. 设置背景；
4. 按稳定顺序添加命令；
5. 检查 `cp_scene_add()` 是否返回 `NULL`。

坐标必须落在 320×240 屏幕内。Loader 会检查结构和枚举范围，但不会替你
设计可用布局。

## 16. 真机载荷限制

真机载荷只附带 `memcpy`、`memmove`、`memset` 和 `memcmp` 的最小运行时。
不要直接使用：

- `malloc`、`free`；
- `printf`、`snprintf`；
- `fopen`、目录遍历；
- pthread 或 Rockbox 线程；
- LVGL API；
- Rockbox 音频、存储或全局设置 API。

需要显示数字时使用 `host->format_number()`。需要持久化时使用状态 API。
需要计时时使用宿主时钟和闹钟 API。

SDK revision 3 在 ABI 1 原始宿主表尾部继续增加可选能力。旧前缀保持不变，
Loader 接受所需宿主表尺寸不超过当前固件的旧 ABI 1 包。新能力必须这样
检查：

```c
if(CP_HOST_HAS(host, CP_CAP_SYSTEM_INFO, system_info)) {
    struct cp_system_info info = {
        .struct_size = sizeof(struct cp_system_info)
    };

    if(host->system_info(&info) == 0) {
        /* battery_percent、充电、USB、播放、Reduce Motion 和 language */
    }
}
```

`info.language` 使用 `enum cp_language`，可让应用跟随 CrazyPod 当前的九国语言。
固件只向应用报告语言，不会自动翻译应用字符串。内置 Calculator 和 Pomodoro
使用生成的 `sdk/crazypod_miniapp_l10n.h`；第三方应用必须维护自己的翻译表。
宿主文本输入仍只支持可打印 ASCII，不是多语言输入法。

可选宿主能力还包括：

- `format_duration()` 和 `format_datetime()`；
- 四个持久闹钟槽；
- `ui_toast()` 和 `ui_request_close()`；
- Divider 和 Progress 语义绘图；
- 异步文本、选择和确认界面；
- 包内资源查询、分块读取和 RGB565 位图绘制；
- 只读 Now Playing 快照。

`ui_request_close()` 只提交异步关闭请求。应用不能直接销毁宿主路由或
LVGL 对象。

### 异步宿主界面

请求必须使用非零且由应用自己管理的 `request_id`。同一应用只能有一个
活动请求或一个尚未读取的结果：

```c
static uint32_t request_id = 1;

if(CP_HOST_HAS(host, CP_CAP_UI_MODAL, ui_confirm)) {
    host->ui_confirm(request_id, "Reset", "Clear all data?", "Reset");
}

/* 在 tick() 中轮询 */
struct cp_ui_result result = {
    .struct_size = sizeof(struct cp_ui_result)
};
if(CP_HOST_HAS(host, CP_CAP_UI_MODAL, ui_poll_result) &&
   host->ui_poll_result(&result) == 1 &&
   result.request_id == request_id &&
   result.status == CP_UI_RESULT_ACCEPTED) {
    /* 执行确认后的应用操作 */
}
```

`ui_choice()` 最多接受 32 个固定大小的选项。`ui_text_input()` 最多返回
255 字节，但当前输入器只支持可打印 ASCII。它不是中文输入法。

### 包内资源

CPK format 2 固定增加 `resources.bin`，不能向 ZIP 任意添加文件。
资源总量不超过 512 KiB，单项不超过 128 KiB，最多 32 项。原始 RGB565
文件放在 `miniapps/<应用 id>/resources`，命名为：

```text
badge.48x48.rgb565
```

读取前检查能力并初始化输出结构：

```c
struct cp_resource_info info = {
    .struct_size = sizeof(struct cp_resource_info)
};

if(CP_HOST_HAS(host, CP_CAP_RESOURCES, resource_stat) &&
   host->resource_stat("badge", &info) == 0) {
    /* 使用 resource_read("badge", offset, buffer, capacity) 分块读取 */
}
```

绘制位图时创建 `CP_DRAW_BITMAP` 命令，将资源 ID 写入 `text`。当前每个
scene 最多一个位图命令，位图最大 160×160。

### Now Playing 快照

`now_playing()` 只复制状态和元数据，不提供播放控制：

```c
struct cp_now_playing now = {
    .struct_size = sizeof(struct cp_now_playing)
};

if(CP_HOST_HAS(host, CP_CAP_NOW_PLAYING, now_playing) &&
   host->now_playing(&now) == 0 &&
   (now.flags & CP_NOW_PLAYING_AVAILABLE) != 0) {
    /* title、artist、album、elapsed_ms、duration_ms */
}
```

## 17. 常见问题

### 应用没有出现在列表

依次检查：

1. `.cpk` 是否放在 `/MiniApps/Install`；
2. format 1 是否恰好四个、format 2 是否恰好五个 stored 条目；
3. manifest id 是否与操作表 id 相同；
4. 真机是否为 `target=ipod6g`、`binary=app.arm`；
5. ABI 是否为 1；
6. 是否使用当前固件信任的签名密钥；
7. `version_code` 是否低于已安装版本；
8. `app.arm` 是否来自当前 iPod 6G 构建。

### 快速滚轮仍然跳项

检查离散焦点代码是否使用了 `event->steps`。离散焦点只允许 `+1` 或 `-1`。
`steps` 只用于明确的数值编辑状态。

### Select 操作了错误控件

不要在应用内部维护第二套未显示的焦点队列。ABI 1 宿主已经保证 Select
会清除尚未呈现的滚轮输入并作用于当前可见焦点。

### 模拟器能运行，真机不能运行

通常原因：

- 真机对象没有链接 `crazypod_miniapp_runtime.o`；
- 包含了 libc 或系统符号；
- 打包了 `app.dylib` 而不是 `app.arm`；
- target、ABI 或原生头不匹配；
- 真机载荷超过插件缓冲区。

检查：

```sh
arm-none-eabi-nm -u \
  build-hw-ipod6g/miniapps/hello-wheel/app.elf
```

不应出现未由最小运行时或链接器解决的外部符号。

### 修改代码后设备仍运行旧版本

Loader 使用 `version_code` 判断升级。发布新版本时同时修改：

- manifest `version`；
- manifest `version_code`；
- `cp_miniapp_ops.version`。

打包器会根据 manifest 版本生成输出文件名。

## 18. 发布前检查

开发包发布前至少完成：

- 模拟器编译和交互回归；
- iPod 6G ARM 编译；
- 正常主机测试；
- ASan/UBSan 主机测试；
- CPK1 四条目或 CPK2 五条目、资源索引、manifest、哈希和签名检查；
- 快慢速滚轮、反向滚动和快速 Select；
- Menu、Play、Left、Right；
- 退出重进后的状态恢复；
- 休眠、唤醒和后台闹钟；
- 音乐播放并行状态；
- 损坏状态和损坏包拒绝测试。

生产发布必须替换仓库中的公开开发密钥。开发密钥说明见
[keys/README.md](keys/README.md)。

## 19. 下一步

- ABI、绘图 token、输入结构和宿主 API：
  [sdk/crazypod_miniapp.h](sdk/crazypod_miniapp.h)
- 包格式、安装生命周期和安全边界：
  [Mini Apps reference](README.md)
- 标准计算器实现：
  [calculator/app.c](calculator/app.c)
- 持久闹钟实现：
  [pomodoro/app.c](pomodoro/app.c)

当前实现到 SDK revision 3。受控文件导入/导出仍是设计方案；原始文件系统、
播放控制、PCM、线程、网络和传感器没有开放。
