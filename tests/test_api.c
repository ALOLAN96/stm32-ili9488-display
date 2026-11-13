/** @file test_api.c @brief 捕捉非法绑定、查询覆盖输出及意外硬件访问。 */
#include "test_support.h"

/** @brief 初始化前的错误优先级和失败查询不得写输出。 */
TEST(test_uninitialized_queries)
{
    DRV_ILI9488_Info info = {DRV_ILI9488_FAULT, DRV_ILI9488_ROTATION_270, DRV_ILI9488_DBI_8, 9U, 7U};
    DRV_ILI9488_Diagnostics d = {DRV_ILI9488_IO_ERROR, DRV_ILI9488_OP_WAKE, 7U, 8U, 9U};
    CHECK(DRV_ILI9488_GetInfo(&info) == DRV_ILI9488_NOT_INITIALIZED);
    CHECK(info.state == DRV_ILI9488_FAULT && info.width == 9U && info.height == 7U);
    CHECK(DRV_ILI9488_GetDiagnostics(&d) == DRV_ILI9488_NOT_INITIALIZED);
    CHECK(d.failed_operations == 7U && d.recovery_attempts == 9U);
    CHECK(DRV_ILI9488_GetInfo(NULL) == DRV_ILI9488_INVALID_ARGUMENT);
    CHECK(DRV_ILI9488_GetDiagnostics(NULL) == DRV_ILI9488_INVALID_ARGUMENT);
    return 0;
}
/** @brief 配置提交只能更新软件，旋转影响逻辑尺寸。 */
TEST(test_init_commit)
{
    DRV_ILI9488_Port p = Mock_Port(); DRV_ILI9488_Config c = Mock_Config(DRV_ILI9488_DBI_16);
    DRV_ILI9488_Info info; DRV_ILI9488_Diagnostics d;
    Mock_Reset();
    CHECK(DRV_ILI9488_InitState(&p, &c) == DRV_ILI9488_OK);
    CHECK(DRV_ILI9488_GetInfo(&info) == DRV_ILI9488_OK);
    CHECK(info.state == DRV_ILI9488_CLOSED && info.width == 320U && info.height == 480U);
    CHECK(DRV_ILI9488_GetDiagnostics(&d) == DRV_ILI9488_OK);
    CHECK(d.last_error == DRV_ILI9488_OK && d.failed_operations == 0U && d.recovery_attempts == 0U);
    c.initial_rotation = DRV_ILI9488_ROTATION_90;
    CHECK(DRV_ILI9488_InitState(&p, &c) == DRV_ILI9488_OK);
    CHECK(DRV_ILI9488_GetInfo(&info) == DRV_ILI9488_OK);
    CHECK(info.width == 480U && info.height == 320U && info.rotation == DRV_ILI9488_ROTATION_90);
    CHECK(mock.calls == 0U && mock.queries == 0U);
    return 0;
}
/** @brief 错误配置不可破坏已提交的绑定；必需回调逐个拒绝。 */
TEST(test_config_rejection)
{
    DRV_ILI9488_Port p = Mock_Port(), bad;
    DRV_ILI9488_Config c = Mock_Config(DRV_ILI9488_DBI_16), b;
    DRV_ILI9488_Info info;
    Mock_Reset(); CHECK(DRV_ILI9488_InitState(&p, &c) == DRV_ILI9488_OK);
    CHECK(DRV_ILI9488_InitState(NULL, &c) == DRV_ILI9488_INVALID_ARGUMENT);
    CHECK(DRV_ILI9488_InitState(&p, NULL) == DRV_ILI9488_INVALID_ARGUMENT);
    bad = p; bad.now_ms = NULL; CHECK(DRV_ILI9488_InitState(&bad, &c) == DRV_ILI9488_INVALID_ARGUMENT);
    bad = p; bad.is_ready = NULL; CHECK(DRV_ILI9488_InitState(&bad, &c) == DRV_ILI9488_INVALID_ARGUMENT);
    bad = p; bad.wait_ms = NULL; CHECK(DRV_ILI9488_InitState(&bad, &c) == DRV_ILI9488_INVALID_ARGUMENT);
    bad = p; bad.write_command8 = NULL; CHECK(DRV_ILI9488_InitState(&bad, &c) == DRV_ILI9488_INVALID_ARGUMENT);
    bad = p; bad.write_data_units = NULL; CHECK(DRV_ILI9488_InitState(&bad, &c) == DRV_ILI9488_INVALID_ARGUMENT);
    b = c; b.dbi_width = (DRV_ILI9488_DBIWidth)9; CHECK(DRV_ILI9488_InitState(&p, &b) == DRV_ILI9488_INVALID_ARGUMENT);
    b = c; b.initial_rotation = (DRV_ILI9488_Rotation)-1; CHECK(DRV_ILI9488_InitState(&p, &b) == DRV_ILI9488_INVALID_ARGUMENT);
    b = c; b.panel = NULL; CHECK(DRV_ILI9488_InitState(&p, &b) == DRV_ILI9488_INVALID_ARGUMENT);
    CHECK(DRV_ILI9488_GetInfo(&info) == DRV_ILI9488_OK);
    CHECK(info.dbi_width == DRV_ILI9488_DBI_16 && info.rotation == DRV_ILI9488_ROTATION_0);
    CHECK(mock.calls == 0U && mock.queries == 0U);
    return 0;
}

