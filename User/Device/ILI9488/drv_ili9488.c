/** @file drv_ili9488.c
 * @brief ILI9488 单实例生命周期、预算与 RGB565 矩形传输。
 * @layer DRV
 * @note 唯一主循环/任务上下文拥有全部状态；busy 仅检测同步重入。
 */
#include "drv_ili9488.h"
#include "drv_ili9488_port.h"
#include "drv_ili9488_internal.h"

/** Device 私有状态；port 表/标量复制，context/panel 保持借用。 */
typedef struct {
    bool busy;
    DRV_ILI9488_Port port;
    DRV_ILI9488_Config config;
    DRV_ILI9488_Info info;
    DRV_ILI9488_Diagnostics diagnostics;
    DRV_ILI9488_Budget budget; /**< 当前公开操作唯一预算，单位 ms。 */
    DRV_ILI9488_Operation operation;
    bool started, guard_active; /**< 首个副作用标记与跨调用切换保护。 */
    uint32_t guard_start_ms, guard_duration_ms;
    uint16_t scratch[CONFIG_ILI9488_TX_UNITS]; /**< 有界同步打包区，非帧缓冲。 */
} ILI9488_Device;
/** 唯一生命周期拥有者，静态零初始化对应 UNINITIALIZED。 */
static ILI9488_Device device;

/** @brief 排除负值与越界方向，避免用非法值索引面板。 */
static bool rotation_valid(DRV_ILI9488_Rotation rotation)
{ return rotation >= DRV_ILI9488_ROTATION_0 && rotation <= DRV_ILI9488_ROTATION_270; }

/** @brief 全部方向写成功后才提交软件方向和逻辑像素尺寸。 */
static void commit_rotation(DRV_ILI9488_Rotation rotation)
{
    bool swap = rotation == DRV_ILI9488_ROTATION_90 || rotation == DRV_ILI9488_ROTATION_270;
    device.info.rotation = rotation;
    device.info.width = swap ? 480U : 320U; device.info.height = swap ? 320U : 480U;
}

/** @brief 全量校验后建立软件绑定，无任何 port 回调或硬件副作用。
 * @param port 同步能力表，复制表；context 对象须保持有效。
 * @param config 标量复制；不可变 panel 借用至下次成功绑定。
 * @retval DRV_ILI9488 status
 * @pre 单上下文、非 ISR；仅 UNINITIALIZED/CLOSED 可调用。
 * @note 失败不改变旧绑定或诊断；成功清零诊断。
 */
DRV_ILI9488_Status DRV_ILI9488_InitState(const DRV_ILI9488_Port *port,
                                      const DRV_ILI9488_Config *config)
{
    if (device.busy) { return DRV_ILI9488_BUSY; }
    if (port == NULL || config == NULL) { return DRV_ILI9488_INVALID_ARGUMENT; }
    if (port->is_ready == NULL || port->now_ms == NULL || port->wait_ms == NULL ||
        port->write_command8 == NULL || port->write_data_units == NULL ||
        !rotation_valid(config->initial_rotation) ||
        (config->dbi_width != DRV_ILI9488_DBI_8 && config->dbi_width != DRV_ILI9488_DBI_16) ||
        !ILI9488_PanelValid(config->panel)) { return DRV_ILI9488_INVALID_ARGUMENT; }
    if (config->panel->demo != NULL && config->dbi_width != DRV_ILI9488_DBI_16) {
        return DRV_ILI9488_INVALID_ARGUMENT;
    }
    if (config->panel->reset_mode == ILI9488_RESET_HARD && port->set_reset == NULL) {
        return DRV_ILI9488_INVALID_ARGUMENT;
    }
    if (device.info.state != DRV_ILI9488_UNINITIALIZED && device.info.state != DRV_ILI9488_CLOSED) {
        return DRV_ILI9488_INVALID_STATE;
    }
    device.port = *port; device.config = *config;
    device.info.state = DRV_ILI9488_CLOSED; device.info.dbi_width = config->dbi_width;
    commit_rotation(config->initial_rotation);
    device.diagnostics.last_error = DRV_ILI9488_OK;
    device.diagnostics.last_failed_operation = DRV_ILI9488_OP_NONE;
    device.diagnostics.failed_operations = 0U; device.diagnostics.timeout_operations = 0U;
    device.diagnostics.recovery_attempts = 0U;
    device.guard_active = false;
    return DRV_ILI9488_OK;
}

