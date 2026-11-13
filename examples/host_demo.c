/** @file host_demo.c
 * @brief 主机生命周期示例；Mock/合成 panel 仅演示协议，不能用于固件。
 * @layer APP
 */
#include "drv_ili9488.h"
#include "drv_ili9488_port.h"
#include "mock_port.h"

/** @brief 应用拥有重绘；先按查询尺寸清屏，再画一条 RGB565 测试图案。 */
static DRV_ILI9488_Status redraw(void)
{
    const uint16_t colors[3] = {0xF800U, 0x07E0U, 0x001FU};
    DRV_ILI9488_Info info; DRV_ILI9488_Rect rect;
    DRV_ILI9488_Status result = DRV_ILI9488_GetInfo(&info);
    if (result != DRV_ILI9488_OK) { return result; }
    rect.x = 0U; rect.y = 0U; rect.width = info.width; rect.height = info.height;
    result = DRV_ILI9488_FillRect(rect, 0U, 1000U);
    if (result != DRV_ILI9488_OK) { return result; }
    rect.width = 3U; rect.height = 1U;
    return DRV_ILI9488_BlitRect(rect, colors, 3U, 3U, 100U);
}

/** @brief 返回0表示模拟流程完成；任何错误都停止，不自动恢复或重试。
 * @note 真正 App 在错误策略中读取诊断、决定是否 Recover，再查询尺寸/重绘。
 * BSP 的初始化/背光控制属于目标工程，不在此主机示例中伪造。
 */
int host_demo(void)
{
    DRV_ILI9488_Port port = Mock_Port();
    DRV_ILI9488_Config config = Mock_Config(DRV_ILI9488_DBI_16);
    DRV_ILI9488_Info info; DRV_ILI9488_Diagnostics diagnostics;
    Mock_Reset(); mock.capture = false;
    if (DRV_ILI9488_InitState(&port, &config) != DRV_ILI9488_OK) { return 1; }
    if (DRV_ILI9488_Open(1000U) != DRV_ILI9488_OK) { return 2; }
    if (redraw() != DRV_ILI9488_OK) { return 3; }
    if (DRV_ILI9488_SetRotation(DRV_ILI9488_ROTATION_90, 100U) != DRV_ILI9488_OK) { return 4; }
    if (redraw() != DRV_ILI9488_OK) { return 5; }
    if (DRV_ILI9488_Sleep(1000U) != DRV_ILI9488_OK) { return 6; }
    if (DRV_ILI9488_Wake(1000U) != DRV_ILI9488_OK) { return 7; }
    if (redraw() != DRV_ILI9488_OK) { return 8; }
    if (DRV_ILI9488_Close(1000U) != DRV_ILI9488_OK) { return 9; }
    if (DRV_ILI9488_GetInfo(&info) != DRV_ILI9488_OK || info.state != DRV_ILI9488_CLOSED) { return 10; }
    if (DRV_ILI9488_GetDiagnostics(&diagnostics) != DRV_ILI9488_OK || diagnostics.failed_operations != 0U) { return 11; }
    return 0;
}
