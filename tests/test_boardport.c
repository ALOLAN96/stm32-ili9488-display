/** @file test_boardport.c @brief 真实薄port对虚拟时钟/原始写周期的契约测试。 */
#include "test_support.h"
#include "bsp_ili9488_fsmc_port.h"
#include "bsp_lcd8080.h"

/** 本测试进程独占的原始总线记录，不替换port逻辑。 */
static uint32_t tick, step, idle_count;
static size_t command_count, data_count;
static uint16_t data_values[256];
static uint8_t last_command;
static bool bus_ready = true;
static size_t lose_ready_after;
/** @brief 替代无法在主机执行的MMIO，保留一个unit一个写周期。 */
void BSP_LCD8080_WriteCommand8(uint8_t command) { ++command_count; last_command = command; }
/** @brief 记录有序数据周期并可模拟板级就绪丢失。 */
void BSP_LCD8080_WriteData16(uint16_t unit)
{
    if (data_count < 256U) { data_values[data_count] = unit; }
    ++data_count;
    if (lose_ready_after != 0U && data_count >= lose_ready_after) { bus_ready = false; }
}
/** @brief 无副作用的宿主就绪信号。 */
static bool is_ready(void *context) { (void)context; return bus_ready; }
/** @brief 可控回绕/每次查询成本。 */
static uint32_t now(void *context) { uint32_t value = tick; (void)context; tick += step; return value; }
/** @brief 模拟调度让出一毫秒。 */
static void idle(void *context) { (void)context; ++tick; ++idle_count; }
/** 持久借用的宿主服务表，测试中不修改。 */
static const BSP_ILI9488_FSMC_Services services = {NULL, is_ready, now, idle};

/** @brief 工厂无I/O、失败不写输出，且不能绑定系统NRST为独立复位。 */
TEST(test_boardport_factory)
{
    DRV_ILI9488_Port p = Mock_Port(); BSP_ILI9488_FSMC_Services bad = services;
    void *sentinel = &bad; p.context = sentinel;
    CHECK(BSP_ILI9488_FSMC_MakePort(NULL, &p) == DRV_ILI9488_INVALID_ARGUMENT && p.context == sentinel);
    bad.now_ms = NULL;
    CHECK(BSP_ILI9488_FSMC_MakePort(&bad, &p) == DRV_ILI9488_INVALID_ARGUMENT && p.context == sentinel);
    CHECK(BSP_ILI9488_FSMC_MakePort(&services, NULL) == DRV_ILI9488_INVALID_ARGUMENT);
    CHECK(BSP_ILI9488_FSMC_MakePort(&services, &p) == DRV_ILI9488_OK);
    CHECK(p.set_reset == NULL && command_count == 0U && data_count == 0U && tick == 0U);
    CHECK(p.is_ready(p.context)); bus_ready = false; CHECK(!p.is_ready(p.context)); return 0;
}
/** @brief 原始RGB565单元保持顺序和数值，不在port再次拆字节。 */
TEST(test_boardport_units)
{
    DRV_ILI9488_Port p; DRV_ILI9488_Budget b = {0U, 10U};
    const uint16_t rgb[] = {0xF800U, 0x07E0U, 0x001FU};
    CHECK(BSP_ILI9488_FSMC_MakePort(&services, &p) == DRV_ILI9488_OK);
    CHECK(p.write_command8(p.context, 0x2CU, &b) == DRV_ILI9488_PORT_OK);
    CHECK(command_count == 1U && last_command == 0x2CU);
    CHECK(p.write_data_units(p.context, rgb, 3U, &b) == DRV_ILI9488_PORT_OK);
    CHECK(data_count == 3U && data_values[0] == 0xF800U && data_values[1] == 0x07E0U && data_values[2] == 0x001FU);
    CHECK(p.write_data_units(p.context, NULL, 3U, &b) == DRV_ILI9488_PORT_IO_ERROR);
    CHECK(p.write_data_units(p.context, rgb, 0U, &b) == DRV_ILI9488_PORT_IO_ERROR);
    CHECK(p.write_data_units(p.context, rgb, 257U, &b) == DRV_ILI9488_PORT_IO_ERROR);
    CHECK(data_count == 3U); return 0;
}
/** @brief 预算到期不得继续下一写周期，预算不在批次内重置。 */
TEST(test_boardport_deadline)
{
    DRV_ILI9488_Port p; DRV_ILI9488_Budget b = {0U, 3U}; const uint16_t values[4] = {1U,2U,3U,4U};
    CHECK(BSP_ILI9488_FSMC_MakePort(&services, &p) == DRV_ILI9488_OK); step = 1U;
    CHECK(p.write_data_units(p.context, values, 4U, &b) == DRV_ILI9488_PORT_TIMEOUT);
    CHECK(data_count == 3U && data_values[2] == 3U);
    CHECK(p.write_command8(p.context, 1U, &b) == DRV_ILI9488_PORT_TIMEOUT && command_count == 0U);
    return 0;
}
/** @brief 就绪中途撤销后停止访问MMIO，报告I/O路径失败。 */
TEST(test_boardport_readiness_loss)
{
    DRV_ILI9488_Port p; DRV_ILI9488_Budget b = {0U, 100U}; const uint16_t values[4] = {1U,2U,3U,4U};
    CHECK(BSP_ILI9488_FSMC_MakePort(&services, &p) == DRV_ILI9488_OK); lose_ready_after = 2U;
    CHECK(p.write_data_units(p.context, values, 4U, &b) == DRV_ILI9488_PORT_IO_ERROR);
    CHECK(data_count == 2U); return 0;
}
/** @brief 等待满足下限和总截止时间，跨uint32回绕仍正确。 */
TEST(test_boardport_wait)
{
    DRV_ILI9488_Port p; DRV_ILI9488_Budget b = {0xFFFFFFF0U, 32U}; tick = 0xFFFFFFF0U;
    CHECK(BSP_ILI9488_FSMC_MakePort(&services, &p) == DRV_ILI9488_OK);
    CHECK(p.wait_ms(p.context, 31U, &b) == DRV_ILI9488_PORT_OK && tick == 15U && idle_count == 31U);
    CHECK(p.wait_ms(p.context, 1U, &b) == DRV_ILI9488_PORT_TIMEOUT && tick == 16U);
    CHECK(command_count == 0U && data_count == 0U); return 0;
}
/** @brief Device通过真实port完成Open/像素写入，验证跨层组合。 */
TEST(test_boardport_device_binding)
{
    DRV_ILI9488_Port p; DRV_ILI9488_Config c = Mock_Config(DRV_ILI9488_DBI_16);
    DRV_ILI9488_Rect r = {0U,0U,1U,1U};
    CHECK(BSP_ILI9488_FSMC_MakePort(&services, &p) == DRV_ILI9488_OK);
    CHECK(DRV_ILI9488_InitState(&p, &c) == DRV_ILI9488_OK);
    CHECK(DRV_ILI9488_Open(1000U) == DRV_ILI9488_OK && tick >= 240U);
    command_count = 0U; data_count = 0U;
    CHECK(DRV_ILI9488_FillRect(r, 0xF800U, 10U) == DRV_ILI9488_OK);
    CHECK(command_count == 3U && data_count == 9U && data_values[8] == 0xF800U); return 0;
}