/** @brief 复制最近提交的状态和尺寸，失败不改输出。
 * @param info 调用者可写结果；FAULT 时仅代表软件记录。
 * @retval DRV_ILI9488 status
 * @pre 单上下文、非 ISR。不能在 port 回调中调用。
 */
DRV_ILI9488_Status DRV_ILI9488_GetInfo(DRV_ILI9488_Info *info)
{
    if (device.busy) { return DRV_ILI9488_BUSY; }
    if (info == NULL) { return DRV_ILI9488_INVALID_ARGUMENT; }
    if (device.info.state == DRV_ILI9488_UNINITIALIZED) { return DRV_ILI9488_NOT_INITIALIZED; }
    *info = device.info; return DRV_ILI9488_OK;
}

/** @brief 复制故障快照，不清除记录；失败不改输出。
 * @param diagnostics 调用者可写结果。
 * @retval DRV_ILI9488 status
 * @pre 单上下文、非 ISR。计数仅通过成功 InitState 清零。
 */
DRV_ILI9488_Status DRV_ILI9488_GetDiagnostics(DRV_ILI9488_Diagnostics *diagnostics)
{
    if (device.busy) { return DRV_ILI9488_BUSY; }
    if (diagnostics == NULL) { return DRV_ILI9488_INVALID_ARGUMENT; }
    if (device.info.state == DRV_ILI9488_UNINITIALIZED) { return DRV_ILI9488_NOT_INITIALIZED; }
    *diagnostics = device.diagnostics; return DRV_ILI9488_OK;
}

/** @brief 回绕安全的预算检查；时基必须持续运行且不跨完整 uint32_t 周期。 */
static bool expired(void)
{
    return (uint32_t)(device.port.now_ms(device.port.context) - device.budget.started_ms) >= device.budget.timeout_ms;
}

/** @brief 回调返回后再检查预算，晚返回 OK 不可被当作成功。 */
static DRV_ILI9488_Status port_result(DRV_ILI9488_PortStatus result)
{
    if (expired()) { return DRV_ILI9488_TIMEOUT; }
    switch (result) {
    case DRV_ILI9488_PORT_OK: return DRV_ILI9488_OK;
    case DRV_ILI9488_PORT_TIMEOUT: return DRV_ILI9488_TIMEOUT;
    case DRV_ILI9488_PORT_UNSUPPORTED: return DRV_ILI9488_UNSUPPORTED;
    default: return DRV_ILI9488_IO_ERROR;
    }
}

/** @brief 等待是有界同步操作；提前 OK 视为适配器违反契约。
 * @retval DRV_ILI9488 status
 */
DRV_ILI9488_Status ILI9488_Wait(uint32_t minimum_ms)
{
    uint32_t start; DRV_ILI9488_Status result;
    if (expired()) { return DRV_ILI9488_TIMEOUT; }
    if (minimum_ms == 0U) { return DRV_ILI9488_OK; }
    start = device.port.now_ms(device.port.context);
    result = port_result(device.port.wait_ms(device.port.context, minimum_ms, &device.budget));
    if (result == DRV_ILI9488_OK &&
        (uint32_t)(device.port.now_ms(device.port.context) - start) < minimum_ms) {
        return DRV_ILI9488_IO_ERROR;
    }
    return result;
}

