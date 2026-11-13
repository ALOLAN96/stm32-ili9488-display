/** @file test_lifecycle.c @brief 状态矩阵、跨调用等待与故障注入。 */
#include "test_support.h"
#include "drv_ili9488_internal.h"

/** @brief 建立 READY 测试状态。 */
static int ready(void)
{
    DRV_ILI9488_Port p = Mock_Port(); DRV_ILI9488_Config c = Mock_Config(DRV_ILI9488_DBI_16);
    Mock_Reset();
    if (DRV_ILI9488_InitState(&p, &c) != DRV_ILI9488_OK || DRV_ILI9488_Open(1000U) != DRV_ILI9488_OK) { return 1; }
    Mock_ClearTrace(); return 0;
}
/** @brief 未初始化的全部操作均拒绝，非法预算优先。 */
TEST(test_uninitialized_operations)
{
    DRV_ILI9488_Rect empty = {0U, 0U, 0U, 0U};
    CHECK(DRV_ILI9488_Open(10U) == DRV_ILI9488_NOT_INITIALIZED);
    CHECK(DRV_ILI9488_Close(10U) == DRV_ILI9488_NOT_INITIALIZED);
    CHECK(DRV_ILI9488_Sleep(10U) == DRV_ILI9488_NOT_INITIALIZED);
    CHECK(DRV_ILI9488_Wake(10U) == DRV_ILI9488_NOT_INITIALIZED);
    CHECK(DRV_ILI9488_Recover(10U) == DRV_ILI9488_NOT_INITIALIZED);
    CHECK(DRV_ILI9488_SetRotation(DRV_ILI9488_ROTATION_0, 10U) == DRV_ILI9488_NOT_INITIALIZED);
    CHECK(DRV_ILI9488_FillRect(empty, 0U, 10U) == DRV_ILI9488_NOT_INITIALIZED);
    CHECK(DRV_ILI9488_BlitRect(empty, NULL, 0U, 0U, 10U) == DRV_ILI9488_NOT_INITIALIZED);
    CHECK(DRV_ILI9488_Wake(0U) == DRV_ILI9488_INVALID_ARGUMENT); return 0;
}
/** @brief 稳定状态中允许、幂等和拒绝行为都不混用。 */
TEST(test_state_matrix)
{
    DRV_ILI9488_Rect empty = {0U, 0U, 0U, 0U};
    DRV_ILI9488_Port p = Mock_Port(); DRV_ILI9488_Config c = Mock_Config(DRV_ILI9488_DBI_16);
    DRV_ILI9488_Info info;
    Mock_Reset(); CHECK(DRV_ILI9488_InitState(&p, &c) == DRV_ILI9488_OK);
    CHECK(DRV_ILI9488_Close(10U) == DRV_ILI9488_OK);
    CHECK(DRV_ILI9488_Sleep(10U) == DRV_ILI9488_INVALID_STATE);
    CHECK(DRV_ILI9488_Wake(10U) == DRV_ILI9488_INVALID_STATE);
    CHECK(DRV_ILI9488_Recover(10U) == DRV_ILI9488_INVALID_STATE);
    CHECK(DRV_ILI9488_FillRect(empty, 0U, 10U) == DRV_ILI9488_INVALID_STATE);
    CHECK(DRV_ILI9488_BlitRect(empty, NULL, 0U, 0U, 10U) == DRV_ILI9488_INVALID_STATE);
    CHECK(DRV_ILI9488_SetRotation(DRV_ILI9488_ROTATION_0, 10U) == DRV_ILI9488_INVALID_STATE);
    CHECK(mock.calls == 0U && mock.queries == 0U);
    CHECK(DRV_ILI9488_Open(1000U) == DRV_ILI9488_OK); Mock_ClearTrace();
    CHECK(DRV_ILI9488_Wake(10U) == DRV_ILI9488_OK);
    CHECK(DRV_ILI9488_InitState(&p, &c) == DRV_ILI9488_INVALID_STATE);
    CHECK(DRV_ILI9488_Recover(10U) == DRV_ILI9488_INVALID_STATE);
    CHECK(mock.calls == 0U && mock.queries == 0U);
    CHECK(DRV_ILI9488_Sleep(1000U) == DRV_ILI9488_OK); Mock_ClearTrace();
    CHECK(DRV_ILI9488_GetInfo(&info) == DRV_ILI9488_OK && info.state == DRV_ILI9488_SLEEPING);
    CHECK(DRV_ILI9488_Sleep(10U) == DRV_ILI9488_OK);
    CHECK(DRV_ILI9488_Open(10U) == DRV_ILI9488_INVALID_STATE);
    CHECK(DRV_ILI9488_FillRect(empty, 0U, 10U) == DRV_ILI9488_INVALID_STATE);
    CHECK(DRV_ILI9488_BlitRect(empty, NULL, 0U, 0U, 10U) == DRV_ILI9488_INVALID_STATE);
    CHECK(DRV_ILI9488_SetRotation(DRV_ILI9488_ROTATION_90, 10U) == DRV_ILI9488_INVALID_STATE);
    CHECK(DRV_ILI9488_Recover(10U) == DRV_ILI9488_INVALID_STATE);
    CHECK(DRV_ILI9488_InitState(&p, &c) == DRV_ILI9488_INVALID_STATE);
    CHECK(mock.calls == 0U && mock.queries == 0U);
    CHECK(DRV_ILI9488_Close(1000U) == DRV_ILI9488_OK);
    CHECK(DRV_ILI9488_GetInfo(&info) == DRV_ILI9488_OK && info.state == DRV_ILI9488_CLOSED);
    return 0;
}
/** @brief 方向失败后 Recover/Wake/Open 均只能恢复最近成功方向。 */
TEST(test_restore_committed_rotation)
{
    DRV_ILI9488_Info info; DRV_ILI9488_Diagnostics d; DRV_ILI9488_Rect empty = {0U, 0U, 0U, 0U};
    DRV_ILI9488_Port p = Mock_Port(); DRV_ILI9488_Config c = Mock_Config(DRV_ILI9488_DBI_16);
    CHECK(ready() == 0);
    CHECK(DRV_ILI9488_SetRotation(DRV_ILI9488_ROTATION_90, 100U) == DRV_ILI9488_OK);
    Mock_ClearTrace(); mock.fail_at = 2U;
    CHECK(DRV_ILI9488_SetRotation(DRV_ILI9488_ROTATION_180, 100U) == DRV_ILI9488_IO_ERROR);
    CHECK(DRV_ILI9488_GetInfo(&info) == DRV_ILI9488_OK && info.rotation == DRV_ILI9488_ROTATION_90 && info.width == 480U);
    Mock_ClearTrace();
    CHECK(DRV_ILI9488_Open(10U) == DRV_ILI9488_INVALID_STATE);
    CHECK(DRV_ILI9488_Sleep(10U) == DRV_ILI9488_INVALID_STATE);
    CHECK(DRV_ILI9488_Wake(10U) == DRV_ILI9488_INVALID_STATE);
    CHECK(DRV_ILI9488_FillRect(empty, 0U, 10U) == DRV_ILI9488_INVALID_STATE);
    CHECK(DRV_ILI9488_BlitRect(empty, NULL, 0U, 0U, 10U) == DRV_ILI9488_INVALID_STATE);
    CHECK(DRV_ILI9488_SetRotation(DRV_ILI9488_ROTATION_90, 10U) == DRV_ILI9488_INVALID_STATE);
    CHECK(DRV_ILI9488_InitState(&p, &c) == DRV_ILI9488_INVALID_STATE);
    CHECK(mock.calls == 0U && mock.queries == 0U);
    mock.fail_at = 0U; CHECK(DRV_ILI9488_Recover(1000U) == DRV_ILI9488_OK);
    CHECK(mock.units[33] == 0x68U);
    CHECK(DRV_ILI9488_GetDiagnostics(&d) == DRV_ILI9488_OK && d.recovery_attempts == 1U && d.failed_operations == 1U);
    CHECK(d.last_error == DRV_ILI9488_IO_ERROR && d.last_failed_operation == DRV_ILI9488_OP_ROTATE);
    CHECK(DRV_ILI9488_Sleep(1000U) == DRV_ILI9488_OK); Mock_ClearTrace();
    CHECK(DRV_ILI9488_Wake(1000U) == DRV_ILI9488_OK && mock.units[33] == 0x68U);
    CHECK(mock.events[0].kind == MOCK_CMD && mock.events[0].value == 0x11U);
    CHECK(mock.events[1].kind == MOCK_WAIT && mock.events[1].value >= 120U);
    CHECK(DRV_ILI9488_Close(1000U) == DRV_ILI9488_OK); Mock_ClearTrace();
    CHECK(DRV_ILI9488_Open(1000U) == DRV_ILI9488_OK && mock.units[33] == 0x68U);
    CHECK(DRV_ILI9488_GetInfo(&info) == DRV_ILI9488_OK && info.rotation == DRV_ILI9488_ROTATION_90 && info.height == 320U);
    return 0;
}
/** @brief 跨调用超时不能绕过 Sleep Out 保护等待，未开始恢复不增加尝试计数。 */
TEST(test_guard_survives_timeout)
{
    DRV_ILI9488_Port p = Mock_Port(); DRV_ILI9488_Config c = Mock_Config(DRV_ILI9488_DBI_16);
    DRV_ILI9488_Diagnostics d; DRV_ILI9488_Info info;
    Mock_Reset(); CHECK(DRV_ILI9488_InitState(&p, &c) == DRV_ILI9488_OK);
    CHECK(DRV_ILI9488_Open(200U) == DRV_ILI9488_TIMEOUT); CHECK(mock.now == 200U);
    Mock_ClearTrace(); CHECK(DRV_ILI9488_Recover(39U) == DRV_ILI9488_TIMEOUT);
    CHECK(mock.calls == 1U && mock.events[0].kind == MOCK_WAIT);
    CHECK(DRV_ILI9488_GetInfo(&info) == DRV_ILI9488_OK && info.state == DRV_ILI9488_FAULT);
    CHECK(DRV_ILI9488_GetDiagnostics(&d) == DRV_ILI9488_OK && d.recovery_attempts == 0U && d.timeout_operations == 2U);
    Mock_ClearTrace(); CHECK(DRV_ILI9488_Recover(1000U) == DRV_ILI9488_OK);
    CHECK(mock.events[0].kind == MOCK_WAIT && mock.events[0].value == 1U);
    CHECK(mock.events[1].kind == MOCK_CMD && mock.events[1].time == 240U);
    CHECK(DRV_ILI9488_GetDiagnostics(&d) == DRV_ILI9488_OK && d.recovery_attempts == 1U);
    return 0;
}
/** @brief reset/sleep 回调报错也可能生效，因此恢复前仍须等待。 */
TEST(test_guard_survives_callback_error)
{
    DRV_ILI9488_Port p = Mock_Port(); DRV_ILI9488_Config c = Mock_Config(DRV_ILI9488_DBI_16);
    DRV_ILI9488_Diagnostics d;
    Mock_Reset(); CHECK(DRV_ILI9488_InitState(&p, &c) == DRV_ILI9488_OK);
    mock.fail_at = 1U; mock.io_cost = 7U;
    CHECK(DRV_ILI9488_Open(1000U) == DRV_ILI9488_IO_ERROR);
    Mock_ClearTrace(); mock.fail_at = 0U; mock.io_cost = 0U;
    mock.ready = false; CHECK(DRV_ILI9488_Recover(1000U) == DRV_ILI9488_NOT_READY);
    CHECK(DRV_ILI9488_GetDiagnostics(&d) == DRV_ILI9488_OK && d.recovery_attempts == 0U);
    mock.ready = true; CHECK(DRV_ILI9488_Recover(1000U) == DRV_ILI9488_OK);
    CHECK(mock.events[0].kind == MOCK_WAIT && mock.events[0].value == 120U);
    CHECK(mock.events[1].time == 127U);
    Mock_ClearTrace(); mock.fail_at = 2U;
    CHECK(DRV_ILI9488_Sleep(1000U) == DRV_ILI9488_IO_ERROR);
    Mock_ClearTrace(); mock.fail_at = 0U;
    CHECK(DRV_ILI9488_Close(1000U) == DRV_ILI9488_OK);
    CHECK(mock.events[0].kind == MOCK_WAIT && mock.events[0].value == 120U);
    return 0;
}
/** @brief 更短面板描述不能缩短120ms保护；硬复位释放报错也保留保护。 */
TEST(test_hard_reset_guard)
{
    DRV_ILI9488_Panel panel = test_panel;
    DRV_ILI9488_Port p = Mock_Port(); DRV_ILI9488_Config c = Mock_Config(DRV_ILI9488_DBI_16);
    Mock_Reset(); panel.reset_mode = ILI9488_RESET_HARD; panel.reset_hold_ms = 1U;
    panel.reset_recovery_ms = 0U; panel.sleep_in_ms = 0U; panel.sleep_out_ms = 0U; c.panel = &panel;
    CHECK(DRV_ILI9488_InitState(&p, &c) == DRV_ILI9488_OK); mock.fail_at = 3U;
    CHECK(DRV_ILI9488_Open(1000U) == DRV_ILI9488_IO_ERROR);
    Mock_ClearTrace(); mock.fail_at = 0U; CHECK(DRV_ILI9488_Recover(1000U) == DRV_ILI9488_OK);
    CHECK(mock.events[0].kind == MOCK_WAIT && mock.events[0].value == 120U);
    CHECK(mock.events[1].kind == MOCK_RESET && mock.events[1].time == 121U);
    CHECK(mock.now == 362U); return 0;
}
/** @brief 每一个 Open 回调失败均停止序列、记录一次，并可显式 Close。
 * 注入循环经过真实生命周期重建，不调用产品测试后门。
 */
