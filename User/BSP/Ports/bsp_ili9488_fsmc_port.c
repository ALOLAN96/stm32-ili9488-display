/** @file bsp_ili9488_fsmc_port.c
 * @brief 同步预算/时基适配；不拥有Device状态，不初始化GPIO/FSMC。
 * @layer BSP/port
 */
#include "bsp_ili9488_fsmc_port.h"
#include "bsp_lcd8080.h"

/** @brief 拒绝无效预算，避免空指针和失去有界性的直接回调调用。 */
static bool budget_valid(const DRV_ILI9488_Budget *budget)
{ return budget != NULL && budget->timeout_ms != 0U && budget->timeout_ms <= 0x7FFFFFFFU; }

/** @brief 不刷新started_ms，按模2^32时基检查总预算。 */
static bool expired(const BSP_ILI9488_FSMC_Services *s, const DRV_ILI9488_Budget *b)
{ return (uint32_t)(s->now_ms(s->context) - b->started_ms) >= b->timeout_ms; }

/** @brief 先查运行就绪，再查预算；两者均无总线副作用。 */
static DRV_ILI9488_PortStatus before_write(const BSP_ILI9488_FSMC_Services *s,
                                         const DRV_ILI9488_Budget *b)
{
    if (!budget_valid(b)) { return DRV_ILI9488_PORT_IO_ERROR; }
    if (!s->is_ready(s->context)) { return DRV_ILI9488_PORT_IO_ERROR; }
    return expired(s, b) ? DRV_ILI9488_PORT_TIMEOUT : DRV_ILI9488_PORT_OK;
}

/** @brief Device预检的软件就绪能力；不能当作屏幕ACK。 */
static bool port_ready(void *context)
{
    const BSP_ILI9488_FSMC_Services *s = context;
    return s->is_ready(s->context);
}

/** @brief 转发唯一单调毫秒时基。 */
static uint32_t port_now(void *context)
{
    const BSP_ILI9488_FSMC_Services *s = context;
    return s->now_ms(s->context);
}

/** @brief 同步等满minimum_ms，始终消费传入总预算；idle只用于有界让出CPU。
 * @note 时基必须持续运行；软件不能修复停止的tick或打断卡住的宿主回调。
 */
static DRV_ILI9488_PortStatus port_wait(void *context, uint32_t minimum_ms,
                                      const DRV_ILI9488_Budget *b)
{
    const BSP_ILI9488_FSMC_Services *s = context;
    uint32_t start, now;
    if (!budget_valid(b) || minimum_ms > 0x7FFFFFFFU) { return DRV_ILI9488_PORT_IO_ERROR; }
    start = s->now_ms(s->context);
    for (;;) {
        if (!s->is_ready(s->context)) { return DRV_ILI9488_PORT_IO_ERROR; }
        now = s->now_ms(s->context);
        if ((uint32_t)(now - b->started_ms) >= b->timeout_ms) { return DRV_ILI9488_PORT_TIMEOUT; }
        if ((uint32_t)(now - start) >= minimum_ms) { return DRV_ILI9488_PORT_OK; }
        if (s->idle != NULL) { s->idle(s->context); }
    }
}

/** @brief 单个命令地址写周期，完成后再检查是否晚于截止时间。 */
static DRV_ILI9488_PortStatus port_command(void *context, uint8_t command,
                                         const DRV_ILI9488_Budget *b)
{
    const BSP_ILI9488_FSMC_Services *s = context;
    DRV_ILI9488_PortStatus result = before_write(s, b);
    if (result != DRV_ILI9488_PORT_OK) { return result; }
    BSP_LCD8080_WriteCommand8(command);
    return expired(s, b) ? DRV_ILI9488_PORT_TIMEOUT : DRV_ILI9488_PORT_OK;
}

/** @brief 一个unit一个16位总线写周期；逐周期查预算，不打包像素、不保存源区。
 * @note Device契约上限256周期；原始MMIO异常不能映射成可恢复HAL错误。
 */
static DRV_ILI9488_PortStatus port_data(void *context, const uint16_t *units, size_t count,
                                      const DRV_ILI9488_Budget *b)
{
    const BSP_ILI9488_FSMC_Services *s = context;
    size_t i;
    if (units == NULL || count == 0U || count > 256U) { return DRV_ILI9488_PORT_IO_ERROR; }
    for (i = 0U; i < count; ++i) {
        DRV_ILI9488_PortStatus result = before_write(s, b);
        if (result != DRV_ILI9488_PORT_OK) { return result; }
        BSP_LCD8080_WriteData16(units[i]);
    }
    return expired(s, b) ? DRV_ILI9488_PORT_TIMEOUT : DRV_ILI9488_PORT_OK;
}

/** @brief 无I/O地建立板级port表，失败不改输出。
 * @param services 持久、不可变的服务表及长期有效context；两者均借用。
 * @param port 调用者可写输出；随后可由Device的InitState复制。
 * @retval DRV_ILI9488 status
 * @pre 唯一主循环/任务上下文、非ISR；实际模组须配置DBI_16及软复位profile。
 * @note 不提供set_reset：排针RESET与MCU NRST相连，不能当独立复位使用。
 */
DRV_ILI9488_Status BSP_ILI9488_FSMC_MakePort(const BSP_ILI9488_FSMC_Services *services,
                                         DRV_ILI9488_Port *port)
{
    DRV_ILI9488_Port result;
    if (services == NULL || port == NULL || services->is_ready == NULL || services->now_ms == NULL) {
        return DRV_ILI9488_INVALID_ARGUMENT;
    }
    /* Port的通用context为void*；回调立即还原const类型，绝不改写服务表。 */
    result.context = (void *)services;
    result.is_ready = port_ready; result.now_ms = port_now; result.wait_ms = port_wait;
    result.write_command8 = port_command; result.write_data_units = port_data;
    result.set_reset = NULL;
    *port = result;
    return DRV_ILI9488_OK;
}