/** @brief 补足上次可能生效的切换等待；失败保留记录，禁止提前新副作用。 */
DRV_ILI9488_Status ILI9488_WaitGuard(void)
{
    uint32_t elapsed; DRV_ILI9488_Status result;
    if (expired()) { return DRV_ILI9488_TIMEOUT; }
    if (!device.guard_active) { return DRV_ILI9488_OK; }
    elapsed = (uint32_t)(device.port.now_ms(device.port.context) - device.guard_start_ms);
    if (elapsed < device.guard_duration_ms) {
        result = ILI9488_Wait(device.guard_duration_ms - elapsed);
        if (result != DRV_ILI9488_OK) { return result; }
    }
    device.guard_active = false;
    return DRV_ILI9488_OK;
}

/** @brief 无论回调是否报错都记录可能生效的切换，取回调结束后的时间。 */
static void set_guard(uint32_t duration_ms)
{
    if (duration_ms != 0U) {
        device.guard_start_ms = device.port.now_ms(device.port.context);
        device.guard_duration_ms = duration_ms < 120U ? 120U : duration_ms;
        device.guard_active = true;
    }
}

/** @brief 在首个副作用之前计一次恢复尝试，失败也不可假定无副作用。 */
static DRV_ILI9488_Status before_effect(void)
{
    DRV_ILI9488_Status result = ILI9488_WaitGuard();
    if (result != DRV_ILI9488_OK) { return result; }
    if (expired()) { return DRV_ILI9488_TIMEOUT; }
    if (!device.started) {
        device.started = true;
        if (device.operation == DRV_ILI9488_OP_RECOVER) { ILI9488_CountFailure(&device.diagnostics.recovery_attempts); }
    }
    return DRV_ILI9488_OK;
}

/** @brief 发一个命令；guard_ms 非零表示 reset/sleep 类切换。 */
DRV_ILI9488_Status ILI9488_Command(uint8_t command, uint32_t guard_ms)
{
    DRV_ILI9488_PortStatus status; DRV_ILI9488_Status result = before_effect();
    if (result != DRV_ILI9488_OK) { return result; }
    status = device.port.write_command8(device.port.context, command, &device.budget);
    set_guard(guard_ms);
    return port_result(status);
}

/** @brief 同步发送一个有界非空数据批次，不保存源指针。 */
static DRV_ILI9488_Status write_units(const uint16_t *units, size_t count)
{
    DRV_ILI9488_Status result = before_effect();
    if (result != DRV_ILI9488_OK) { return result; }
    return port_result(device.port.write_data_units(device.port.context, units, count, &device.budget));
}

/** @brief 参数始终逐字节扩展为低8位有效单元，不能按像素合并。 */
DRV_ILI9488_Status ILI9488_Bytes(const uint8_t *bytes, size_t count)
{
    while (count != 0U) {
        size_t i, batch = count < CONFIG_ILI9488_TX_UNITS ? count : CONFIG_ILI9488_TX_UNITS;
        DRV_ILI9488_Status result;
        for (i = 0U; i < batch; ++i) { device.scratch[i] = bytes[i]; }
        result = write_units(device.scratch, batch);
        if (result != DRV_ILI9488_OK) { return result; }
        bytes += batch; count -= batch;
    }
    return DRV_ILI9488_OK;
}

/** @brief 逻辑复位线操作；失败不隐含释放或重试。 */
DRV_ILI9488_Status ILI9488_Reset(bool asserted, uint32_t guard_ms)
{
    DRV_ILI9488_PortStatus status; DRV_ILI9488_Status result = before_effect();
    if (result != DRV_ILI9488_OK) { return result; }
    status = device.port.set_reset(device.port.context, asserted, &device.budget);
    set_guard(guard_ms);
    return port_result(status);
}

