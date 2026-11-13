/** @file drv_ili9488_panels.h
 * @brief 具名面板配置选择；应用无需接触寄存器或私有结构。
 * @layer DRV
 */
#ifndef DRV_ILI9488_PANELS_H
#define DRV_ILI9488_PANELS_H
#include "drv_ili9488_types.h"

/** 旧Demo候选：DBI16/RGB565、软复位、0度；尚未实板验证。
 * 可复制Config修改initial_rotation；不可改变为DBI8。
 * 供电/物理复位/FSMC就绪由宿主保证，不包含背光控制。
 * C5/B1保留旧例程写法，详见docs/legacy-panel-candidate.md。
 */
extern const DRV_ILI9488_Config DRV_ILI9488_DemoCandidateConfig;
#endif
