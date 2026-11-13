/** @file bsp_lcd8080.h @brief 麒麟F407 LCD连接器的原始8080写周期。 @layer BSP
 * @note 调用者先完成GPIO/FSMC初始化、就绪检查及预算检查；仅唯一任务上下文。
 * 本接口不初始化总线，不解释ILI9488命令，不支持读回、DMA、复位或背光。
 */
#ifndef BSP_LCD8080_H
#define BSP_LCD8080_H
#include <stdint.h>

/* NE4对应NOR/PSRAM第4区；16位总线的外部A6对应CPU字节地址bit7。 */
#define BSP_LCD8080_COMMAND_ADDRESS ((uintptr_t)0x6C000000UL)
#define BSP_LCD8080_DATA_ADDRESS    ((uintptr_t)0x6C000080UL)

void BSP_LCD8080_WriteCommand8(uint8_t command);
void BSP_LCD8080_WriteData16(uint16_t unit);
#endif