/** @brief 公开 API 的共同前置检查：重入、预算、初始化依次判断。 */
static DRV_ILI9488_Status validate_operation(uint32_t timeout_ms)
{
    if (device.busy) { return DRV_ILI9488_BUSY; }
    if (timeout_ms == 0U || timeout_ms > 0x7FFFFFFFU) { return DRV_ILI9488_INVALID_ARGUMENT; }
    if (device.info.state == DRV_ILI9488_UNINITIALIZED) { return DRV_ILI9488_NOT_INITIALIZED; }
    return DRV_ILI9488_OK;
}

/** @brief 建立唯一预算，覆盖软件就绪检查与遗留等待；调用者必须 finish。 */
static DRV_ILI9488_Status begin_operation(DRV_ILI9488_Operation operation, uint32_t timeout_ms)
{
    bool ready;
    device.busy = true; device.started = false; device.operation = operation;
    device.budget.timeout_ms = timeout_ms;
    device.budget.started_ms = device.port.now_ms(device.port.context);
    ready = device.port.is_ready(device.port.context);
    if (expired()) { return DRV_ILI9488_TIMEOUT; }
    if (!ready) { return DRV_ILI9488_NOT_READY; }
    return ILI9488_WaitGuard();
}

/** @brief 一次操作只记一次故障；首个副作用前失败保留原状态。 */
static DRV_ILI9488_Status finish_operation(DRV_ILI9488_Status result, DRV_ILI9488_State success_state)
{
    if (result == DRV_ILI9488_OK) { device.info.state = success_state; }
    else {
        if (device.started) { device.info.state = DRV_ILI9488_FAULT; }
        device.diagnostics.last_error = result;
        device.diagnostics.last_failed_operation = device.operation;
        ILI9488_CountFailure(&device.diagnostics.failed_operations);
        if (result == DRV_ILI9488_TIMEOUT) { ILI9488_CountFailure(&device.diagnostics.timeout_operations); }
    }
    device.busy = false;
    return result;
}

/** @brief 复位并完整初始化控制器，成功后进入 READY；重复 Open 无 I/O。
 * @param timeout_ms 全流程预算，含两次保守切换等待，单位 ms。
 * @retval DRV_ILI9488 status
 * @pre 单上下文、非 ISR；CLOSED/READY。BSP 初始化由宿主完成。
 */
DRV_ILI9488_Status DRV_ILI9488_Open(uint32_t timeout_ms)
{
    DRV_ILI9488_Status result = validate_operation(timeout_ms);
    if (result != DRV_ILI9488_OK) { return result; }
    if (device.info.state == DRV_ILI9488_READY) { return DRV_ILI9488_OK; }
    if (device.info.state != DRV_ILI9488_CLOSED) { return DRV_ILI9488_INVALID_STATE; }
    result = begin_operation(DRV_ILI9488_OP_OPEN, timeout_ms);
    if (result == DRV_ILI9488_OK) { result = ILI9488_PanelOpen(device.config.panel, device.info.rotation); }
    return finish_operation(result, DRV_ILI9488_READY);
}

/** @brief READY 绘制前检查；空矩形仍须通过状态与预算合法性检查。 */
static DRV_ILI9488_Status validate_draw(uint32_t timeout_ms)
{
    DRV_ILI9488_Status result = validate_operation(timeout_ms);
    if (result == DRV_ILI9488_OK && device.info.state != DRV_ILI9488_READY) { result = DRV_ILI9488_INVALID_STATE; }
    return result;
}

/** @brief 用差值判断非空范围，避免 x+width 的整数溢出。 */
static bool rect_in_bounds(DRV_ILI9488_Rect rect)
{
    return rect.x < device.info.width && rect.y < device.info.height &&
        rect.width <= (uint16_t)(device.info.width - rect.x) &&
        rect.height <= (uint16_t)(device.info.height - rect.y);
}

