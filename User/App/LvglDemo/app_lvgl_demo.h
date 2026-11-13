/** @file app_lvgl_demo.h @brief 最小两页状态显示，无触摸依赖。 */
#ifndef APP_LVGL_DEMO_H
#define APP_LVGL_DEMO_H
#include "drv_ili9488_types.h"
/** 全部函数由显示所有者调用；Start服务后Create，Stop服务前Destroy。 */
DRV_ILI9488_Status APP_LVGL_DemoCreate(void);
DRV_ILI9488_Status APP_LVGL_DemoSetValue(int32_t value);
DRV_ILI9488_Status APP_LVGL_DemoShowPage(unsigned page);
void APP_LVGL_DemoDestroy(void);
#endif