TEST(test_every_open_failure)
{
    size_t count, failure;
    DRV_ILI9488_Port p = Mock_Port(); DRV_ILI9488_Config c = Mock_Config(DRV_ILI9488_DBI_16);
    DRV_ILI9488_Info info; DRV_ILI9488_Diagnostics d;
    CHECK(ready() == 0); CHECK(DRV_ILI9488_Close(1000U) == DRV_ILI9488_OK);
    Mock_ClearTrace(); CHECK(DRV_ILI9488_Open(1000U) == DRV_ILI9488_OK); count = mock.calls;
    CHECK(DRV_ILI9488_Close(1000U) == DRV_ILI9488_OK);
    for (failure = 1U; failure <= count; ++failure) {
        Mock_Reset(); CHECK(DRV_ILI9488_InitState(&p, &c) == DRV_ILI9488_OK); mock.fail_at = failure;
        CHECK(DRV_ILI9488_Open(1000U) == DRV_ILI9488_IO_ERROR); CHECK(mock.calls == failure);
        CHECK(DRV_ILI9488_GetInfo(&info) == DRV_ILI9488_OK && info.state == DRV_ILI9488_FAULT);
        CHECK(DRV_ILI9488_GetDiagnostics(&d) == DRV_ILI9488_OK && d.failed_operations == 1U);
        mock.fail_at = 0U; Mock_ClearTrace(); CHECK(DRV_ILI9488_Close(1000U) == DRV_ILI9488_OK);
    }
    return 0;
}

