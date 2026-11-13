/** @file svc_lvgl_display.h @brief 独占LVGL引擎的同步显示服务。 */
#ifndef SVC_LVGL_DISPLAY_H
#define SVC_LVGL_DISPLAY_H
#include "drv_ili9488_types.h"
#include "lvgl.h"
/** 唯一主循环/显示任务调用所有API及LVGL API，禁止ISR/并发/回调重入。
 * Start要求Device已Open且时基与port同源，服务独占lv_init/lv_deinit/tick。
 * now_ms为模2^32毫秒且必须持续推进。NULL拒绝。
 */
DRV_ILI9488_Status SVC_LVGL_Start(uint32_t (*now_ms)(void));
/** 每1..5ms调用；budget是本次总刷新I/O时限，不能抢占LVGL CPU渲染。
 * 首次发送失败粘性暂停，所有flush均释放缓冲但不将错误当成功。
 */
DRV_ILI9488_Status SVC_LVGL_Process(uint32_t budget_ms);
/** 应用先显式恢复Device，方向不变时Resume整屏重绘；不自动复位硬件。 */
DRV_ILI9488_Status SVC_LVGL_Resume(void);
/** 先销毁应用UI，再Stop；释放引擎，不关闭Device。 */
DRV_ILI9488_Status SVC_LVGL_Stop(void);
/** 借用显示指针，Stop后失效；仅供同一所有者的App创建UI，不得删除/旋转显示。 */
lv_display_t *SVC_LVGL_Display(void);
#endif