/** @brief 非法保留位、方向映射及复位等待拒绝，旧配置仍可查询。 */
TEST(test_panel_validation)
{
    DRV_ILI9488_Panel panel = test_panel;
    DRV_ILI9488_Port p = Mock_Port(); DRV_ILI9488_Config c = Mock_Config(DRV_ILI9488_DBI_16);
    Mock_Reset(); c.panel = &panel;
    panel.gamma_positive[0] = 0x80U;
    CHECK(DRV_ILI9488_InitState(&p, &c) == DRV_ILI9488_INVALID_ARGUMENT);
    panel = test_panel; panel.power1[0] = 0x20U;
    CHECK(DRV_ILI9488_InitState(&p, &c) == DRV_ILI9488_INVALID_ARGUMENT);
    panel = test_panel; panel.madctl[0] = 0x01U;
    CHECK(DRV_ILI9488_InitState(&p, &c) == DRV_ILI9488_INVALID_ARGUMENT);
    panel = test_panel; panel.madctl[1] = 0x08U;
    CHECK(DRV_ILI9488_InitState(&p, &c) == DRV_ILI9488_INVALID_ARGUMENT);
    panel = test_panel; panel.madctl[2] = panel.madctl[0];
    CHECK(DRV_ILI9488_InitState(&p, &c) == DRV_ILI9488_INVALID_ARGUMENT);
    panel = test_panel; panel.sleep_out_ms = 0x80000000U;
    CHECK(DRV_ILI9488_InitState(&p, &c) == DRV_ILI9488_INVALID_ARGUMENT);
    panel = test_panel; panel.reset_mode = (ILI9488_ResetMode)99;
    CHECK(DRV_ILI9488_InitState(&p, &c) == DRV_ILI9488_INVALID_ARGUMENT);
    panel = test_panel; panel.reset_mode = ILI9488_RESET_HARD; panel.reset_hold_ms = 0U;
    CHECK(DRV_ILI9488_InitState(&p, &c) == DRV_ILI9488_INVALID_ARGUMENT);
    CHECK(mock.calls == 0U && mock.queries == 0U); return 0;
}

/** @brief MV单独交换行列是镜像转置，不可接受为90度旋转。 */
TEST(test_reject_transpose_profile)
{
    DRV_ILI9488_Panel panel = test_panel;
    DRV_ILI9488_Port p = Mock_Port(); DRV_ILI9488_Config c = Mock_Config(DRV_ILI9488_DBI_16);
    Mock_Reset(); c.panel = &panel;
    panel.madctl[0] = 0x08U; panel.madctl[1] = 0x28U;
    panel.madctl[2] = 0xC8U; panel.madctl[3] = 0xE8U;
    CHECK(DRV_ILI9488_InitState(&p, &c) == DRV_ILI9488_INVALID_ARGUMENT);
    CHECK(mock.calls == 0U && mock.queries == 0U); return 0;
}
