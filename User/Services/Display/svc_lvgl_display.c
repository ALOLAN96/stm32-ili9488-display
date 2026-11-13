/** @file svc_lvgl_display.c @brief RGB565 PARTIAL刷新，单缓冲和粘性故障。 */
#include "svc_lvgl_display.h"
#include "drv_ili9488.h"
#if LVGL_VERSION_MAJOR != 9 || LVGL_VERSION_MINOR != 2 || LVGL_VERSION_PATCH != 2
#error This integration is pinned to LVGL 9.2.2
#endif
#if LV_COLOR_DEPTH != 16 || LV_DRAW_BUF_STRIDE_ALIGN != 1
#error This port requires native RGB565 with tightly packed rows
#endif
/** LVGL缓冲与内存池必须位于CPU可访问RAM；DMA未启用。 */
static uint32_t draw_buffer[480U*20U/2U];
static lv_display_t *display;
static DRV_ILI9488_Info bound;
static uint32_t (*clock_ms)(void);
static uint32_t started,budget;
static bool active;
static DRV_ILI9488_Status fault;
/** 尺寸/方向由Device拥有；绑定期禁止外部更改，否则暂停。 */
static bool device_ready(void)
{
    DRV_ILI9488_Info i;
    return DRV_ILI9488_GetInfo(&i)==DRV_ILI9488_OK && i.state==DRV_ILI9488_READY &&
        i.width==bound.width && i.height==bound.height && i.rotation==bound.rotation;
}
/** 每个LVGL块是紧密RGB565，FSMC半字写无需字节交换。 */
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *pixels)
{
    if(fault==DRV_ILI9488_OK) {
        if(!active || !device_ready() || a->x1<0 || a->y1<0 || a->x2<a->x1 || a->y2<a->y1 ||
           a->x2>=bound.width || a->y2>=bound.height) {fault=DRV_ILI9488_NOT_READY;}
        else {
            uint32_t elapsed=(uint32_t)(clock_ms()-started);
            if(elapsed>=budget) {fault=DRV_ILI9488_TIMEOUT;}
            else {
                DRV_ILI9488_Rect rect={(uint16_t)a->x1,(uint16_t)a->y1,
                    (uint16_t)(a->x2-a->x1+1),(uint16_t)(a->y2-a->y1+1)};
                size_t count=(size_t)rect.width*rect.height;
                fault=DRV_ILI9488_BlitRect(rect,(const uint16_t *)(const void *)pixels,count,rect.width,budget-elapsed);
            }
        }
    }
    /* 同步消费已结束，即使失败也须解除LVGL等待；fault单独保存。 */
    lv_display_flush_ready(d);
}
DRV_ILI9488_Status SVC_LVGL_Start(uint32_t (*now_ms)(void))
{
    if(display!=NULL || active) {return DRV_ILI9488_INVALID_STATE;}
    if(now_ms==NULL) {return DRV_ILI9488_INVALID_ARGUMENT;}
    if(DRV_ILI9488_GetInfo(&bound)!=DRV_ILI9488_OK || bound.state!=DRV_ILI9488_READY) {return DRV_ILI9488_NOT_READY;}
    active=true; lv_init(); clock_ms=now_ms; lv_tick_set_cb(now_ms);
    display=lv_display_create(bound.width,bound.height);
    if(display==NULL) {lv_deinit();active=false;clock_ms=NULL;return DRV_ILI9488_IO_ERROR;}
    lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display,draw_buffer,NULL,sizeof(draw_buffer),LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display,flush);
    fault=DRV_ILI9488_OK;active=false;return DRV_ILI9488_OK;
}
DRV_ILI9488_Status SVC_LVGL_Process(uint32_t timeout)
{
    if(active) {return DRV_ILI9488_BUSY;}
    if(timeout==0U || timeout>0x7FFFFFFFU) {return DRV_ILI9488_INVALID_ARGUMENT;}
    if(display==NULL) {return DRV_ILI9488_NOT_INITIALIZED;}
    if(fault!=DRV_ILI9488_OK) {return fault;}
    if(!device_ready()) {fault=DRV_ILI9488_NOT_READY;return fault;}
    active=true;started=clock_ms();budget=timeout;
    (void)lv_timer_handler(); active=false;
    return fault;
}
DRV_ILI9488_Status SVC_LVGL_Resume(void)
{
    if(active) {return DRV_ILI9488_BUSY;}
    if(display==NULL) {return DRV_ILI9488_NOT_INITIALIZED;}
    if(!device_ready()) {return DRV_ILI9488_NOT_READY;}
    fault=DRV_ILI9488_OK;lv_obj_invalidate(lv_display_get_screen_active(display));return DRV_ILI9488_OK;
}
DRV_ILI9488_Status SVC_LVGL_Stop(void)
{
    if(active) {return DRV_ILI9488_BUSY;}
    if(display!=NULL) {
        active=true;lv_display_delete(display);display=NULL;lv_deinit();clock_ms=NULL;active=false;
    }
    return DRV_ILI9488_OK;
}
lv_display_t *SVC_LVGL_Display(void) {return display;}
