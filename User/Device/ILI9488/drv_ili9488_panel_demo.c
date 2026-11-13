/** @file drv_ili9488_panel_demo.c
 * @brief 用户旧Demo的ILI9488候选参数；不代表厂商或实板验证结论。
 * @layer DRV
 * @note 来源、哈希、兼容写法与顺序变化见docs/legacy-panel-candidate.md。
 */
#include "drv_ili9488_panels.h"
#include "drv_ili9488_panel_internal.h"

/** 固定长度的具名参数。B1未发送第二字节，C5不补第四字节。 */
static const ILI9488_DemoParameters demo_parameters = {
    .power2 = 0x41U,
    .vcom = {0x00U,0x1AU,0x80U},
    .interface_mode = 0x00U, .frame_rate = 0xA0U, .inversion_control = 0x02U,
    .display_function = {0x00U,0x22U,0x3BU},
    .image_function = 0x00U, .adjust3 = {0xA9U,0x51U,0x2CU,0x82U}
};
/** 方向由原MADCTL=08推导；BGR及安装方向需在实际模组确认。 */
static const DRV_ILI9488_Panel demo_panel = {
    .reset_mode = ILI9488_RESET_SOFT,
    .reset_hold_ms = 0U, .reset_recovery_ms = 120U,
    .sleep_in_ms = 120U, .sleep_out_ms = 120U,
    .power1 = {0x18U,0x17U},
    .gamma_positive = {0x00U,0x13U,0x18U,0x04U,0x0FU,0x06U,0x3AU,0x56U,0x4DU,0x03U,0x0AU,0x06U,0x30U,0x3EU,0x0FU},
    .gamma_negative = {0x00U,0x13U,0x18U,0x01U,0x11U,0x06U,0x38U,0x34U,0x4DU,0x06U,0x0DU,0x0BU,0x31U,0x37U,0x0FU},
    .madctl = {0x08U,0x68U,0xC8U,0xA8U}, .inversion = false,
    .demo = &demo_parameters
};
const DRV_ILI9488_Config DRV_ILI9488_DemoCandidateConfig = {
    DRV_ILI9488_DBI_16, DRV_ILI9488_ROTATION_0, &demo_panel
};
