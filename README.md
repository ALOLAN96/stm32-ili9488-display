# stm32-ili9488-display

STM32 ILI9488 显示单元，提供LVGL和轻量裸机Graphics两种方案，通过FSMC/FMC 8080并口输出。

# ILI9488 同步驱动组件

按 `stm32-architecture-c` 的所有权、依赖与生命周期规则实现。旧 Demo 提供硬件线索及候选初始化参数；驱动按独立分层实现，参数来源与改动见候选说明。

**当前交付是可在主机测试的驱动核心，尚未完成真实屏幕接入。** 已补充普中麒麟F407的FSMC写总线、薄port模板和旧Demo候选面板配置；已提供LVGL两页示例、8×16 ASCII字库和文字渲染，模组候选配置待实板确认。目标为单屏、单调用上下文、同步 C99。LVGL采用固定内存池分配对象。

## 分层与源码清单

```text
App：启动/停止/恢复策略、背光、查询尺寸后重绘
  ↓                           Graphics（可选，独立的通用 fill/blit sink）
Device：drv_ili9488.c + drv_ili9488_panel.c + drv_ili9488_panel_demo.c
  ↓ drv_ili9488_port.h 的同步能力
薄 port：bsp_ili9488_fsmc_port.c，消费宿主时基/就绪能力
  ↓
BSP：bsp_lcd8080_puzhong_f407.c，NE4/A6/16位MMIO写周期
  ↓
CubeMX/HAL：唯一 GPIO/FSMC 静态初始化路径
```

独立核心由 `User/Device/ILI9488/drv_ili9488.c`、`drv_ili9488_panel.c` 组成。已新增的F407适配包含 `User/BSP/bsp_lcd8080_puzhong_f407.c` 与 `User/BSP/Ports/bsp_ili9488_fsmc_port.c`；使用候选配置时另加入 `drv_ili9488_panel_demo.c`，宿主仍需绑定时基/就绪能力并验证实板。包含目录和启动前提见[板级适配依据](docs/board-puzhong-qilin-f407.md)。公共头是 `drv_ili9488.h`、`drv_ili9488_types.h`、`drv_ili9488_port.h`、`drv_ili9488_panels.h`。`*_internal.h` 仅 Device 自身使用。

`tests/`、`tests/fixtures/panel_test.c` 和 `examples/host_demo.c` **全部为主机专用，禁止加入 STM32 固件**。测试 fixture 的电压/Gamma 数值没有模组依据；结构校验通过不代表可上板。

## 已实现接口

| 能力 | API 与语义 |
|---|---|
| 建立软件绑定 | `InitState`：无 I/O，校验后复制表和标量，清零诊断，仅 Uninitialized/Closed |
| 生命周期 | `Open`、`Sleep`、`Wake`、`Close`、`Recover`；显式恢复，无自动重试 |
| 绘制 | `FillRect` / `BlitRect`：RGB565 数值、一次矩形窗口、同步分批传输 |
| 方向 | `SetRotation`：全部发送成功才提交方向/逻辑尺寸 |
| 查询 | `GetInfo` / `GetDiagnostics`：复制快照，失败不写输出 |

0/180 度为 320×480；90/270 度为 480×320。空矩形在 Ready 中无 I/O，但预算仍须合法；其他状态不会因矩形为空而放行。非空越界拒绝，不裁剪。`BlitRect` 的容量与 stride 均以像素计，行间 padding 跳过；调用返回（包括失败）后驱动不保留源指针。

`InitState` 复制 port 表和 config 标量，但 `port.context` 与不可变 `config.panel` 在绑定有效期内必须有效。所有 API 限同一主循环/任务调用，禁止 ISR、并发和 port 回调反调；busy 只能检测同步重入，不能替代线程同步。

驱动只拥有器件状态和协议。GPIO 电平、FSMC 地址/访问宽度、屏障和 `volatile` 属于 BSP；背光属于 App/BSP，port 不重复调用 CubeMX 初始化。

## 传输与时间契约

