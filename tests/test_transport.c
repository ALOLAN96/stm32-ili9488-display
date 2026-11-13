/** @file test_transport.c @brief 捕捉命令错序、预算刷新、晚成功和错误状态提交。 */
#include "test_support.h"

/** @brief 建立软件绑定；无真实硬件默认参数。 */
static int bind(void)
{
    DRV_ILI9488_Port p = Mock_Port(); DRV_ILI9488_Config c = Mock_Config(DRV_ILI9488_DBI_16);
    Mock_Reset(); return DRV_ILI9488_InitState(&p, &c) == DRV_ILI9488_OK ? 0 : 1;
}
/** @brief 工作参数必须在 Sleep Out 保护间隔之后发送，Open 幂等。 */
TEST(test_open_sequence)
{
    const uint8_t commands[] = {0x01U, 0x11U, 0xC0U, 0xE0U, 0xE1U, 0x3AU, 0x36U, 0x20U, 0x29U};
    size_t i, n = 0U; DRV_ILI9488_Info info;
    CHECK(bind() == 0); CHECK(DRV_ILI9488_Open(1000U) == DRV_ILI9488_OK);
    for (i = 0U; i < mock.events_count; ++i) {
        if (mock.events[i].kind == MOCK_CMD) {
            CHECK(n < sizeof(commands) && mock.events[i].value == commands[n]);
            if (n == 1U) { CHECK(mock.events[i].time >= 120U); }
            if (n >= 2U) { CHECK(mock.events[i].time >= 240U); }
            ++n;
        }
    }
    CHECK(n == sizeof(commands) && !mock.overflow && !mock.budget_mismatch);
    CHECK(mock.units_count == 34U && mock.units[32] == 0x55U && mock.units[33] == 0x08U);
    CHECK(DRV_ILI9488_GetInfo(&info) == DRV_ILI9488_OK && info.state == DRV_ILI9488_READY);
    Mock_ClearTrace(); CHECK(DRV_ILI9488_Open(1U) == DRV_ILI9488_OK);
    CHECK(mock.calls == 0U && mock.queries == 0U);
    return 0;
}
/** @brief 未就绪或首个副作用前超时不应进入 FAULT。 */
TEST(test_preflight_deadline)
{
    DRV_ILI9488_Info info; DRV_ILI9488_Diagnostics d;
    CHECK(bind() == 0); mock.ready = false;
    CHECK(DRV_ILI9488_Open(1000U) == DRV_ILI9488_NOT_READY);
    CHECK(mock.calls == 0U); mock.ready = true; mock.ready_cost = 10U;
    CHECK(DRV_ILI9488_Open(10U) == DRV_ILI9488_TIMEOUT);
    CHECK(mock.calls == 0U);
    CHECK(DRV_ILI9488_GetInfo(&info) == DRV_ILI9488_OK && info.state == DRV_ILI9488_CLOSED);
    CHECK(DRV_ILI9488_GetDiagnostics(&d) == DRV_ILI9488_OK);
    CHECK(d.failed_operations == 2U && d.timeout_operations == 1U && d.recovery_attempts == 0U);
    CHECK(DRV_ILI9488_Open(0U) == DRV_ILI9488_INVALID_ARGUMENT);
    CHECK(DRV_ILI9488_Open(0x80000000U) == DRV_ILI9488_INVALID_ARGUMENT);
    return 0;
}
/** @brief 已触发副作用后的晚 OK 必须转成 TIMEOUT，且停止后续命令。 */
TEST(test_late_ok)
{
    DRV_ILI9488_Info info;
    CHECK(bind() == 0); mock.io_cost = 32U; mock.now = 0xFFFFFFF0U;
    CHECK(DRV_ILI9488_Open(32U) == DRV_ILI9488_TIMEOUT);
    CHECK(mock.calls == 1U && mock.now == 0x10U);
    CHECK(DRV_ILI9488_GetInfo(&info) == DRV_ILI9488_OK && info.state == DRV_ILI9488_FAULT);
    return 0;
}
/** @brief 模 2^32 时钟回绕不可触发伪超时。 */
TEST(test_clock_wrap)
{
    CHECK(bind() == 0); mock.now = 0xFFFFFFF0U;
    CHECK(DRV_ILI9488_Open(241U) == DRV_ILI9488_OK);
    CHECK(mock.now == 224U && !mock.budget_mismatch);
    return 0;
}
/** @brief 等待未完成不能继续发参数。 */
TEST(test_wait_timeout)
{
    DRV_ILI9488_Info info;
    CHECK(bind() == 0);
    CHECK(DRV_ILI9488_Open(120U) == DRV_ILI9488_TIMEOUT);
    CHECK(mock.calls == 2U);
    CHECK(DRV_ILI9488_GetInfo(&info) == DRV_ILI9488_OK && info.state == DRV_ILI9488_FAULT);
    return 0;
}
/** @brief 违反最短等待的 port OK 属于 I/O 契约失败。 */
TEST(test_wait_early_success)
{
    CHECK(bind() == 0); mock.short_wait = true;
    CHECK(DRV_ILI9488_Open(1000U) == DRV_ILI9488_IO_ERROR);
    CHECK(mock.calls == 2U); return 0;
}
/** @brief 未知 port 状态不可误判成功。 */
TEST(test_unknown_port_error)
{
    DRV_ILI9488_Info info;
    CHECK(bind() == 0); mock.fail_at = 1U; mock.failure = (DRV_ILI9488_PortStatus)99;
    CHECK(DRV_ILI9488_Open(1000U) == DRV_ILI9488_IO_ERROR);
    CHECK(mock.calls == 1U);
    CHECK(DRV_ILI9488_GetInfo(&info) == DRV_ILI9488_OK && info.state == DRV_ILI9488_FAULT);
    return 0;
}
/** @brief 参数传输失败可能已部分发送，不能继续下一条命令。 */
TEST(test_parameter_error)
{
    CHECK(bind() == 0); mock.fail_at = 6U; mock.failure = DRV_ILI9488_PORT_UNSUPPORTED;
    CHECK(DRV_ILI9488_Open(1000U) == DRV_ILI9488_UNSUPPORTED);
    CHECK(mock.calls == 6U && mock.events[5].kind == MOCK_DATA); return 0;
}
/** 本用例的回调检测结果；只有测试钩子写入。 */
static bool reentry_ok;
/** @brief 在 port 中故意重入；输出须保持哨兵值。 */
static void reenter(void)
{
    DRV_ILI9488_Info info = {DRV_ILI9488_SLEEPING, DRV_ILI9488_ROTATION_180, DRV_ILI9488_DBI_8, 9U, 7U};
    DRV_ILI9488_Diagnostics d = {DRV_ILI9488_IO_ERROR, DRV_ILI9488_OP_WAKE, 7U, 8U, 9U};
    reentry_ok = reentry_ok && DRV_ILI9488_GetInfo(&info) == DRV_ILI9488_BUSY && info.width == 9U &&
        DRV_ILI9488_GetDiagnostics(&d) == DRV_ILI9488_BUSY && d.failed_operations == 7U &&
        DRV_ILI9488_InitState(NULL, NULL) == DRV_ILI9488_BUSY && DRV_ILI9488_Open(0U) == DRV_ILI9488_BUSY;
}
/** @brief busy 必须覆盖就绪查询、等待、命令和数据回调。 */
TEST(test_reentry)
{
    CHECK(bind() == 0); reentry_ok = true; mock.hook = reenter;
    CHECK(DRV_ILI9488_Open(1000U) == DRV_ILI9488_OK);
    CHECK(reentry_ok); return 0;
}
/** @brief 硬复位必须成对操作并等待，缺 reset 能力拒绝绑定。 */
TEST(test_hard_reset)
{
    DRV_ILI9488_Panel panel = test_panel;
    DRV_ILI9488_Config c = Mock_Config(DRV_ILI9488_DBI_16); DRV_ILI9488_Port p = Mock_Port();
    Mock_Reset(); panel.reset_mode = ILI9488_RESET_HARD; panel.reset_hold_ms = 5U; c.panel = &panel;
    p.set_reset = NULL; CHECK(DRV_ILI9488_InitState(&p, &c) == DRV_ILI9488_INVALID_ARGUMENT);
    p = Mock_Port(); CHECK(DRV_ILI9488_InitState(&p, &c) == DRV_ILI9488_OK);
    CHECK(DRV_ILI9488_Open(1000U) == DRV_ILI9488_OK);
    CHECK(mock.events[0].kind == MOCK_RESET && mock.events[0].value == 1U);
    CHECK(mock.events[1].kind == MOCK_WAIT && mock.events[1].value == 5U);
    CHECK(mock.events[2].kind == MOCK_RESET && mock.events[2].value == 0U && mock.events[2].time == 5U);
    CHECK(mock.events[3].kind == MOCK_WAIT && mock.events[3].value == 120U);
    return 0;
}

/** @brief InitState 必须复制表/标量，调用者栈变量改变不能重绑驱动。 */
TEST(test_binding_copies_table)
{
    DRV_ILI9488_Port p = Mock_Port(); DRV_ILI9488_Config c = Mock_Config(DRV_ILI9488_DBI_16);
    DRV_ILI9488_Info info;
    Mock_Reset(); CHECK(DRV_ILI9488_InitState(&p, &c) == DRV_ILI9488_OK);
    p.now_ms = NULL; p.write_command8 = NULL;
    c.dbi_width = DRV_ILI9488_DBI_8; c.initial_rotation = DRV_ILI9488_ROTATION_90; c.panel = NULL;
    CHECK(DRV_ILI9488_Open(1000U) == DRV_ILI9488_OK);
    CHECK(DRV_ILI9488_GetInfo(&info) == DRV_ILI9488_OK && info.dbi_width == DRV_ILI9488_DBI_16 && info.width == 320U);
    return 0;
}
