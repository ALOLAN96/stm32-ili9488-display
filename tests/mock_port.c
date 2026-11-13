/** @file mock_port.c @brief 虚拟时钟、总线记录与一次性故障注入。 */
#include "mock_port.h"
/** 测试独占；静态容量避免依赖宿主 CRT 分配器。 */
Mock mock;

/** @brief 清空一次公开操作的跟踪，不改变虚拟时间或故障配置。 */
void Mock_ClearTrace(void)
{
    mock.calls = 0U; mock.events_count = 0U; mock.units_count = 0U;
    mock.max_batch = 0U; mock.queries = 0U; mock.overflow = false;
    mock.budget_mismatch = false;
}
/** @brief 只重置测试替身，不接触驱动生命周期。 */
void Mock_Reset(void)
{
    mock.now = 0U; mock.ready_cost = 0U; mock.io_cost = 0U;
    mock.ready = true; mock.short_wait = false; mock.fail_at = 0U; mock.capture = true;
    mock.failure = DRV_ILI9488_PORT_IO_ERROR; mock.hook = NULL;
    Mock_ClearTrace();
}
/** @brief 返回指定 DBI 协议宽度的合成配置。 */
DRV_ILI9488_Config Mock_Config(DRV_ILI9488_DBIWidth width)
{
    DRV_ILI9488_Config c = {width, DRV_ILI9488_ROTATION_0, &test_panel};
    return c;
}
/** @brief 检测同一操作是否重置预算，记录副作用或等待。 */
static DRV_ILI9488_PortStatus record(MockKind kind, uint32_t value, size_t count,
                                    const DRV_ILI9488_Budget *budget)
{
    if (mock.calls == 0U) { mock.first_budget = *budget; }
    else if ((mock.first_budget.started_ms != budget->started_ms) ||
             (mock.first_budget.timeout_ms != budget->timeout_ms)) { mock.budget_mismatch = true; }
    ++mock.calls;
    if (!mock.capture) { /* 整屏示例只计调用，避免把测试记录器变为帧缓冲。 */ }
    else if (mock.events_count < 4096U) {
        MockEvent *e = &mock.events[mock.events_count++];
        e->kind = kind; e->time = mock.now; e->value = value; e->count = count;
    } else { mock.overflow = true; }
    if (mock.hook != NULL) { mock.hook(); }
    mock.now += mock.io_cost;
    return (mock.calls == mock.fail_at) ? mock.failure : DRV_ILI9488_PORT_OK;
}
/** @brief 软件就绪查询，支持在首个副作用前耗尽预算。 */
static bool ready(void *context)
{
    (void)context; ++mock.queries; mock.now += mock.ready_cost;
    if (mock.hook != NULL) { mock.hook(); }
    return mock.ready;
}
/** @brief 模 2^32 毫秒时基。 */
static uint32_t now(void *context) { (void)context; ++mock.queries; return mock.now; }
/** @brief 可注入失败、提前 OK 和预算到期的有界等待。 */
static DRV_ILI9488_PortStatus wait_ms(void *context, uint32_t ms, const DRV_ILI9488_Budget *b)
{
    uint32_t elapsed, remaining;
    DRV_ILI9488_PortStatus status;
    (void)context; status = record(MOCK_WAIT, ms, 0U, b);
    if (status != DRV_ILI9488_PORT_OK || mock.short_wait) { return status; }
    elapsed = (uint32_t)(mock.now - b->started_ms);
    remaining = elapsed < b->timeout_ms ? b->timeout_ms - elapsed : 0U;
    if (ms >= remaining) { mock.now += remaining; return DRV_ILI9488_PORT_TIMEOUT; }
    mock.now += ms; return DRV_ILI9488_PORT_OK;
}
/** @brief 记录命令写周期。 */
static DRV_ILI9488_PortStatus command(void *context, uint8_t value, const DRV_ILI9488_Budget *b)
{ (void)context; return record(MOCK_CMD, value, 0U, b); }
/** @brief 在返回前消费全部数据；绝不保存调用者指针。 */
static DRV_ILI9488_PortStatus data(void *context, const uint16_t *units, size_t count,
                                  const DRV_ILI9488_Budget *b)
{
    size_t i; uint32_t offset = (uint32_t)mock.units_count;
    (void)context;
    if (count > mock.max_batch) { mock.max_batch = count; }
    if (!mock.capture) { return record(MOCK_DATA, 0U, count, b); }
    if ((count == 0U) || (units == NULL) || (count > 16384U - mock.units_count)) {
        mock.overflow = true; return DRV_ILI9488_PORT_IO_ERROR;
    }
    for (i = 0U; i < count; ++i) { mock.units[mock.units_count++] = units[i]; }
    return record(MOCK_DATA, offset, count, b);
}
/** @brief 记录逻辑复位操作。 */
static DRV_ILI9488_PortStatus reset(void *context, bool asserted, const DRV_ILI9488_Budget *b)
{ (void)context; return record(MOCK_RESET, asserted ? 1U : 0U, 0U, b); }
/** @brief 返回完整 port 表，context=NULL 是合法契约。 */
DRV_ILI9488_Port Mock_Port(void)
{
    DRV_ILI9488_Port p = {NULL, ready, now, wait_ms, command, data, reset};
    return p;
}
