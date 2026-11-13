/** @file gfx_text.h @brief 硬件无关同步ASCII文字渲染；无堆和帧缓冲。 */
#ifndef GFX_TEXT_H
#define GFX_TEXT_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
typedef enum { GFX_OK=0,GFX_INVALID_ARGUMENT,GFX_TIMEOUT,GFX_IO_ERROR } GFX_Status;
/** 数据为逐字形、逐行、MSB在左；宽度1..16，行占ceil(width/8)字节。
 * data与描述在调用期间有效；data_bytes用于验证容量。
 */
typedef struct { uint8_t width,height,first,last; const uint8_t *data; size_t data_bytes; } GFX_Font;
extern const GFX_Font GFX_Font8x16;
/** RGB565；opaque=false时只画前景。wrap=true时超右边界换行至起始x。 */
typedef struct { uint16_t foreground,background; bool opaque,wrap; } GFX_TextStyle;
/** 单调用上下文，非ISR。回调同步消费像素，不保留指针；now_ms持续推进且可回绕。
 * write_row必须遵守剩余timeout；失败可能已部分输出。尺寸由宿主提供。
 */
typedef struct {
    void *context; uint16_t width,height;
    uint32_t (*now_ms)(void *context);
    GFX_Status (*write_row)(void *,uint16_t,uint16_t,const uint16_t *,size_t,uint32_t);
} GFX_Surface;
/** count为字节上限，遇NUL提前结束；只接受字体范围内ASCII、CR、LF。
 * 全部输入先校验，无效输入无输出。CR回到起始x，LF换行并回到起始x。
 * 支持负坐标和边界裁剪；预算为整次调用1..7FFFFFFF毫秒。
 * 不支持UTF-8、缩放或格式化；失败不回滚已绘制内容。
 */
GFX_Status GFX_DrawString(const GFX_Surface *,int16_t,int16_t,const char *,size_t,
                          const GFX_Font *,const GFX_TextStyle *,uint32_t);
#endif
