/** @file drv_ili9488.h
 * @brief 单屏、单主循环/任务上下文、同步 C99 驱动。禁止 ISR/并发调用。
 * @layer DRV
 * @note 无内部锁；所有 API 同步重入返回 BUSY。timeout_ms 范围 1..0x7FFFFFFF。
 * 公共函数完整契约见对应 .c 定义及组件 README；port 定义单独包含。
 */
#ifndef DRV_ILI9488_H
#define DRV_ILI9488_H
#include "drv_ili9488_types.h"

DRV_ILI9488_Status DRV_ILI9488_InitState(const DRV_ILI9488_Port *port,
                                      const DRV_ILI9488_Config *config);
DRV_ILI9488_Status DRV_ILI9488_GetInfo(DRV_ILI9488_Info *info);
DRV_ILI9488_Status DRV_ILI9488_GetDiagnostics(DRV_ILI9488_Diagnostics *diagnostics);
DRV_ILI9488_Status DRV_ILI9488_Open(uint32_t timeout_ms);
DRV_ILI9488_Status DRV_ILI9488_SetRotation(DRV_ILI9488_Rotation rotation, uint32_t timeout_ms);
DRV_ILI9488_Status DRV_ILI9488_FillRect(DRV_ILI9488_Rect rect, uint16_t color_rgb565,
                                     uint32_t timeout_ms);
DRV_ILI9488_Status DRV_ILI9488_BlitRect(DRV_ILI9488_Rect rect, const uint16_t *pixels,
                                     size_t buffer_pixels, size_t stride_pixels,
                                     uint32_t timeout_ms);
DRV_ILI9488_Status DRV_ILI9488_Sleep(uint32_t timeout_ms);
DRV_ILI9488_Status DRV_ILI9488_Wake(uint32_t timeout_ms);
DRV_ILI9488_Status DRV_ILI9488_Close(uint32_t timeout_ms);
DRV_ILI9488_Status DRV_ILI9488_Recover(uint32_t timeout_ms);
#endif
