/** @file bsp_lcd8080_puzhong_f407.c
 * @brief 普中麒麟F407、NE4/A6/16位FSMC的原始写总线。 @layer BSP
 * @note 依据提供的两张原理图按排针脚号配对；仅适用本文档对应硬件。
 * 必须先由宿主唯一初始化路径配置GPIO/FSMC，预算/就绪由port负责。
 * 不能用于主机进程；不访问系统RESET或LCD_BL。
 */
#include "bsp_lcd8080.h"

#if !defined(__ARM_ARCH_7EM__) || (!defined(__GNUC__) && !defined(__clang__))
#error "This MMIO backend requires Cortex-M4/M7 ARMv7E-M GCC/Clang; select the matching F407 target"
#endif

/** @brief 将低8位命令写到A6=0地址；DSB使后续计时位于总线写完成之后。
 * @param command 命令数值，BSP不解析其含义。
 * @retval None
 * @pre 仅已配置的F407 FSMC，单上下文；无总线错误捕获或ACK检测。
 */
void BSP_LCD8080_WriteCommand8(uint8_t command)
{
    *(volatile uint16_t *)BSP_LCD8080_COMMAND_ADDRESS = command;
    __asm__ volatile ("dsb sy" ::: "memory");
}

/** @brief 向A6=1地址完成一次16位写周期，不拆分RGB565或合并参数字节。
 * @param unit 原始总线值。
 * @retval None
 * @pre 与WriteCommand8相同；MMIO/DSB不能由软件超时抢占。
 */
void BSP_LCD8080_WriteData16(uint16_t unit)
{
    *(volatile uint16_t *)BSP_LCD8080_DATA_ADDRESS = unit;
    __asm__ volatile ("dsb sy" ::: "memory");
}