/** @brief 硬复位有效/保持/释放三个阶段失败均不能隐式补偿或继续发命令。 */
TEST(test_every_hard_reset_failure)
{
    DRV_ILI9488_Panel panel = test_panel;
    DRV_ILI9488_Port p = Mock_Port(); DRV_ILI9488_Config c = Mock_Config(DRV_ILI9488_DBI_16);
    DRV_ILI9488_Info info; size_t failure;
    panel.reset_mode = ILI9488_RESET_HARD; panel.reset_hold_ms = 5U; c.panel = &panel;
    for (failure = 1U; failure <= 4U; ++failure) {
        Mock_Reset(); CHECK(DRV_ILI9488_InitState(&p, &c) == DRV_ILI9488_OK); mock.fail_at = failure;
        CHECK(DRV_ILI9488_Open(1000U) == DRV_ILI9488_IO_ERROR && mock.calls == failure);
        CHECK(DRV_ILI9488_GetInfo(&info) == DRV_ILI9488_OK && info.state == DRV_ILI9488_FAULT);
        mock.fail_at = 0U; Mock_ClearTrace();
        CHECK(DRV_ILI9488_Recover(1000U) == DRV_ILI9488_OK);
        CHECK(DRV_ILI9488_Close(1000U) == DRV_ILI9488_OK);
    }
    return 0;
}