屏端 **DBI8/DBI16 与 MCU FSMC 配置宽度是不同事实**。驱动支持的软件打包：RGB565 `0xF800` 在 DBI16 发一个 `0xF800` 单元，在 DBI8 发 `0x00F8,0x0000` 两个单元。每个单元代表一次板级写周期；命令参数始终逐字节发送。真实 port 只能启用已核实的模组协议。

默认 `CONFIG_ILI9488_TX_UNITS=64`，允许 2..256 的偶数。该宏在所有 Device 生产 C 编译单元必须一致。暂存空间为 `2 × TX_UNITS` 字节，不是整屏缓存；无堆、VLA、RTOS 对象和后台 DMA。

timeout 合法范围为 `1..0x7FFFFFFF` ms。一次调用只有一个总预算，所有回调都借用相同预算；时基须持续运行、模 2^32 单调，操作不得跨完整计数周期。驱动前后检查预算，晚返回 OK 仍是超时。`wait_ms` 必须真正等满；提前 OK 被判 I/O 错误。软件无法中断卡住的 MMIO，实板必须测量最长访问与阻塞时间。

软复位、硬复位释放、Sleep In/Out 后统一至少等待120ms，模组要求更长则使用更长值。这是保守实现策略，依据 ILItek V100 的复位/睡眠约束；不是宣称每种普通命令都需要120ms。等待记录在回调报错/超时后仍保留，下一次硬件操作先补足剩余等待。[原始手册镜像](https://device.report/m/aeb23170f809610458e05ab514a5e3017344938ccdba1c2031acb453941df38b)

参数/状态拒绝不计运行故障。NOT_READY、TIMEOUT、IO_ERROR、UNSUPPORTED 记一次故障；开始副作用后失败进入 Fault，之前失败保持原状态。恢复尝试仅在 Recover 的首个副作用前增加。计数饱和，成功不擦除上次错误。OK 不代表 ACK、画面正确或 GRAM 保留；Wake/Recover 后查询尺寸并整屏重绘。

复位回调失败后没有隐含“尽力释放”或重试；系统必须确定异常复位电平的处理策略。Close 只表示停止序列发送完成，不是关电或电平安全的证明。

## 面板扩展边界

当前固定序列是复位 → 保护等待 → Sleep Out → 保护等待 → C0 电源参数、E0/E1 Gamma、3A RGB565、36 MADCTL、反色选择 → Display On。Sleep/Close 执行 Display Off → Sleep In → 等待。Wake 重建工作参数与最近成功方向。

候选扩展在 Gamma 后增加 C1、C5、B0、B1、B4、B6、E9、F7；之后继续像素格式、MADCTL、反色选择与 Display On。扩展为 NULL 时维持基础序列。扩展仅接受旧Demo记录的具名参数组合，不开放任意命令脚本。C5三字节和B1单字节是保留原例程的兼容写法，手册差异及待实测事项见[候选说明](docs/legacy-panel-candidate.md)。

应用通过公开配置选择候选，毋需包含私有头：

```c
#include "drv_ili9488.h"
#include "drv_ili9488_panels.h"

/* port由宿主通过BSP_ILI9488_FSMC_MakePort建立；就绪前提见板级说明。 */
DRV_ILI9488_Config config = DRV_ILI9488_DemoCandidateConfig;
config.initial_rotation = DRV_ILI9488_ROTATION_90;
/* 在唯一生命周期所有者的启动函数中检查每一步返回值： */
status = DRV_ILI9488_InitState(&port, &config);
if (status == DRV_ILI9488_OK) {
    status = DRV_ILI9488_Open(1000U);
}
```

这是接入片段，`port` 和 `status` 由宿主声明；不重复初始化FSMC。候选强制DBI16、使用软复位；四方向由原08推导为08/68/C8/A8，最终安装方向、颜色及模拟参数均待实板确认。

## 主机验证

需要 Python 3 和 C99 编译器。Windows 路径使用相邻 `lld.exe` 的 Clang freestanding 环境，不依赖 CRT；Linux 原生共享库路径尚未在本次验证。

```powershell
python tests/run.py --cc '<clang.exe的绝对路径>' --suite all --opt all
python tests/run.py --cc '<clang.exe的绝对路径>' --suite all --opt all --arch 32
python tests/run.py --cc '<clang.exe的绝对路径>' --suite all --opt all --units 2
python tests/run.py --cc '<clang.exe的绝对路径>' --suite all --opt all --units 256
python tests/run.py --cc '<clang.exe的绝对路径>' --suite all --opt 2 --ubsan-trap
python tests/check_arm.py --cc '<支持ARM目标的clang.exe绝对路径>'
python tests/check_runner.py --cc '<clang.exe的绝对路径>'
```

从组件目录执行。每个用例在新进程内运行，失败、编译失败、链接失败、运行异常均返回非零。可用 `--suite api|transport|pixels|lifecycle|boardport|candidate` 缩小范围；`--suite all` 还运行完整生命周期示例。头文件独立编译检查随每轮执行。输出在忽略的 `build/` 下。

具体执行结果、未执行项目见 [验证记录](docs/validation.md)。这些主机检查不替代 STM32 工程构建与实测。

## 实板接入还需要的资料

2026-11-01 已收到并核对正确的ILI9488屏幕图、麒麟F407开发板图及V090手册，详见[板级适配依据](docs/board-puzhong-qilin-f407.md)。已确认图示屏端DBI16、主板A6、公共NRST及固定供电背光；以下清单中对应接线事项已从“资料缺失”转为“待实物版本/连接方式核对”。

2026-11-03 用户指定以旧 Demo 的 ILI9488 初始化分支作为参数参考，已记录[完整候选参数及核对事项](docs/legacy-panel-candidate.md)。无需另行提供厂商初始化表才能继续开发；候选配置已完成具名字段扩展和命令测试，仍须实板验证，不能视为已验证的模组默认值。

原理图、IM脚、数据线、复位和背光资料已收到，不需要重复提供。未来上板时核实实物版本与排针直连关系、供电/NRST波形即可。用户已选择独立模板交付，本阶段不再要求目标工程路径；未来集成时再确定HCLK、刷新频率、允许阻塞时间和主循环/RTOS调用上下文。

真实profile及宿主绑定确认后，复用本模板BSP/port并核算 FSMC 时序，验证红绿蓝、四方向、边界窗口、睡眠唤醒与失败恢复。EIDE 配置/构建按其 Skill/MCP，烧录需要明确授权；本次没有修改旧工程、CubeMX 配置或烧录硬件。

本组件版权权利人和发布许可未确认，未推断相邻组件的许可适用于本组件。

## 上层文字接口

已实现 `GFX_ILI9488_DrawString()`，支持8×16 ASCII、前景/背景色、透明背景、换行、自动换行和边界裁剪。单字符可传长度1。通用Graphics独立于Device，通过Services适配调用BlitRect。用法、源码清单、字体来源和限制见[文字绘制说明](docs/graphics.md)。

## LVGL接入

已接入Git子模块管理的固定版本LVGL v9.2.2、RGB565分块刷新服务和最小两页示例。数字标签变短由LVGL自动重绘旧区域。启动、周期调用、恢复、停止和源码参与清单见[LVGL接入说明](docs/lvgl-integration.md)。使用LVGL时由单一显示所有者调用，不能混用直接Graphics绘图；本阶段仍是独立模板，尚未实板验证。

首次克隆后，从父仓库根目录执行 `git submodule update --init -- User/ThirdParty/lvgl` 获取已锁定依赖。升级与离线打包说明见LVGL接入文档。

## 显示单元可选方案

双方案分层、生命周期与主循环接入见[显示单元设计](docs/display-unit-design.md)。轻量裸机方案使用baremetal-sources.txt，LVGL方案使用lvgl-sources.txt，两套共用Device和板级适配。实际工程按选定清单加入源码及包含目录。