/** @brief 发送一个轴的包含端点窗口，参数字节按高字节先行。 */
static DRV_ILI9488_Status axis_window(uint8_t command, uint16_t start, uint16_t length)
{
    uint16_t end = (uint16_t)(start + length - 1U);
    uint8_t bytes[4]; DRV_ILI9488_Status result;
    bytes[0] = (uint8_t)(start >> 8U); bytes[1] = (uint8_t)start;
    bytes[2] = (uint8_t)(end >> 8U); bytes[3] = (uint8_t)end;
    result = ILI9488_Command(command, 0U);
    return result == DRV_ILI9488_OK ? ILI9488_Bytes(bytes, 4U) : result;
}

/** @brief 设置整个矩形的一次窗口，随后 GRAM 地址由屏幕自动递增。 */
static DRV_ILI9488_Status start_window(DRV_ILI9488_Rect rect)
{
    DRV_ILI9488_Status result = axis_window(0x2AU, rect.x, rect.width);
    if (result == DRV_ILI9488_OK) { result = axis_window(0x2BU, rect.y, rect.height); }
    if (result == DRV_ILI9488_OK) { result = ILI9488_Command(0x2CU, 0U); }
    return result;
}

/** @brief 同步发送像素；source=NULL 为重复填色，否则只借用当前行。
 * @note 16位直接分批借用；8位明确高/低字节，不依赖宿主字节序。
 */
static DRV_ILI9488_Status pixels_write(const uint16_t *source, size_t count, uint16_t color)
{
    size_t units_per_pixel = device.info.dbi_width == DRV_ILI9488_DBI_8 ? 2U : 1U;
    size_t capacity = CONFIG_ILI9488_TX_UNITS / units_per_pixel;
    while (count != 0U) {
        size_t i, batch = count < capacity ? count : capacity;
        const uint16_t *units = source; DRV_ILI9488_Status result;
        if (source == NULL || units_per_pixel == 2U) {
            for (i = 0U; i < batch; ++i) {
                uint16_t pixel = source == NULL ? color : source[i];
                if (units_per_pixel == 2U) {
                    device.scratch[i * 2U] = (uint16_t)(pixel >> 8U);
                    device.scratch[i * 2U + 1U] = (uint16_t)(pixel & 0xFFU);
                } else { device.scratch[i] = pixel; }
            }
            units = device.scratch;
        }
        result = write_units(units, batch * units_per_pixel);
        if (result != DRV_ILI9488_OK) { return result; }
        count -= batch;
        if (source != NULL && count != 0U) { source += batch; }
    }
    return DRV_ILI9488_OK;
}

/** @brief 旋转成功后提交逻辑尺寸，相同方向无 I/O；失败保留旧方向。
 * @param rotation 合法方向枚举。
 * @param timeout_ms 总预算，单位 ms。
 * @retval DRV_ILI9488 status
 * @pre 单上下文、非 ISR；READY。成功后 App 应同步 Graphics 尺寸并重绘。
 */
DRV_ILI9488_Status DRV_ILI9488_SetRotation(DRV_ILI9488_Rotation rotation, uint32_t timeout_ms)
{
    DRV_ILI9488_Status result;
    if (device.busy) { return DRV_ILI9488_BUSY; }
    if (!rotation_valid(rotation)) { return DRV_ILI9488_INVALID_ARGUMENT; }
    result = validate_draw(timeout_ms);
    if (result != DRV_ILI9488_OK) { return result; }
    if (rotation == device.info.rotation) { return DRV_ILI9488_OK; }
    result = begin_operation(DRV_ILI9488_OP_ROTATE, timeout_ms);
    if (result == DRV_ILI9488_OK) { result = ILI9488_Command(0x36U, 0U); }
    if (result == DRV_ILI9488_OK) { result = ILI9488_Bytes(&device.config.panel->madctl[(size_t)rotation], 1U); }
    if (result == DRV_ILI9488_OK) { commit_rotation(rotation); }
    return finish_operation(result, DRV_ILI9488_READY);
}

