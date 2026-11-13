/** @file panel_test.c
 * @brief 合成面板，数值只用于断言协议。禁止加入任何固件源码清单。
 */
#include "drv_ili9488_panel_internal.h"
/** 合成数据无真实模组依据；使用非零图样发现参数漏发/错序。 */
const DRV_ILI9488_Panel test_panel = {
    ILI9488_RESET_SOFT, 1U, 120U, 120U, 120U,
    {1U, 2U},
    {1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U, 12U, 13U, 14U, 15U},
    {15U, 14U, 13U, 12U, 11U, 10U, 9U, 8U, 7U, 6U, 5U, 4U, 3U, 2U, 1U},
    {0x08U, 0x68U, 0xC8U, 0xA8U}, false, NULL
};
