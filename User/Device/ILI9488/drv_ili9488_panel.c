/** @file drv_ili9488_panel.c
 * @brief ILI9488 私有参数验证；具名寄存器，不包含板级默认初始化表。
 * @layer DRV
 * @note 来源：ILItek ILI9488 V100，36h/C0h/E0h/E1h 命令定义。
 */
#include "drv_ili9488_internal.h"

/** @brief 扩展仅接受旧Demo记录的组合，避免将兼容写法泛化到其它面板。
 * 未接入更多模组前不开放任意电源/接口参数；需要扩展时连同证据和测试修改。
 */
static bool demo_valid(const ILI9488_DemoParameters *p)
{
    return p == NULL || (p->power2 == 0x41U &&
        p->vcom[0] == 0U && p->vcom[1] == 0x1AU && p->vcom[2] == 0x80U &&
        p->interface_mode == 0U && p->frame_rate == 0xA0U && p->inversion_control == 2U &&
        p->display_function[0] == 0U && p->display_function[1] == 0x22U && p->display_function[2] == 0x3BU &&
        p->image_function == 0U && p->adjust3[0] == 0xA9U && p->adjust3[1] == 0x51U &&
        p->adjust3[2] == 0x2CU && p->adjust3[3] == 0x82U);
}

/** @brief 校验固定长度、保留位、方向尺寸关系与有界等待。
 * @retval true 表示描述结构合法，不表示电气参数适用于实际模组。
 */
bool ILI9488_PanelValid(const DRV_ILI9488_Panel *p)
{
    /* E0/E1 各字段有效位；保留位不得传给屏幕。 */
    static const uint8_t gamma_mask[15] = {
        0x0FU, 0x3FU, 0x3FU, 0x0FU, 0x1FU, 0x0FU, 0x7FU, 0xFFU,
        0x7FU, 0x0FU, 0x1FU, 0x0FU, 0x3FU, 0x3FU, 0x0FU
    };
    size_t i;
    if (p == NULL || !demo_valid(p->demo)) { return false; }
    if (p->reset_mode != ILI9488_RESET_SOFT && p->reset_mode != ILI9488_RESET_HARD) { return false; }
    if (p->reset_hold_ms > 0x7FFFFFFFU || p->reset_recovery_ms > 0x7FFFFFFFU ||
        p->sleep_in_ms > 0x7FFFFFFFU || p->sleep_out_ms > 0x7FFFFFFFU) { return false; }
    if (p->reset_mode == ILI9488_RESET_HARD && p->reset_hold_ms == 0U) { return false; }
    if ((p->power1[0] & 0xE0U) != 0U || (p->power1[1] & 0xE0U) != 0U) { return false; }
    for (i = 0U; i < 15U; ++i) {
        if ((p->gamma_positive[i] & (uint8_t)~gamma_mask[i]) != 0U ||
            (p->gamma_negative[i] & (uint8_t)~gamma_mask[i]) != 0U) { return false; }
    }
    for (i = 0U; i < 4U; ++i) {
        uint8_t mv = (i % 2U) != 0U ? 0x20U : 0U;
        if ((p->madctl[i] & 0x03U) != 0U || (p->madctl[i] & 0x20U) != mv ||
            (p->madctl[i] & 0x1CU) != (p->madctl[0] & 0x1CU)) { return false; }
    }
    /* 90度需交换轴且恰翻转一轴；只设置MV会成为转置镜像。
     * 接受模组约定的顺/逆时针关系，零度安装方向仍由描述定义。 */
    if ((uint8_t)(p->madctl[0] ^ p->madctl[1]) != 0x60U &&
        (uint8_t)(p->madctl[0] ^ p->madctl[1]) != 0xA0U) { return false; }
    /* 180度必须翻转两轴；90/270 同理。 */
    return (uint8_t)(p->madctl[0] ^ p->madctl[2]) == 0xC0U &&
           (uint8_t)(p->madctl[1] ^ p->madctl[3]) == 0xC0U;
}

/** @brief 固定命令及参数事务，首个失败立即返回。 */
static DRV_ILI9488_Status parameter(uint8_t command, const uint8_t *values, size_t count)
{
    DRV_ILI9488_Status result = ILI9488_Command(command, 0U);
    return result == DRV_ILI9488_OK ? ILI9488_Bytes(values, count) : result;
}

/** @brief 发出候选扩展；共用事务预算，任何失败立即停止。
 * 这里只产生固定命令，不提供应用可注入的命令脚本。
 */