/** @brief 用 RGB565 数值填充矩形；非空越界拒绝，空矩形无 port 调用。
 * @param rect 逻辑像素区域，不裁剪。
 * @param color_rgb565 颜色数值，与宿主内存字节序无关。
 * @param timeout_ms 全部批次共用的总预算，单位 ms。
 * @retval DRV_ILI9488 status
 * @pre 单上下文、非 ISR；READY。失败可已更新部分 GRAM。
 */
DRV_ILI9488_Status DRV_ILI9488_FillRect(DRV_ILI9488_Rect rect, uint16_t color_rgb565, uint32_t timeout_ms)
{
    DRV_ILI9488_Status result = validate_draw(timeout_ms);
    if (result != DRV_ILI9488_OK) { return result; }
    if (rect.width == 0U || rect.height == 0U) { return DRV_ILI9488_OK; }
    if (!rect_in_bounds(rect)) { return DRV_ILI9488_OUT_OF_RANGE; }
    result = begin_operation(DRV_ILI9488_OP_FILL, timeout_ms);
    if (result == DRV_ILI9488_OK) { result = start_window(rect); }
    if (result == DRV_ILI9488_OK) { result = pixels_write(NULL, (size_t)rect.width * rect.height, color_rgb565); }
    return finish_operation(result, DRV_ILI9488_READY);
}

/** @brief 同步写 RGB565 矩形，逐行跳过 padding，校验容量乘加溢出。
 * @param rect 逻辑像素区域，空矩形忽略源参数，仍要求 READY/合法预算。
 * @param pixels 对齐的 uint16_t 数组，仅借用到返回；期间调用方不可修改。
 * @param buffer_pixels 调用方声明的可读像素容量，驱动不能检验物理内存。
 * @param stride_pixels 相邻源行起点距离，以像素计，非空时不小于 width。
 * @param timeout_ms 全流程预算，单位 ms。
 * @retval DRV_ILI9488 status
 * @pre 单上下文、非 ISR；READY。失败返回也不再访问源区，不自动续传。
 */
DRV_ILI9488_Status DRV_ILI9488_BlitRect(DRV_ILI9488_Rect rect, const uint16_t *pixels,
                                     size_t buffer_pixels, size_t stride_pixels, uint32_t timeout_ms)
{
    size_t required, rows, row; DRV_ILI9488_Status result = validate_draw(timeout_ms);
    if (result != DRV_ILI9488_OK) { return result; }
    if (rect.width == 0U || rect.height == 0U) { return DRV_ILI9488_OK; }
    if (!rect_in_bounds(rect)) { return DRV_ILI9488_OUT_OF_RANGE; }
    if (pixels == NULL || stride_pixels < rect.width) { return DRV_ILI9488_INVALID_ARGUMENT; }
    rows = (size_t)rect.height - 1U;
    if (rows > (SIZE_MAX - rect.width) / stride_pixels) { return DRV_ILI9488_INVALID_ARGUMENT; }
    required = rows * stride_pixels + rect.width;
    if (buffer_pixels < required) { return DRV_ILI9488_BUFFER_TOO_SMALL; }
    result = begin_operation(DRV_ILI9488_OP_BLIT, timeout_ms);
    if (result == DRV_ILI9488_OK) { result = start_window(rect); }
    for (row = 0U; row < rect.height && result == DRV_ILI9488_OK; ++row) {
        result = pixels_write(pixels, rect.width, 0U);
        /* 最后一行不做额外指针运算，避免不需要的越界指针。 */
        if (result == DRV_ILI9488_OK && row + 1U < rect.height) { pixels += stride_pixels; }
    }
    return finish_operation(result, DRV_ILI9488_READY);
}

/** @brief 关显示并进入睡眠；SLEEPING 时幂等，无 I/O。
 * @param timeout_ms 含保护等待的总预算，单位 ms。
 * @retval DRV_ILI9488 status
 * @pre 单上下文、非 ISR；READY/SLEEPING。背光仍由 App/BSP 管理。
 */
