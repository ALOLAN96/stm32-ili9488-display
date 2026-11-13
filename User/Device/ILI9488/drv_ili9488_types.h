/** @file drv_ili9488_types.h
 * @brief ILI9488 公共值类型；不依赖 HAL/RTOS。
 * @layer DRV
 */
#ifndef DRV_ILI9488_TYPES_H
#define DRV_ILI9488_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** 同步 API 结果；OK 只表示发送契约完成，不代表屏幕确认。 */
typedef enum {
    DRV_ILI9488_OK = 0,
    DRV_ILI9488_INVALID_ARGUMENT, /**< 空指针、非法枚举/预算或容量计算溢出。 */
    DRV_ILI9488_OUT_OF_RANGE,     /**< 非空目标矩形越界，不裁剪。 */
    DRV_ILI9488_BUFFER_TOO_SMALL, /**< 源容量不足，以像素计。 */
    DRV_ILI9488_NOT_INITIALIZED,
    DRV_ILI9488_INVALID_STATE,
    DRV_ILI9488_NOT_READY,        /**< 板级软件尚未就绪，未发起副作用。 */
    DRV_ILI9488_BUSY,             /**< 同步重入；不是线程锁。 */
    DRV_ILI9488_TIMEOUT,
    DRV_ILI9488_IO_ERROR,
    DRV_ILI9488_UNSUPPORTED
} DRV_ILI9488_Status;

/** 单实例生命周期。FAULT 下的信息仅是最后提交的软件记录。 */
typedef enum {
    DRV_ILI9488_UNINITIALIZED = 0, DRV_ILI9488_CLOSED,
    DRV_ILI9488_READY, DRV_ILI9488_SLEEPING, DRV_ILI9488_FAULT
} DRV_ILI9488_State;

/** 逻辑方向；具体 MADCTL 映射属于面板描述。 */
typedef enum {
    DRV_ILI9488_ROTATION_0 = 0, DRV_ILI9488_ROTATION_90,
    DRV_ILI9488_ROTATION_180, DRV_ILI9488_ROTATION_270
} DRV_ILI9488_Rotation;

/** 屏端 DBI 协议宽度，不能由 STM32 FSMC 宽度推断。 */
typedef enum { DRV_ILI9488_DBI_8 = 8, DRV_ILI9488_DBI_16 = 16 } DRV_ILI9488_DBIWidth;

/** 坐标与尺寸以像素计；任一尺寸为零表示空矩形。 */
typedef struct { uint16_t x, y, width, height; } DRV_ILI9488_Rect;
typedef struct DRV_ILI9488_Port DRV_ILI9488_Port;
typedef struct DRV_ILI9488_Panel DRV_ILI9488_Panel;

/** 标量复制；panel 不可变且须在整个绑定期有效。 */
typedef struct {
    DRV_ILI9488_DBIWidth dbi_width;
    DRV_ILI9488_Rotation initial_rotation;
    const DRV_ILI9488_Panel *panel;
} DRV_ILI9488_Config;

/** 最近提交的信息；width/height 为旋转后的逻辑像素尺寸。 */
typedef struct {
    DRV_ILI9488_State state;
    DRV_ILI9488_Rotation rotation;
    DRV_ILI9488_DBIWidth dbi_width;
    uint16_t width, height;
} DRV_ILI9488_Info;

/** 诊断中的操作标识，不作为持久化编码。 */
typedef enum {
    DRV_ILI9488_OP_NONE = 0, DRV_ILI9488_OP_OPEN, DRV_ILI9488_OP_ROTATE,
    DRV_ILI9488_OP_FILL, DRV_ILI9488_OP_BLIT, DRV_ILI9488_OP_SLEEP,
    DRV_ILI9488_OP_WAKE, DRV_ILI9488_OP_CLOSE, DRV_ILI9488_OP_RECOVER
} DRV_ILI9488_Operation;

/** 运行故障快照，计数饱和于 UINT32_MAX；成功不清除最近故障。 */
typedef struct {
    DRV_ILI9488_Status last_error;
    DRV_ILI9488_Operation last_failed_operation;
    uint32_t failed_operations, timeout_operations, recovery_attempts;
} DRV_ILI9488_Diagnostics;

#endif
