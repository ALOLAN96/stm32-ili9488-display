# LVGL显示接入

2026-11-05，独立模板增量；未接入旧Demo、修改EIDE配置或烧录。

## 层次与版本

App/LvglDemo组织示例页面和业务数据；Services/Display/svc_lvgl_display独占LVGL初始化、显示和周期处理；LVGL渲染结果经服务调用Device BlitRect，再到port/BSP/FSMC。旧轻量Graphics保留，但LVGL运行时不得混合调用它或直接绘图，否则LVGL不知道外部画面变化。

固定LVGL **v9.2.2**作为本模板验证基线，不表示最新版。上游来源 https://github.com/lvgl/lvgl/tree/v9.2.2 ，下载地址、归档SHA256及每个导入文件的哈希见User/ThirdParty/lvgl-source.json。当前本机通过稀疏检出获取src及根目录文件，完整上游仓库由子模块引用；上游源码无本地修改。2026-11-06起使用Git子模块管理，固定提交7f07a129e8d77f4984fff8e623fd5be18ff42e74，原导入的791个文件哈希保留用于迁移核对。按MIT声明保留版权及许可；本工程lv_conf.h为项目配置，不改上游模板。

## 编译参与

lvgl-sources.txt为固件生产源码清单，路径相对组件根目录；覆盖上游全部C文件以及Device/BSP/port/显示服务/示例App。关闭的功能由lv_conf.h排除，很多上游编译单元为空。不要加入tests或旧host_demo。

包含目录：

- User/Config
- User/ThirdParty/lvgl
- User/Device/ILI9488
- User/BSP
- User/BSP/Ports
- User/Services/Display
- User/App/LvglDemo

所有编译单元定义LV_CONF_INCLUDE_SIMPLE。使用目标工具链自己的标准C头和运行库。此清单未改任何实际Keil/EIDE工程；未来集成需检查源码、宏、链接及map，不能仅凭文件存在宣称目标工程已构建。

## 启动与周期调用

宿主完成供电/复位前提、FSMC配置和port绑定。先通过DRV_ILI9488_InitState/Open使器件进入READY。然后由同一个显示所有者执行：

```c
#include "svc_lvgl_display.h"
#include "app_lvgl_demo.h"

/* 与port同源的模2^32毫秒；回调中不得调用LVGL或阻塞。 */
static uint32_t display_now_ms(void) { return HAL_GetTick(); }

/* 在初始化函数内，依次检查错误；HAL引用只存在于宿主。 */
status = SVC_LVGL_Start(display_now_ms);
if (status == DRV_ILI9488_OK) {
    status = APP_LVGL_DemoCreate();
}
/* 创建失败：调用APP_LVGL_DemoDestroy及SVC_LVGL_Stop清理。 */
```

主循环或唯一显示任务每1..5ms调用SVC_LVGL_Process(timeout_ms)，逐次检查返回值。不要另行调用lv_init/lv_deinit/lv_tick_inc/lv_tick_set_cb/lv_timer_handler/lv_refr_now。LVGL通过tick回调取时间，默认33ms触发刷新；不需要新增SysTick拥有者。RTOS工程由既有调度方式安排显示任务，禁止从其它任务或ISR直接修改控件。

```c
/* 以下调用都在显示所有者上下文中。 */
status = APP_LVGL_DemoSetValue(12345);
/* 后续更新同一标签；背景重绘由LVGL管理。 */
status = APP_LVGL_DemoSetValue(12);
status = APP_LVGL_DemoShowPage(1U); /* 0=数字页，1=状态页 */
status = SVC_LVGL_Process(100U);
```

目前没有接入物理触摸或按键输入；页面选择可由应用转交事件实现。演示使用LVGL自带Montserrat14 ASCII字库，不依赖旧Graphics点阵；中文需要单独选取字形资源。

## 内存和时序