/** @brief 每个 Wake/停止回调失败后必须停止；不偷偷续传图像或下一条命令。 */
TEST(test_every_wake_and_stop_failure)
{
    size_t count, failure; DRV_ILI9488_Info info;
    CHECK(ready() == 0); CHECK(DRV_ILI9488_Sleep(1000U) == DRV_ILI9488_OK);
    Mock_ClearTrace(); CHECK(DRV_ILI9488_Wake(1000U) == DRV_ILI9488_OK); count = mock.calls;
    for (failure = 1U; failure <= count; ++failure) {
        CHECK(DRV_ILI9488_Sleep(1000U) == DRV_ILI9488_OK);
        Mock_ClearTrace(); mock.fail_at = failure;
        CHECK(DRV_ILI9488_Wake(1000U) == DRV_ILI9488_IO_ERROR && mock.calls == failure);
        CHECK(DRV_ILI9488_GetInfo(&info) == DRV_ILI9488_OK && info.state == DRV_ILI9488_FAULT);
        mock.fail_at = 0U; Mock_ClearTrace(); CHECK(DRV_ILI9488_Recover(1000U) == DRV_ILI9488_OK);
    }
    for (failure = 1U; failure <= 3U; ++failure) {
        Mock_ClearTrace(); mock.fail_at = failure;
        CHECK(DRV_ILI9488_Close(1000U) == DRV_ILI9488_IO_ERROR && mock.calls == failure);
        CHECK(DRV_ILI9488_GetInfo(&info) == DRV_ILI9488_OK && info.state == DRV_ILI9488_FAULT);
        mock.fail_at = 0U; Mock_ClearTrace(); CHECK(DRV_ILI9488_Recover(1000U) == DRV_ILI9488_OK);
    }
    return 0;
}
/** @brief 停止失败进入 FAULT，恢复首个回调失败仍计一次尝试；重绑清零诊断。 */
TEST(test_diagnostics_and_stop_failure)
{
    DRV_ILI9488_Info info; DRV_ILI9488_Diagnostics d;
    DRV_ILI9488_Port p = Mock_Port(); DRV_ILI9488_Config c = Mock_Config(DRV_ILI9488_DBI_16);
    uint32_t counter = UINT32_MAX - 1U;
    CHECK(ready() == 0); mock.fail_at = 1U;
    CHECK(DRV_ILI9488_Close(1000U) == DRV_ILI9488_IO_ERROR);
    CHECK(DRV_ILI9488_GetInfo(&info) == DRV_ILI9488_OK && info.state == DRV_ILI9488_FAULT);
    Mock_ClearTrace(); CHECK(DRV_ILI9488_Recover(1000U) == DRV_ILI9488_IO_ERROR);
    CHECK(DRV_ILI9488_GetDiagnostics(&d) == DRV_ILI9488_OK && d.recovery_attempts == 1U && d.failed_operations == 2U);
    CHECK(d.last_failed_operation == DRV_ILI9488_OP_RECOVER);
    mock.fail_at = 0U; CHECK(DRV_ILI9488_Close(1000U) == DRV_ILI9488_OK);
    CHECK(DRV_ILI9488_InitState(&p, &c) == DRV_ILI9488_OK);
    CHECK(DRV_ILI9488_GetDiagnostics(&d) == DRV_ILI9488_OK && d.failed_operations == 0U && d.last_error == DRV_ILI9488_OK);
    ILI9488_CountFailure(&counter); CHECK(counter == UINT32_MAX);
    ILI9488_CountFailure(&counter); CHECK(counter == UINT32_MAX); return 0;
}
/** 测试钩子写入，记录所有公开 API 的重入结果。 */
static bool reentry_ok;
/** @brief 重入必须先于参数/状态判断，且不能改变诊断。 */
static void reenter_all(void)
{
    DRV_ILI9488_Rect r = {0U, 0U, 0U, 0U};
    reentry_ok = reentry_ok && DRV_ILI9488_Sleep(0U) == DRV_ILI9488_BUSY &&
        DRV_ILI9488_Wake(0U) == DRV_ILI9488_BUSY && DRV_ILI9488_Close(0U) == DRV_ILI9488_BUSY &&
        DRV_ILI9488_Recover(0U) == DRV_ILI9488_BUSY &&
        DRV_ILI9488_SetRotation((DRV_ILI9488_Rotation)-1, 0U) == DRV_ILI9488_BUSY &&
        DRV_ILI9488_FillRect(r, 0U, 0U) == DRV_ILI9488_BUSY &&
        DRV_ILI9488_BlitRect(r, NULL, 0U, 0U, 0U) == DRV_ILI9488_BUSY;
}
/** @brief 生命周期回调期间所有公开入口都受 busy 防护。 */
TEST(test_all_api_reentry)
{
    DRV_ILI9488_Diagnostics d; CHECK(ready() == 0); reentry_ok = true; mock.hook = reenter_all;
    CHECK(DRV_ILI9488_Sleep(1000U) == DRV_ILI9488_OK); CHECK(reentry_ok);
    CHECK(DRV_ILI9488_GetDiagnostics(&d) == DRV_ILI9488_OK && d.failed_operations == 0U); return 0;
}
