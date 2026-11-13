/** @file test_pixels.c @brief 验证窗口、RGB565 数值打包、容量边界和方向提交。 */
#include "test_support.h"
#include "drv_ili9488_internal.h"

/** @brief 在 READY 开始一个独立测试事务。 */
static int start(DRV_ILI9488_DBIWidth width)
{
    DRV_ILI9488_Port p = Mock_Port(); DRV_ILI9488_Config c = Mock_Config(width);
    Mock_Reset();
    if (DRV_ILI9488_InitState(&p, &c) != DRV_ILI9488_OK || DRV_ILI9488_Open(1000U) != DRV_ILI9488_OK) { return 1; }
    Mock_ClearTrace(); return 0;
}
/** @brief 每个矩形只发一个窗口，坐标参数在16位DBI也不能合并字节。 */
TEST(test_window_and_fill)
{
    DRV_ILI9488_Rect rect = {1U, 2U, 3U, 2U};
    const uint16_t expected[] = {0U, 1U, 0U, 3U, 0U, 2U, 0U, 3U, 0x1234U, 0x1234U, 0x1234U, 0x1234U, 0x1234U, 0x1234U};
    const uint8_t commands[] = {0x2AU, 0x2BU, 0x2CU}; size_t i, n = 0U;
    CHECK(start(DRV_ILI9488_DBI_16) == 0);
    CHECK(DRV_ILI9488_FillRect(rect, 0x1234U, 100U) == DRV_ILI9488_OK);
    CHECK(mock.units_count == 14U);
    for (i = 0U; i < 14U; ++i) { CHECK(mock.units[i] == expected[i]); }
    for (i = 0U; i < mock.events_count; ++i) {
        if (mock.events[i].kind == MOCK_CMD) { CHECK(n < 3U && mock.events[i].value == commands[n]); ++n; }
    }
    CHECK(n == 3U && mock.max_batch <= CONFIG_ILI9488_TX_UNITS && !mock.budget_mismatch);
    return 0;
}
/** @brief 8位协议高字节先行，不受宿主内存小端布局影响。 */
TEST(test_rgb565_dbi8)
{
    uint16_t pixels[] = {0xF800U, 0x07E0U, 0x001FU};
    const uint16_t expected[] = {0xF8U, 0U, 7U, 0xE0U, 0U, 0x1FU};
    DRV_ILI9488_Rect rect = {0U, 0U, 3U, 1U}; size_t i;
    CHECK(start(DRV_ILI9488_DBI_8) == 0);
    CHECK(DRV_ILI9488_BlitRect(rect, pixels, 3U, 3U, 100U) == DRV_ILI9488_OK);
    CHECK(mock.units_count == 14U);
    for (i = 0U; i < 6U; ++i) { CHECK(mock.units[8U + i] == expected[i]); }
    Mock_ClearTrace(); CHECK(DRV_ILI9488_FillRect(rect, 0xF800U, 100U) == DRV_ILI9488_OK);
    for (i = 8U; i < 14U; i += 2U) { CHECK(mock.units[i] == 0xF8U && mock.units[i + 1U] == 0U); }
    return 0;
}
/** @brief 16位协议每个 RGB565 数值是一个写周期。 */
TEST(test_rgb565_dbi16)
{
    uint16_t pixels[] = {0xF800U, 0x07E0U, 0x001FU};
    DRV_ILI9488_Rect rect = {0U, 0U, 3U, 1U}; DRV_ILI9488_Info info;
    CHECK(start(DRV_ILI9488_DBI_16) == 0);
    CHECK(DRV_ILI9488_BlitRect(rect, pixels, 3U, 3U, 100U) == DRV_ILI9488_OK);
    CHECK(mock.units_count == 11U && mock.units[8] == 0xF800U && mock.units[9] == 0x07E0U && mock.units[10] == 0x001FU);
    pixels[0] = 0U; pixels[1] = 0U; pixels[2] = 0U;
    Mock_ClearTrace(); CHECK(DRV_ILI9488_GetInfo(&info) == DRV_ILI9488_OK);
    CHECK(mock.calls == 0U && mock.queries == 0U); return 0;
}
/** @brief 跳过行padding；短容量和SIZE_MAX溢出必须在I/O之前拒绝。 */
TEST(test_stride_and_capacity)
{
    const uint16_t pixels[] = {0xAU, 0xBU, 0xDEADU, 0xCU, 0xDU};
    DRV_ILI9488_Rect rect = {0U, 0U, 2U, 2U};
    CHECK(start(DRV_ILI9488_DBI_16) == 0);
    CHECK(DRV_ILI9488_BlitRect(rect, pixels, 4U, 3U, 100U) == DRV_ILI9488_BUFFER_TOO_SMALL);
    CHECK(DRV_ILI9488_BlitRect(rect, pixels, SIZE_MAX, SIZE_MAX, 100U) == DRV_ILI9488_INVALID_ARGUMENT);
    CHECK(DRV_ILI9488_BlitRect(rect, NULL, 5U, 3U, 100U) == DRV_ILI9488_INVALID_ARGUMENT);
    CHECK(DRV_ILI9488_BlitRect(rect, pixels, 5U, 1U, 100U) == DRV_ILI9488_INVALID_ARGUMENT);
    CHECK(mock.calls == 0U && mock.queries == 0U);
    CHECK(DRV_ILI9488_BlitRect(rect, pixels, 5U, 3U, 100U) == DRV_ILI9488_OK);
    CHECK(mock.units_count == 12U && mock.units[8] == 0xAU && mock.units[9] == 0xBU && mock.units[10] == 0xCU && mock.units[11] == 0xDU);
    return 0;
}
/** @brief 空矩形忽略坐标/源/步幅，非空越界必须拒绝且不裁剪。 */
TEST(test_empty_and_bounds)
{
    DRV_ILI9488_Rect rect = {UINT16_MAX, UINT16_MAX, 0U, UINT16_MAX};
    CHECK(start(DRV_ILI9488_DBI_16) == 0);
    CHECK(DRV_ILI9488_BlitRect(rect, NULL, 0U, 0U, 1U) == DRV_ILI9488_OK);
    CHECK(DRV_ILI9488_FillRect(rect, 0U, 1U) == DRV_ILI9488_OK);
    CHECK(DRV_ILI9488_FillRect(rect, 0U, 0U) == DRV_ILI9488_INVALID_ARGUMENT);
    rect.x = 319U; rect.y = 479U; rect.width = 2U; rect.height = 1U;
    CHECK(DRV_ILI9488_FillRect(rect, 0U, 10U) == DRV_ILI9488_OUT_OF_RANGE);
    rect.x = UINT16_MAX; rect.width = 1U;
    CHECK(DRV_ILI9488_FillRect(rect, 0U, 10U) == DRV_ILI9488_OUT_OF_RANGE);
    CHECK(mock.calls == 0U && mock.queries == 0U);
    rect.x = 319U; CHECK(DRV_ILI9488_FillRect(rect, 1U, 10U) == DRV_ILI9488_OK);
    return 0;
}
/** @brief 四个方向维度正确；相同方向不写，失败保留最近提交记录。 */
TEST(test_rotation_commit)
{
    DRV_ILI9488_Info info;
    CHECK(start(DRV_ILI9488_DBI_16) == 0);
    CHECK(DRV_ILI9488_SetRotation(DRV_ILI9488_ROTATION_0, 1U) == DRV_ILI9488_OK);
    CHECK(DRV_ILI9488_SetRotation((DRV_ILI9488_Rotation)4, 1U) == DRV_ILI9488_INVALID_ARGUMENT);
    CHECK(mock.calls == 0U && mock.queries == 0U);
    CHECK(DRV_ILI9488_SetRotation(DRV_ILI9488_ROTATION_90, 100U) == DRV_ILI9488_OK);
    CHECK(mock.units_count == 1U && mock.units[0] == 0x68U);
    CHECK(DRV_ILI9488_GetInfo(&info) == DRV_ILI9488_OK && info.width == 480U && info.height == 320U);
    CHECK(DRV_ILI9488_SetRotation(DRV_ILI9488_ROTATION_270, 100U) == DRV_ILI9488_OK);
    CHECK(DRV_ILI9488_GetInfo(&info) == DRV_ILI9488_OK && info.width == 480U && info.height == 320U);
    CHECK(DRV_ILI9488_SetRotation(DRV_ILI9488_ROTATION_180, 100U) == DRV_ILI9488_OK);
    CHECK(DRV_ILI9488_GetInfo(&info) == DRV_ILI9488_OK && info.width == 320U && info.height == 480U);
    Mock_ClearTrace(); mock.fail_at = 2U;
    CHECK(DRV_ILI9488_SetRotation(DRV_ILI9488_ROTATION_90, 100U) == DRV_ILI9488_IO_ERROR);
    CHECK(DRV_ILI9488_GetInfo(&info) == DRV_ILI9488_OK && info.rotation == DRV_ILI9488_ROTATION_180 && info.state == DRV_ILI9488_FAULT);
    return 0;
}
/** @brief 多批次传输不能重置总预算，也不能丢失末尾不满批次像素。 */
TEST(test_batching)
{
    uint16_t pixels[257]; size_t i; DRV_ILI9488_Rect rect = {0U, 0U, 257U, 1U};
    CHECK(start(DRV_ILI9488_DBI_8) == 0);
    for (i = 0U; i < 257U; ++i) { pixels[i] = (uint16_t)i; }
    CHECK(DRV_ILI9488_BlitRect(rect, pixels, 257U, 257U, 1000U) == DRV_ILI9488_OK);
    CHECK(mock.units_count == 522U && mock.max_batch <= CONFIG_ILI9488_TX_UNITS && !mock.budget_mismatch);
    for (i = 0U; i < 257U; ++i) {
        CHECK(mock.units[8U + i * 2U] == (uint16_t)(i >> 8U));
        CHECK(mock.units[9U + i * 2U] == (uint16_t)(i & 0xFFU));
    }
    Mock_ClearTrace(); mock.io_cost = 1U;
    CHECK(DRV_ILI9488_FillRect(rect, 0U, 5U) == DRV_ILI9488_TIMEOUT);
    CHECK(mock.calls == 5U && !mock.budget_mismatch);
    return 0;
}