单个4字节对齐RGB565缓冲为480×20×2=19200字节，支持器件绑定时两种逻辑宽度。LVGL内部固定内存池48KiB；软件渲染、无OS、无DMA。两项静态存储共68352字节，此外还有对象之外的全局状态、栈、Device和应用内存。对象在池中分配，不调用系统malloc；不再适用“整个显示栈无动态分配”的表述。

LVGL分配/NULL断言保留默认致命处理，内存耗尽可能进入assert死循环；上板前必须评估峰值、余量和系统fatal策略。API中的NULL检查不代表上游所有OOM路径均可恢复。此模板不是生产内存/栈余量已验证的声明。

Config固定16位RGB565和stride对齐1，flush区域逐行紧密排列，直接交给BlitRect，不交换字节。FSMC按RGB565半字写；不能套用SPI字节交换例程。

Process为全部flush共享一个总I/O预算，每块得到剩余毫秒；不能抢占LVGL CPU渲染，亦不能中断卡住的MMIO。绘图复杂度与阻塞时长须在实板测量。不要将整个Process解释成可硬性抢占的实时截止时间。

## 故障、方向与停止

首次flush失败保留错误，后续块不发送，但每个flush均调用flush_ready释放LVGL等待，避免死锁。Process随后返回粘性错误并暂停。不能把flush_ready当作屏幕已经显示成功的确认。

应用显式恢复Device后调用SVC_LVGL_Resume；确认READY且方向/尺寸与绑定一致时，服务清除故障并标记整屏重绘。Device故障采用Recover；仅服务预算到期而Device仍READY时，可直接Resume。没有自动重试。

运行期间禁止直接旋转或睡眠Device，亦不允许通过借用display指针删除/旋转显示。检测到方向、尺寸或READY状态变化会暂停。需切换方向时，先APP_LVGL_DemoDestroy，再SVC_LVGL_Stop，再修改器件方向，重新Start/Create。这个顺序也适用于睡眠：销毁UI/停止服务后由应用停止Device；唤醒后重建UI。

Stop只释放LVGL资源，不关闭硬件。应用UI指针必须在Stop前销毁，Stop后全部LVGL指针失效。服务独占整个LVGL引擎，不与其它显示实例共享lv_init/lv_deinit。

## 验证

测试脚本tests/check_lvgl.py编译真实上游C源码，不使用LVGL接口替身。主机测试通过实际Device/port将像素写到测试专用320×480模拟GRAM；该帧缓冲不进入固件。

五个独立进程用例覆盖启动/停止与页面、短文本整屏对比、发送失败暂停与恢复、总预算及方向变更、480×320横屏实际渲染。短文本用例确认存在可见像素且12345到12确实改变画面，再与重新启动后只显示12的整屏结果逐像素比较。

脚本参数--cc、--headers、--extra-headers指定Windows Clang及可用C标准头目录；本次使用现有本机Clang和C声明头，无需安装或改环境。host模式为Windows x64真实CRT导入，导入库生成仅用于主机测试；实际测试DLL链接不允许未解析符号。--arm模式使用目标arm-none-eabi/cortex-m4进行真实库及适配对象编译；并未链接完整STM32固件。固件禁止引用主机C头目录或测试CRT。

日志见build/lvgl-host.log、lvgl-arm.log、lvgl-core-regression.log。尚未实板验证屏幕初始化候选、供电/NRST、颜色方向、撕裂、刷新时长和内存栈余量。

## 子模块获取与升级

仓库通过.gitmodules和gitlink固定LVGL提交。克隆后在仓库根目录执行以下命令获取依赖。

```sh
git submodule update --init -- User/ThirdParty/lvgl
```

也可直接git clone --recurse-submodules。项目配置位于User/Config，显示适配与App分别维护。有意升级时，在子模块切换目标提交并验证配置、源码清单、主机运行与目标构建，然后提交父仓库的新指针。

本地可使用git sparse-checkout set src保留构建需要的目录。离线交付应打包已初始化的源码及许可声明，父仓库git archive不会自动包含子模块源码。