DRV_ILI9488_Status DRV_ILI9488_Sleep(uint32_t timeout_ms)
{
    DRV_ILI9488_Status result = validate_operation(timeout_ms);
    if (result != DRV_ILI9488_OK) { return result; }
    if (device.info.state == DRV_ILI9488_SLEEPING) { return DRV_ILI9488_OK; }
    if (device.info.state != DRV_ILI9488_READY) { return DRV_ILI9488_INVALID_STATE; }
    result = begin_operation(DRV_ILI9488_OP_SLEEP, timeout_ms);
    if (result == DRV_ILI9488_OK) { result = ILI9488_PanelStop(device.config.panel); }
    return finish_operation(result, DRV_ILI9488_SLEEPING);
}

/** @brief 退出睡眠，恢复最近提交的方向和工作参数；READY 时无 I/O。
 * @param timeout_ms 全流程预算，单位 ms。
 * @retval DRV_ILI9488 status
 * @pre 单上下文、非 ISR；SLEEPING/READY。成功后查询尺寸并整屏重绘。
 * @note 不保证 GRAM 内容保留；失败进入 FAULT 的条件与其他事务一致。
 */
DRV_ILI9488_Status DRV_ILI9488_Wake(uint32_t timeout_ms)
{
    DRV_ILI9488_Status result = validate_operation(timeout_ms);
    if (result != DRV_ILI9488_OK) { return result; }
    if (device.info.state == DRV_ILI9488_READY) { return DRV_ILI9488_OK; }
    if (device.info.state != DRV_ILI9488_SLEEPING) { return DRV_ILI9488_INVALID_STATE; }
    result = begin_operation(DRV_ILI9488_OP_WAKE, timeout_ms);
    if (result == DRV_ILI9488_OK) { result = ILI9488_PanelWake(device.config.panel, device.info.rotation); }
    return finish_operation(result, DRV_ILI9488_READY);
}

/** @brief 执行停止序列后进入 CLOSED，保留绑定、诊断与最近成功方向。
 * @param timeout_ms 全流程预算，单位 ms。
 * @retval DRV_ILI9488 status
 * @pre 单上下文、非 ISR；CLOSED/READY/SLEEPING/FAULT，CLOSED 幂等。
 * @note 不释放 BSP 资源。失败不隐含补偿复位电平或无限重试。
 */
DRV_ILI9488_Status DRV_ILI9488_Close(uint32_t timeout_ms)
{
    DRV_ILI9488_Status result = validate_operation(timeout_ms);
    if (result != DRV_ILI9488_OK) { return result; }
    if (device.info.state == DRV_ILI9488_CLOSED) { return DRV_ILI9488_OK; }
    result = begin_operation(DRV_ILI9488_OP_CLOSE, timeout_ms);
    if (result == DRV_ILI9488_OK) { result = ILI9488_PanelStop(device.config.panel); }
    return finish_operation(result, DRV_ILI9488_CLOSED);
}

/** @brief 从 FAULT 显式完整重建，不自动重试；保留最近成功方向。
 * @param timeout_ms 含跨调用遗留保护等待及重建的总预算，单位 ms。
 * @retval DRV_ILI9488 status
 * @pre 单上下文、非 ISR；仅 FAULT。BSP 就绪与异常输出策略由系统负责。
 * @note 首个副作用前才计恢复尝试；成功后 App 查询尺寸并整屏重绘。
 */
DRV_ILI9488_Status DRV_ILI9488_Recover(uint32_t timeout_ms)
{
    DRV_ILI9488_Status result = validate_operation(timeout_ms);
    if (result != DRV_ILI9488_OK) { return result; }
    if (device.info.state != DRV_ILI9488_FAULT) { return DRV_ILI9488_INVALID_STATE; }
    result = begin_operation(DRV_ILI9488_OP_RECOVER, timeout_ms);
    if (result == DRV_ILI9488_OK) { result = ILI9488_PanelOpen(device.config.panel, device.info.rotation); }
    return finish_operation(result, DRV_ILI9488_READY);
}
