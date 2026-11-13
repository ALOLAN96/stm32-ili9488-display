/** @file drv_ili9488_port.h
 * @brief Device 向板级适配器借用的同步能力；适配器不得反调 Device。
 * @layer DRV
 */
#ifndef DRV_ILI9488_PORT_H
#define DRV_ILI9488_PORT_H
#include "drv_ili9488_types.h"

/** 失败可已产生部分副作用；适配器不得自动重试整条命令。 */
typedef enum {
    DRV_ILI9488_PORT_OK = 0, DRV_ILI9488_PORT_TIMEOUT,
    DRV_ILI9488_PORT_IO_ERROR, DRV_ILI9488_PORT_UNSUPPORTED
} DRV_ILI9488_PortStatus;

/** 一次公开操作的总预算，单位 ms；回调仅借用，不可修改/保存。 */
typedef struct { uint32_t started_ms, timeout_ms; } DRV_ILI9488_Budget;

/** 表本身被复制，context 借用至重新绑定。所有回调同步且有界。
 * now_ms 单调模 2^32，操作内持续运行；elapsed >= timeout_ms 即超时。
 * wait_ms 成功须至少等待 minimum_ms。units 为一次板级写周期，低位有效。
 * 返回后不保留 units/budget 指针，不得隐含 DMA/后台访问。
 * set_reset 可空；硬复位 profile 要求非空，asserted 为逻辑有效电平。
 * is_ready 只查软件状态，没有硬件副作用，也不表示收到屏幕 ACK。
 */
struct DRV_ILI9488_Port {
    void *context;
    bool (*is_ready)(void *context);
    uint32_t (*now_ms)(void *context);
    DRV_ILI9488_PortStatus (*wait_ms)(void *context, uint32_t minimum_ms,
                                    const DRV_ILI9488_Budget *budget);
    DRV_ILI9488_PortStatus (*write_command8)(void *context, uint8_t command,
                                           const DRV_ILI9488_Budget *budget);
    DRV_ILI9488_PortStatus (*write_data_units)(void *context, const uint16_t *units,
                                             size_t count, const DRV_ILI9488_Budget *budget);
    DRV_ILI9488_PortStatus (*set_reset)(void *context, bool asserted,
                                      const DRV_ILI9488_Budget *budget);
};
#endif
