/** @file drv_ili9488_internal.h @brief Device 两个编译单元间的私有事务接口。 @layer DRV */
#ifndef DRV_ILI9488_INTERNAL_H
#define DRV_ILI9488_INTERNAL_H
#include "drv_ili9488_panel_internal.h"

/** 每次板级 data 回调的最大写周期数，不是像素数。 */
#ifndef CONFIG_ILI9488_TX_UNITS
#define CONFIG_ILI9488_TX_UNITS 64
#endif
#if CONFIG_ILI9488_TX_UNITS < 2 || CONFIG_ILI9488_TX_UNITS > 256 || (CONFIG_ILI9488_TX_UNITS % 2) != 0
#error "CONFIG_ILI9488_TX_UNITS must be even and within 2..256"
#endif

/** @brief 饱和计数，防止长期运行回绕为零。 @retval None */
static inline void ILI9488_CountFailure(uint32_t *count)
{ if (*count < UINT32_MAX) { ++(*count); } }

/* 以下函数只能在持有 device.busy 的公开事务内使用。 */
DRV_ILI9488_Status ILI9488_Command(uint8_t command, uint32_t guard_ms);
DRV_ILI9488_Status ILI9488_Bytes(const uint8_t *bytes, size_t count);
DRV_ILI9488_Status ILI9488_Wait(uint32_t minimum_ms);
DRV_ILI9488_Status ILI9488_WaitGuard(void);
DRV_ILI9488_Status ILI9488_Reset(bool asserted, uint32_t guard_ms);
DRV_ILI9488_Status ILI9488_PanelOpen(const DRV_ILI9488_Panel *panel, DRV_ILI9488_Rotation rotation);
DRV_ILI9488_Status ILI9488_PanelWake(const DRV_ILI9488_Panel *panel, DRV_ILI9488_Rotation rotation);
DRV_ILI9488_Status ILI9488_PanelStop(const DRV_ILI9488_Panel *panel);
#endif
