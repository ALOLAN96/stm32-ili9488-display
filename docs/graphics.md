# ASCII 文字绘制（2026-11-05）

按照stm32-architecture-c采用implementation / production / standard范围。Module负责硬件无关字形渲染，Services/Display适配ILI9488，Device继续拥有窗口和总线协议。无堆、无帧缓冲，行暂存最多32字节；单任务/主循环调用，禁止ISR和并发旋转。

## 使用

完成Device初始化并Open成功后，在显示所有者中调用：

```c
#include "gfx_ili9488.h"

/* 与Device port使用同源的持续毫秒时基，不能在等待中停止。 */
static uint32_t display_now(void *context)
{
    (void)context;
    return HAL_GetTick();
}

/* 位于应用函数内，检查返回值；RGB565红字黑底。 */
GFX_TextStyle style = {0xF800U, 0x0000U, true, true};
GFX_Status result = GFX_ILI9488_DrawString(
    10, 20, "Hello ILI9488!\nSTM32 F407", 26U, &style, display_now, 1000U);
```

示例HAL引用仅属于宿主应用。now回调的context固定NULL，可以自行包装已有时基。count是最大读取字节数，NUL提前结束；可用sizeof(text)-1避免手算。单字符使用长度1的字符串调用。返回GFX_OK、INVALID_ARGUMENT、TIMEOUT或IO_ERROR；Device细节通过其诊断查询。

- 字体为8×16，覆盖空格至波浪号的95个ASCII字符，包括大小写、数字、标点；中文/UTF-8当前拒绝，且在输出前校验整个输入。
- 支持前景/背景RGB565；opaque=false仅绘制前景，不读取原屏幕。透明绘制可能产生更多短窗口。
- LF换行并回起始x；CR仅回起始x；CRLF产生一次换行。wrap=true时越右边界换行至起始x，否则直接裁剪；负坐标与上下左右边界均裁剪。不处理tab或自动滚屏。
- 一次字符串调用共享一个总预算，每次Device写操作取得剩余毫秒。超时/失败可能留下部分文字，不回滚。软件无法中断卡住的底层回调。
- 通用GFX_DrawString可接受自定义只读字体（宽1..16，行按MSB在左）；GFX_Surface由宿主提供尺寸、时基和同步行输出。文字指针、字体和surface在调用期间有效，回调不得保留行缓冲指针。

## 源码与集成

新增生产源码：

- User/Module/Graphics/gfx_text.c
- User/Module/Graphics/gfx_font8x16.c
- User/Services/Display/gfx_ili9488.c

增加包含目录User/Module/Graphics、User/Services/Display；与现有Device/BSP一起编译。应用不包含私有面板头。字体资源可替换；图形原语、printf格式化、中文和多字号尚未实现。

## 字体来源

字库来自用户旧工程Drivers/BSP/Inc/font.h的ascii_1608数组。仅将列优先高位在上的点阵转换为逐行MSB在左，未迁移旧绘图函数。原文件SHA256：b96d19f40c3f0dfca5c33995787105c9d0748617e9075ac6d8662f4602e4e507

旧工程字体的版权/发布许可未确认，未推断或添加开源许可。此来源记录应随代码保留。

## 验证边界

主机测试检查逐像素自定义字形、换行、裁剪、透明背景、wrap、回绕超时、失败停止和真实Device适配调用；编译检查不替代实板文字方向、颜色与刷新速度测试。见validation.md。

## 固定字段自动擦除尾部

新增GFX_ILI9488_DrawTextField(x,y,text,count,columns,style,now,timeout)。例如columns=5，连续写入12345再写入12，会自动补空格，覆盖整个40×16区域。count为输入字节上限，columns为固定字段字符数（1..64）。空文本清空字段；超长、控制字符或字段超出屏幕时拒绝且无输出。接口强制背景覆盖与单行，忽略style.opaque/wrap，仅使用前景/背景色。使用同一字段位置和宽度更新即可；不会记忆其它DrawString调用的历史区域。超时/传输失败可能只更新部分内容，可由应用重绘整个字段。
