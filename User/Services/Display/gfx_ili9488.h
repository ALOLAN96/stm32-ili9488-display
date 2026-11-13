/** @file gfx_ili9488.h @brief Graphics与ILI9488的同步服务适配。 */
#ifndef GFX_ILI9488_H
#define GFX_ILI9488_H
#include "gfx_text.h"
/** 单一显示所有者调用；now_ms(NULL)必须与Device时基同源，调用期间不得旋转。
 * 选择8x16 ASCII字体。驱动非READY或传输失败返回GFX_IO_ERROR，详细错误查询Device诊断。
 */
GFX_Status GFX_ILI9488_DrawString(int16_t x,int16_t y,const char *text,size_t count,
    const GFX_TextStyle *style,uint32_t (*now_ms)(void *),uint32_t timeout_ms);
/** 固定单行字段，columns为1..64字符；字段必须完整位于屏幕内。
 * 自动补空格并强制背景覆盖，不使用style的opaque/wrap选项。
 * ASCII文本超长/含换行直接拒绝，count=0可清空字段；不记录上次文本。
 */
GFX_Status GFX_ILI9488_DrawTextField(int16_t x,int16_t y,const char *text,size_t count,
    size_t columns,const GFX_TextStyle *style,uint32_t (*now_ms)(void *),uint32_t timeout_ms);
#endif
