/** @file mock_port.h @brief 测试专用板级记录器，不进入固件。 */
#ifndef ILI9488_MOCK_PORT_H
#define ILI9488_MOCK_PORT_H
#include "drv_ili9488_port.h"
#include "drv_ili9488_panel_internal.h"
/** 外部可见的总线事件；DATA 保存数据数组索引和周期数。 */
typedef enum { MOCK_CMD, MOCK_DATA, MOCK_WAIT, MOCK_RESET } MockKind;
typedef struct { MockKind kind; uint32_t time, value; size_t count; } MockEvent;
/** 有界测试记录；溢出标记让测试失败，不静默截断。 */
typedef struct {
    uint32_t now, ready_cost, io_cost;
    bool ready, short_wait, overflow, budget_mismatch, capture;
    size_t calls, events_count, units_count, max_batch, fail_at;
    unsigned queries;
    DRV_ILI9488_PortStatus failure;
    DRV_ILI9488_Budget first_budget;
    MockEvent events[4096];
    uint16_t units[16384];
    void (*hook)(void);
} Mock;
/** 每个 case 独立进程；唯一 mock 仅由测试/同步回调访问。 */
extern Mock mock;
extern const DRV_ILI9488_Panel test_panel;
DRV_ILI9488_Port Mock_Port(void);
void Mock_Reset(void);
void Mock_ClearTrace(void);
DRV_ILI9488_Config Mock_Config(DRV_ILI9488_DBIWidth width);
#endif
