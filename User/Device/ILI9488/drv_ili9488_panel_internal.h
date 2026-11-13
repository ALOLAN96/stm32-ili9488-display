/** @file drv_ili9488_panel_internal.h
 * @brief Device 私有面板参数；应用选择具名常量实例，候选配置须另行实板验证。
 * @layer DRV
 * @note 寄存器字段依据 ILItek ILI9488 V100 命令表；长度固定，无通用脚本。
 */
#ifndef DRV_ILI9488_PANEL_INTERNAL_H
#define DRV_ILI9488_PANEL_INTERNAL_H
#include "drv_ili9488_types.h"

/** SOFT 要求系统已完成上电/物理复位前提；不代表能恢复外部总线故障。 */
typedef enum { ILI9488_RESET_SOFT = 0, ILI9488_RESET_HARD } ILI9488_ResetMode;

/** 旧Demo兼容参数；仅支持记录的组合，不作为通用寄存器脚本。
 * C5三字节/B1单字节明确保留原例程写法，长度歧义见候选说明。
 * 所有对象不可变，生命周期覆盖整个绑定期。
 */
typedef struct {
    uint8_t power2;
    uint8_t vcom[3];
    uint8_t interface_mode, frame_rate, inversion_control;
    uint8_t display_function[3];
    uint8_t image_function, adjust3[4];
} ILI9488_DemoParameters;

/** 模组需提供全部参数。等待单位 ms；reset/sleep 切换至少保守等待120ms。
 * power1 对应 C0 VRH1/2；gamma 对应 E0/E1 各15个固定字段。
 * madctl 对应36h：MV 必须符合方向奇偶，BGR/ML/MH 在各方向一致。
 * 这里只实现明确命名的控制项，新增模组所需命令必须经源码审查扩展。
 */
struct DRV_ILI9488_Panel {
    ILI9488_ResetMode reset_mode;
    uint32_t reset_hold_ms, reset_recovery_ms, sleep_in_ms, sleep_out_ms;
    uint8_t power1[2];
    uint8_t gamma_positive[15], gamma_negative[15];
    uint8_t madctl[4];
    bool inversion;
    const ILI9488_DemoParameters *demo; /**< NULL保留基础序列；非NULL仅支持DBI16。 */
};
bool ILI9488_PanelValid(const DRV_ILI9488_Panel *panel);
#endif