static DRV_ILI9488_Status demo_parameters(const ILI9488_DemoParameters *p)
{
    DRV_ILI9488_Status r;
    if (p == NULL) { return DRV_ILI9488_OK; }
    r = parameter(0xC1U, &p->power2, 1U);
    if (r == DRV_ILI9488_OK) { r = parameter(0xC5U, p->vcom, 3U); }
    if (r == DRV_ILI9488_OK) { r = parameter(0xB0U, &p->interface_mode, 1U); }
    if (r == DRV_ILI9488_OK) { r = parameter(0xB1U, &p->frame_rate, 1U); }
    if (r == DRV_ILI9488_OK) { r = parameter(0xB4U, &p->inversion_control, 1U); }
    if (r == DRV_ILI9488_OK) { r = parameter(0xB6U, p->display_function, 3U); }
    if (r == DRV_ILI9488_OK) { r = parameter(0xE9U, &p->image_function, 1U); }
    if (r == DRV_ILI9488_OK) { r = parameter(0xF7U, p->adjust3, 4U); }
    return r;
}

/** @brief Sleep Out 完成后重建固定工作参数，避免被默认寄存器加载覆盖。 */
static DRV_ILI9488_Status working_parameters(const DRV_ILI9488_Panel *p, DRV_ILI9488_Rotation rotation)
{
    const uint8_t format = 0x55U; /* V100 3Ah：DBI RGB565，DPI 同时为16bpp。 */
    DRV_ILI9488_Status r = parameter(0xC0U, p->power1, 2U);
    if (r == DRV_ILI9488_OK) { r = parameter(0xE0U, p->gamma_positive, 15U); }
    if (r == DRV_ILI9488_OK) { r = parameter(0xE1U, p->gamma_negative, 15U); }
    if (r == DRV_ILI9488_OK) { r = demo_parameters(p->demo); }
    if (r == DRV_ILI9488_OK) { r = parameter(0x3AU, &format, 1U); }
    if (r == DRV_ILI9488_OK) { r = parameter(0x36U, &p->madctl[(size_t)rotation], 1U); }
    if (r == DRV_ILI9488_OK) { r = ILI9488_Command(p->inversion ? 0x21U : 0x20U, 0U); }
    return r;
}

/** @brief 固定复位/退出睡眠/工作参数/开显示顺序；参数由具名模组描述提供，候选仍须实测。 */
DRV_ILI9488_Status ILI9488_PanelOpen(const DRV_ILI9488_Panel *p, DRV_ILI9488_Rotation rotation)
{
    DRV_ILI9488_Status r;
    if (p->reset_mode == ILI9488_RESET_HARD) {
        r = ILI9488_Reset(true, 0U);
        if (r == DRV_ILI9488_OK) { r = ILI9488_Wait(p->reset_hold_ms); }
        if (r == DRV_ILI9488_OK) { r = ILI9488_Reset(false, p->reset_recovery_ms < 120U ? 120U : p->reset_recovery_ms); }
    } else {
        r = ILI9488_Command(0x01U, p->reset_recovery_ms < 120U ? 120U : p->reset_recovery_ms);
    }
    if (r == DRV_ILI9488_OK) { r = ILI9488_PanelWake(p, rotation); }
    return r;
}

/** @brief 退出睡眠并重建最近提交配置；GRAM 内容不受保证。 */
DRV_ILI9488_Status ILI9488_PanelWake(const DRV_ILI9488_Panel *p, DRV_ILI9488_Rotation rotation)
{
    DRV_ILI9488_Status r = ILI9488_Command(0x11U, p->sleep_out_ms < 120U ? 120U : p->sleep_out_ms);
    if (r == DRV_ILI9488_OK) { r = ILI9488_WaitGuard(); }
    if (r == DRV_ILI9488_OK) { r = working_parameters(p, rotation); }
    if (r == DRV_ILI9488_OK) { r = ILI9488_Command(0x29U, 0U); }
    return r;
}

/** @brief 关显示并进入睡眠，结束前完成保护等待；不控制背光或总线时钟。 */
DRV_ILI9488_Status ILI9488_PanelStop(const DRV_ILI9488_Panel *p)
{
    DRV_ILI9488_Status r = ILI9488_Command(0x28U, 0U);
    if (r == DRV_ILI9488_OK) { r = ILI9488_Command(0x10U, p->sleep_in_ms < 120U ? 120U : p->sleep_in_ms); }
    if (r == DRV_ILI9488_OK) { r = ILI9488_WaitGuard(); }
    return r;
}
