/** @file bsp_ili9488_fsmc_port.h @brief ILI9488同步契约与板级8080能力的薄适配。 @layer BSP/port */
#ifndef BSP_ILI9488_FSMC_PORT_H
#define BSP_ILI9488_FSMC_PORT_H
#include "drv_ili9488_port.h"

/** 宿主提供的运行能力；表和context对象在Device绑定期持续有效，表不可修改。
 * is_ready无硬件副作用，包含总线配置、供电/硬复位前提和启动等待已完成。
 * now_ms为持续运行的模2^32毫秒时基。idle可空（忙等），或有界让出CPU；
 * 不得停止时基/操作总线/反调Device。唯一调用上下文，不允许并发重绑。
 */
typedef struct {
    void *context;
    bool (*is_ready)(void *context);
    uint32_t (*now_ms)(void *context);
    void (*idle)(void *context);
} BSP_ILI9488_FSMC_Services;

DRV_ILI9488_Status BSP_ILI9488_FSMC_MakePort(const BSP_ILI9488_FSMC_Services *services,
                                         DRV_ILI9488_Port *port);
#endif
