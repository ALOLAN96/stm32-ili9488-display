/** @file app_lvgl_demo.c @brief 静态文本缓冲，LVGL管理标签旧区域重绘。 */
#include "app_lvgl_demo.h"
#include "svc_lvgl_display.h"
static lv_obj_t *pages[2],*value_label;
static char value_text[16];
DRV_ILI9488_Status APP_LVGL_DemoCreate(void)
{
    lv_display_t *d=SVC_LVGL_Display();lv_obj_t *title;
    if(d==NULL) {return DRV_ILI9488_NOT_READY;}
    if(pages[0]!=NULL) {return DRV_ILI9488_INVALID_STATE;}
    pages[0]=lv_obj_create(lv_display_get_screen_active(d));
    pages[1]=lv_obj_create(lv_display_get_screen_active(d));
    if(pages[0]==NULL || pages[1]==NULL) {APP_LVGL_DemoDestroy();return DRV_ILI9488_IO_ERROR;}
    for(unsigned i=0;i<2U;++i) {
        lv_obj_set_size(pages[i],lv_pct(100),lv_pct(100));
        lv_obj_set_style_bg_color(pages[i],lv_color_black(),0);
        lv_obj_set_style_bg_opa(pages[i],LV_OPA_COVER,0);
        lv_obj_remove_flag(pages[i],LV_OBJ_FLAG_SCROLLABLE);
    }
    title=lv_label_create(pages[0]);value_label=lv_label_create(pages[0]);
    if(title==NULL || value_label==NULL) {APP_LVGL_DemoDestroy();return DRV_ILI9488_IO_ERROR;}
    lv_obj_set_style_text_color(title,lv_color_white(),0);
    lv_label_set_text_static(title,"ILI9488 / LVGL");lv_obj_set_pos(title,8,8);
    lv_obj_set_pos(value_label,8,48);lv_obj_set_style_text_color(value_label,lv_color_white(),0);
    title=lv_label_create(pages[1]);
    if(title==NULL) {APP_LVGL_DemoDestroy();return DRV_ILI9488_IO_ERROR;}
    lv_obj_set_style_text_color(title,lv_color_white(),0);
    lv_label_set_text_static(title,"Status: running");lv_obj_set_pos(title,8,8);
    (void)APP_LVGL_DemoSetValue(0);return APP_LVGL_DemoShowPage(0U);
}
DRV_ILI9488_Status APP_LVGL_DemoSetValue(int32_t value)
{
    if(value_label==NULL) {return DRV_ILI9488_NOT_INITIALIZED;}
    lv_snprintf(value_text,sizeof(value_text),"%ld",(long)value);
    lv_label_set_text_static(value_label,value_text);return DRV_ILI9488_OK;
}
DRV_ILI9488_Status APP_LVGL_DemoShowPage(unsigned page)
{
    if(page>1U) {return DRV_ILI9488_INVALID_ARGUMENT;}
    if(pages[0]==NULL) {return DRV_ILI9488_NOT_INITIALIZED;}
    lv_obj_add_flag(pages[1U-page],LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(pages[page],LV_OBJ_FLAG_HIDDEN);return DRV_ILI9488_OK;
}
void APP_LVGL_DemoDestroy(void)
{
    for(unsigned i=0;i<2U;++i) {if(pages[i]!=NULL) {lv_obj_delete(pages[i]);pages[i]=NULL;}}
    value_label=NULL;
}
