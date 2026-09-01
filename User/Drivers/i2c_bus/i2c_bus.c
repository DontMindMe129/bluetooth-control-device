/**
 * @file i2c_bus.c
 * @brief Hiện thực driver I2C master tổng quát sử dụng STM32 HAL interrupt.
 */

#include "i2c_bus.h"

#include <stddef.h>

/** @brief Chuyển địa chỉ 7-bit công khai sang định dạng HAL yêu cầu. */
static uint16_t i2c_bus_to_hal_address(uint8_t address_7bit)
{
    return (uint16_t)((uint16_t)address_7bit << 1U);
}

/** @brief Kiểm tra elapsed time theo cách an toàn khi HAL tick tràn số. */
static bool i2c_bus_timeout_has_elapsed(uint32_t current_tick_ms,
                                        uint32_t start_tick_ms,
                                        uint32_t timeout_ms)
{
    return ((uint32_t)(current_tick_ms - start_tick_ms) >= timeout_ms);
}

/** @brief Xóa thông tin vùng nhớ đang được driver mượn từ caller. */
static void i2c_bus_release_active_buffers(I2cBus_t *bus)
{
    bus->active_transmit_data = NULL;
    bus->active_receive_data = NULL;
    bus->status.active_memory_address = 0U;
    bus->status.active_data_length = 0U;
    bus->scan_address_buffer = NULL;
    bus->scan_address_capacity = 0U;
}

/** @brief Chốt kết quả và trả state machine về trạng thái không còn giao dịch chạy. */
static void i2c_bus_finish_operation(I2cBus_t *bus,
                                     I2cBus_Operation_t completed_operation,
                                     I2cBus_Result_t result,
                                     bool recovery_is_required)
{
    bus->status.is_busy = false;
    bus->status.recovery_is_required = recovery_is_required;
    bus->status.active_operation = I2C_BUS_OPERATION_NONE;
    bus->status.completed_operation = completed_operation;
    bus->status.last_result = result;
    bus->abort_is_in_progress = false;
    bus->operation_before_abort = I2C_BUS_OPERATION_NONE;
    bus->interrupt_event = I2C_BUS_INTERRUPT_EVENT_NONE;
    bus->result_is_new = true;
    i2c_bus_release_active_buffers(bus);
}

/** @brief Bắt đầu probe địa chỉ scan hiện tại bằng một giao dịch truyền dài 0 byte. */
static HAL_StatusTypeDef i2c_bus_start_current_scan_probe(I2cBus_t *bus,
                                                          uint32_t current_tick_ms)
{
    HAL_StatusTypeDef hal_result;

    bus->operation_start_tick_ms = current_tick_ms;
    bus->interrupt_event = I2C_BUS_INTERRUPT_EVENT_NONE;
    bus->interrupt_hal_error = HAL_I2C_ERROR_NONE;

    /*
     * HAL F1 hiện tại xử lý Size == 0 bằng cách chỉ phát START, địa chỉ và STOP.
     * Dummy byte cung cấp một con trỏ RAM hợp lệ dù handler sẽ không đọc byte này.
     */
    hal_result = HAL_I2C_Master_Transmit_IT(
        bus->hal_i2c,
        i2c_bus_to_hal_address(bus->status.active_address_7bit),
        &bus->scan_probe_dummy_byte,
        0U);

    return hal_result;
}

/** @brief Hoàn tất scan khi đã kiểm tra hết dải địa chỉ hợp lệ. */
static void i2c_bus_finish_scan_success(I2cBus_t *bus)
{
    i2c_bus_finish_operation(bus,
                             I2C_BUS_OPERATION_SCAN,
                             I2C_BUS_RESULT_SUCCESS,
                             false);
}

/** @brief Chuyển scan sang địa chỉ kế tiếp hoặc kết thúc nếu đã hết dải. */
static void i2c_bus_advance_scan(I2cBus_t *bus, uint32_t current_tick_ms)
{
    HAL_StatusTypeDef hal_result;

    if (bus->status.active_address_7bit >= I2C_BUS_SCAN_LAST_ADDRESS_7BIT)
    {
        i2c_bus_finish_scan_success(bus);
        return;
    }

    bus->status.active_address_7bit++;
    hal_result = i2c_bus_start_current_scan_probe(bus, current_tick_ms);
    if (hal_result != HAL_OK)
    {
        bus->status.last_hal_error = HAL_I2C_GetError(bus->hal_i2c);
        i2c_bus_finish_operation(bus,
                                 I2C_BUS_OPERATION_SCAN,
                                 I2C_BUS_RESULT_HAL_ERROR,
                                 (hal_result == HAL_BUSY));
    }
}

/** @brief Ghi lại một địa chỉ ACK mà không vượt quá buffer caller cung cấp. */
static void i2c_bus_store_found_scan_address(I2cBus_t *bus)
{
    bus->status.scan_found_count++;

    if (bus->status.scan_stored_count < bus->scan_address_capacity)
    {
        bus->scan_address_buffer[bus->status.scan_stored_count] =
            bus->status.active_address_7bit;
        bus->status.scan_stored_count++;
    }
    else
    {
        bus->status.scan_result_overflow = true;
    }
}

/** @brief Yêu cầu HAL hủy giao dịch đã vượt timeout mà không reset peripheral. */
static void i2c_bus_start_abort(I2cBus_t *bus, uint32_t current_tick_ms)
{
    HAL_StatusTypeDef hal_result;

    bus->operation_before_abort = bus->status.active_operation;
    hal_result = HAL_I2C_Master_Abort_IT(
        bus->hal_i2c,
        i2c_bus_to_hal_address(bus->status.active_address_7bit));

    if (hal_result == HAL_OK)
    {
        bus->abort_is_in_progress = true;
        bus->operation_start_tick_ms = current_tick_ms;
        bus->interrupt_event = I2C_BUS_INTERRUPT_EVENT_NONE;
    }
    else
    {
        bus->status.last_hal_error = HAL_I2C_GetError(bus->hal_i2c);
        i2c_bus_finish_operation(bus,
                                 bus->operation_before_abort,
                                 I2C_BUS_RESULT_ABORT_FAILED,
                                 true);
    }
}

bool I2cBus_Initialize(I2cBus_t *bus, I2C_HandleTypeDef *hal_i2c)
{
    if ((bus == NULL) || (hal_i2c == NULL) || (hal_i2c->Instance == NULL))
    {
        return false;
    }

    *bus = (I2cBus_t){0};
    bus->hal_i2c = hal_i2c;
    bus->status.is_initialized = true;
    return true;
}

I2cBus_StartResult_t I2cBus_StartTransmit(I2cBus_t *bus,
                                          uint8_t device_address_7bit,
                                          const uint8_t *data,
                                          uint16_t data_length,
                                          uint32_t current_tick_ms,
                                          uint32_t timeout_ms)
{
    HAL_StatusTypeDef hal_result;

    if ((bus == NULL) ||
        (data == NULL) ||
        (device_address_7bit > 0x7FU) ||
        (data_length == 0U) ||
        (timeout_ms == 0U))
    {
        return I2C_BUS_START_INVALID_ARGUMENT;
    }

    if (!bus->status.is_initialized)
    {
        return I2C_BUS_START_NOT_INITIALIZED;
    }

    if (bus->status.recovery_is_required)
    {
        return I2C_BUS_START_RECOVERY_REQUIRED;
    }

    if (bus->status.is_busy)
    {
        return I2C_BUS_START_BUSY;
    }

    if (bus->result_is_new)
    {
        return I2C_BUS_START_RESULT_PENDING;
    }

    bus->active_transmit_data = data;
    bus->status.active_address_7bit = device_address_7bit;
    bus->status.active_data_length = data_length;
    bus->status.active_operation = I2C_BUS_OPERATION_TRANSMIT;
    bus->status.completed_operation = I2C_BUS_OPERATION_NONE;
    bus->status.last_result = I2C_BUS_RESULT_NONE;
    bus->status.last_hal_error = HAL_I2C_ERROR_NONE;
    bus->status.is_busy = true;
    bus->operation_start_tick_ms = current_tick_ms;
    bus->operation_timeout_ms = timeout_ms;
    bus->interrupt_event = I2C_BUS_INTERRUPT_EVENT_NONE;
    bus->interrupt_hal_error = HAL_I2C_ERROR_NONE;
    bus->result_is_new = false;

    hal_result = HAL_I2C_Master_Transmit_IT(
        bus->hal_i2c,
        i2c_bus_to_hal_address(device_address_7bit),
        (uint8_t *)(uintptr_t)data,
        data_length);

    if (hal_result != HAL_OK)
    {
        bus->status.last_hal_error = HAL_I2C_GetError(bus->hal_i2c);
        bus->status.is_busy = false;
        bus->status.active_operation = I2C_BUS_OPERATION_NONE;
        i2c_bus_release_active_buffers(bus);
        return I2C_BUS_START_HAL_REJECTED;
    }

    return I2C_BUS_START_ACCEPTED;
}

I2cBus_StartResult_t I2cBus_StartMemoryRead(
    I2cBus_t *bus,
    uint8_t device_address_7bit,
    uint16_t memory_address,
    I2cBus_MemoryAddressSize_t memory_address_size,
    uint8_t *receive_data,
    uint16_t data_length,
    uint32_t current_tick_ms,
    uint32_t timeout_ms)
{
    HAL_StatusTypeDef hal_result;
    uint16_t hal_memory_address_size;

    if ((bus == NULL) ||
        (receive_data == NULL) ||
        (device_address_7bit > 0x7FU) ||
        (data_length == 0U) ||
        (timeout_ms == 0U) ||
        ((memory_address_size != I2C_BUS_MEMORY_ADDRESS_8BIT) &&
         (memory_address_size != I2C_BUS_MEMORY_ADDRESS_16BIT)) ||
        ((memory_address_size == I2C_BUS_MEMORY_ADDRESS_8BIT) &&
         (memory_address > UINT8_MAX)))
    {
        return I2C_BUS_START_INVALID_ARGUMENT;
    }

    if (!bus->status.is_initialized)
    {
        return I2C_BUS_START_NOT_INITIALIZED;
    }

    if (bus->status.recovery_is_required)
    {
        return I2C_BUS_START_RECOVERY_REQUIRED;
    }

    if (bus->status.is_busy)
    {
        return I2C_BUS_START_BUSY;
    }

    if (bus->result_is_new)
    {
        return I2C_BUS_START_RESULT_PENDING;
    }

    hal_memory_address_size =
        (memory_address_size == I2C_BUS_MEMORY_ADDRESS_8BIT)
            ? I2C_MEMADD_SIZE_8BIT
            : I2C_MEMADD_SIZE_16BIT;

    bus->active_receive_data = receive_data;
    bus->status.active_address_7bit = device_address_7bit;
    bus->status.active_memory_address = memory_address;
    bus->status.active_data_length = data_length;
    bus->status.active_operation = I2C_BUS_OPERATION_MEMORY_READ;
    bus->status.completed_operation = I2C_BUS_OPERATION_NONE;
    bus->status.last_result = I2C_BUS_RESULT_NONE;
    bus->status.last_hal_error = HAL_I2C_ERROR_NONE;
    bus->status.is_busy = true;
    bus->operation_start_tick_ms = current_tick_ms;
    bus->operation_timeout_ms = timeout_ms;
    bus->interrupt_event = I2C_BUS_INTERRUPT_EVENT_NONE;
    bus->interrupt_hal_error = HAL_I2C_ERROR_NONE;
    bus->result_is_new = false;

    hal_result = HAL_I2C_Mem_Read_IT(
        bus->hal_i2c,
        i2c_bus_to_hal_address(device_address_7bit),
        memory_address,
        hal_memory_address_size,
        receive_data,
        data_length);

    if (hal_result != HAL_OK)
    {
        bus->status.last_hal_error = HAL_I2C_GetError(bus->hal_i2c);
        bus->status.is_busy = false;
        bus->status.active_operation = I2C_BUS_OPERATION_NONE;
        i2c_bus_release_active_buffers(bus);
        return I2C_BUS_START_HAL_REJECTED;
    }

    return I2C_BUS_START_ACCEPTED;
}

I2cBus_StartResult_t I2cBus_StartScan(I2cBus_t *bus,
                                      uint8_t *found_addresses,
                                      uint8_t found_address_capacity,
                                      uint32_t current_tick_ms,
                                      uint32_t probe_timeout_ms)
{
    HAL_StatusTypeDef hal_result;

    if ((bus == NULL) ||
        (found_addresses == NULL) ||
        (found_address_capacity == 0U) ||
        (probe_timeout_ms == 0U))
    {
        return I2C_BUS_START_INVALID_ARGUMENT;
    }

    if (!bus->status.is_initialized)
    {
        return I2C_BUS_START_NOT_INITIALIZED;
    }

    if (bus->status.recovery_is_required)
    {
        return I2C_BUS_START_RECOVERY_REQUIRED;
    }

    if (bus->status.is_busy)
    {
        return I2C_BUS_START_BUSY;
    }

    if (bus->result_is_new)
    {
        return I2C_BUS_START_RESULT_PENDING;
    }

    bus->scan_address_buffer = found_addresses;
    bus->scan_address_capacity = found_address_capacity;
    bus->status.scan_found_count = 0U;
    bus->status.scan_stored_count = 0U;
    bus->status.scan_result_overflow = false;
    bus->status.active_address_7bit = I2C_BUS_SCAN_FIRST_ADDRESS_7BIT;
    bus->status.active_data_length = 0U;
    bus->status.active_operation = I2C_BUS_OPERATION_SCAN;
    bus->status.completed_operation = I2C_BUS_OPERATION_NONE;
    bus->status.last_result = I2C_BUS_RESULT_NONE;
    bus->status.last_hal_error = HAL_I2C_ERROR_NONE;
    bus->status.is_busy = true;
    bus->operation_timeout_ms = probe_timeout_ms;
    bus->result_is_new = false;

    hal_result = i2c_bus_start_current_scan_probe(bus, current_tick_ms);
    if (hal_result != HAL_OK)
    {
        bus->status.last_hal_error = HAL_I2C_GetError(bus->hal_i2c);
        bus->status.is_busy = false;
        bus->status.active_operation = I2C_BUS_OPERATION_NONE;
        i2c_bus_release_active_buffers(bus);
        return I2C_BUS_START_HAL_REJECTED;
    }

    return I2C_BUS_START_ACCEPTED;
}

I2cBus_StartResult_t I2cBus_StartProbe(I2cBus_t *bus,
                                       uint8_t device_address_7bit,
                                       uint32_t current_tick_ms,
                                       uint32_t timeout_ms)
{
    HAL_StatusTypeDef hal_result;

    if ((bus == NULL) || (device_address_7bit > 0x7FU) ||
        (timeout_ms == 0U))
    {
        return I2C_BUS_START_INVALID_ARGUMENT;
    }
    if (!bus->status.is_initialized)
    {
        return I2C_BUS_START_NOT_INITIALIZED;
    }
    if (bus->status.recovery_is_required)
    {
        return I2C_BUS_START_RECOVERY_REQUIRED;
    }
    if (bus->status.is_busy)
    {
        return I2C_BUS_START_BUSY;
    }
    if (bus->result_is_new)
    {
        return I2C_BUS_START_RESULT_PENDING;
    }

    bus->status.active_address_7bit = device_address_7bit;
    bus->status.active_data_length = 0U;
    bus->status.active_operation = I2C_BUS_OPERATION_PROBE;
    bus->status.completed_operation = I2C_BUS_OPERATION_NONE;
    bus->status.last_result = I2C_BUS_RESULT_NONE;
    bus->status.last_hal_error = HAL_I2C_ERROR_NONE;
    bus->status.is_busy = true;
    bus->operation_timeout_ms = timeout_ms;
    bus->result_is_new = false;

    hal_result = i2c_bus_start_current_scan_probe(bus, current_tick_ms);
    if (hal_result != HAL_OK)
    {
        bus->status.last_hal_error = HAL_I2C_GetError(bus->hal_i2c);
        bus->status.is_busy = false;
        bus->status.active_operation = I2C_BUS_OPERATION_NONE;
        return I2C_BUS_START_HAL_REJECTED;
    }
    return I2C_BUS_START_ACCEPTED;
}

void I2cBus_Service(I2cBus_t *bus, uint32_t current_tick_ms)
{
    I2cBus_InterruptEvent_t event;
    uint32_t hal_error;

    if ((bus == NULL) ||
        !bus->status.is_initialized ||
        !bus->status.is_busy)
    {
        return;
    }

    event = bus->interrupt_event;
    hal_error = bus->interrupt_hal_error;

    if (event != I2C_BUS_INTERRUPT_EVENT_NONE)
    {
        bus->interrupt_event = I2C_BUS_INTERRUPT_EVENT_NONE;

        if (event == I2C_BUS_INTERRUPT_EVENT_ABORT_COMPLETE)
        {
            i2c_bus_finish_operation(bus,
                                     bus->operation_before_abort,
                                     I2C_BUS_RESULT_TIMEOUT,
                                     false);
            return;
        }

        if (bus->abort_is_in_progress)
        {
            if (event == I2C_BUS_INTERRUPT_EVENT_ERROR)
            {
                bus->status.last_hal_error = hal_error;
                i2c_bus_finish_operation(bus,
                                         bus->operation_before_abort,
                                         I2C_BUS_RESULT_ABORT_FAILED,
                                         true);
            }
            return;
        }

        if (bus->status.active_operation == I2C_BUS_OPERATION_TRANSMIT)
        {
            if (event == I2C_BUS_INTERRUPT_EVENT_TRANSMIT_COMPLETE)
            {
                i2c_bus_finish_operation(bus,
                                         I2C_BUS_OPERATION_TRANSMIT,
                                         I2C_BUS_RESULT_SUCCESS,
                                         false);
            }
            else
            {
                bus->status.last_hal_error = hal_error;
                i2c_bus_finish_operation(bus,
                                         I2C_BUS_OPERATION_TRANSMIT,
                                         (hal_error == HAL_I2C_ERROR_AF)
                                             ? I2C_BUS_RESULT_NACK
                                             : I2C_BUS_RESULT_HAL_ERROR,
                                         false);
            }
            return;
        }

        if (bus->status.active_operation == I2C_BUS_OPERATION_MEMORY_READ)
        {
            if (event == I2C_BUS_INTERRUPT_EVENT_MEMORY_READ_COMPLETE)
            {
                i2c_bus_finish_operation(bus,
                                         I2C_BUS_OPERATION_MEMORY_READ,
                                         I2C_BUS_RESULT_SUCCESS,
                                         false);
            }
            else
            {
                bus->status.last_hal_error = hal_error;
                i2c_bus_finish_operation(bus,
                                         I2C_BUS_OPERATION_MEMORY_READ,
                                         (hal_error == HAL_I2C_ERROR_AF)
                                             ? I2C_BUS_RESULT_NACK
                                             : I2C_BUS_RESULT_HAL_ERROR,
                                         false);
            }
            return;
        }

        if (bus->status.active_operation == I2C_BUS_OPERATION_PROBE)
        {
            if (event == I2C_BUS_INTERRUPT_EVENT_TRANSMIT_COMPLETE)
            {
                i2c_bus_finish_operation(bus, I2C_BUS_OPERATION_PROBE,
                                         I2C_BUS_RESULT_SUCCESS, false);
            }
            else
            {
                bus->status.last_hal_error = hal_error;
                i2c_bus_finish_operation(
                    bus, I2C_BUS_OPERATION_PROBE,
                    (hal_error == HAL_I2C_ERROR_AF)
                        ? I2C_BUS_RESULT_NACK
                        : I2C_BUS_RESULT_HAL_ERROR,
                    false);
            }
            return;
        }

        if (bus->status.active_operation == I2C_BUS_OPERATION_SCAN)
        {
            if (event == I2C_BUS_INTERRUPT_EVENT_TRANSMIT_COMPLETE)
            {
                i2c_bus_store_found_scan_address(bus);
                i2c_bus_advance_scan(bus, current_tick_ms);
            }
            else if (hal_error == HAL_I2C_ERROR_AF)
            {
                /* NACK là kết quả bình thường khi địa chỉ đang scan không tồn tại. */
                i2c_bus_advance_scan(bus, current_tick_ms);
            }
            else
            {
                bus->status.last_hal_error = hal_error;
                i2c_bus_finish_operation(bus,
                                         I2C_BUS_OPERATION_SCAN,
                                         I2C_BUS_RESULT_HAL_ERROR,
                                         false);
            }
            return;
        }
    }

    if (i2c_bus_timeout_has_elapsed(current_tick_ms,
                                    bus->operation_start_tick_ms,
                                    bus->operation_timeout_ms))
    {
        uint32_t interrupt_mask;

        /*
         * Không cho callback hoàn thành chen vào giữa lần kiểm tra sự kiện cuối
         * và yêu cầu abort. Vùng critical chỉ gồm kiểm tra cờ và lời gọi HAL
         * khởi động abort; không chờ phần cứng hoàn thành.
         */
        interrupt_mask = __get_PRIMASK();
        __disable_irq();

        if (bus->interrupt_event != I2C_BUS_INTERRUPT_EVENT_NONE)
        {
            __set_PRIMASK(interrupt_mask);
            return;
        }

        if (bus->abort_is_in_progress)
        {
            i2c_bus_finish_operation(bus,
                                     bus->operation_before_abort,
                                     I2C_BUS_RESULT_ABORT_FAILED,
                                     true);
        }
        else
        {
            i2c_bus_start_abort(bus, current_tick_ms);
        }

        __set_PRIMASK(interrupt_mask);
    }
}

bool I2cBus_TakeResult(I2cBus_t *bus,
                       I2cBus_Operation_t *completed_operation,
                       I2cBus_Result_t *result)
{
    if ((bus == NULL) || (result == NULL) || !bus->result_is_new)
    {
        return false;
    }

    if (completed_operation != NULL)
    {
        *completed_operation = bus->status.completed_operation;
    }

    *result = bus->status.last_result;
    bus->result_is_new = false;
    return true;
}

void I2cBus_GetStatus(const I2cBus_t *bus, I2cBus_Status_t *output_status)
{
    if ((bus != NULL) && (output_status != NULL))
    {
        *output_status = bus->status;
    }
}

void I2cBus_HandleMasterTransmitCompleteInterrupt(I2cBus_t *bus,
                                                 I2C_HandleTypeDef *hal_i2c)
{
    if ((bus != NULL) &&
        bus->status.is_initialized &&
        bus->status.is_busy &&
        (hal_i2c == bus->hal_i2c))
    {
        bus->interrupt_event = I2C_BUS_INTERRUPT_EVENT_TRANSMIT_COMPLETE;
    }
}

void I2cBus_HandleErrorInterrupt(I2cBus_t *bus,
                                 I2C_HandleTypeDef *hal_i2c)
{
    if ((bus != NULL) &&
        bus->status.is_initialized &&
        bus->status.is_busy &&
        (hal_i2c == bus->hal_i2c))
    {
        bus->interrupt_hal_error = HAL_I2C_GetError(hal_i2c);
        bus->interrupt_event = I2C_BUS_INTERRUPT_EVENT_ERROR;
    }
}

void I2cBus_HandleMemoryReadCompleteInterrupt(I2cBus_t *bus,
                                             I2C_HandleTypeDef *hal_i2c)
{
    if ((bus != NULL) &&
        bus->status.is_initialized &&
        bus->status.is_busy &&
        (bus->status.active_operation == I2C_BUS_OPERATION_MEMORY_READ) &&
        (hal_i2c == bus->hal_i2c))
    {
        bus->interrupt_event = I2C_BUS_INTERRUPT_EVENT_MEMORY_READ_COMPLETE;
    }
}

void I2cBus_HandleAbortCompleteInterrupt(I2cBus_t *bus,
                                        I2C_HandleTypeDef *hal_i2c)
{
    if ((bus != NULL) &&
        bus->status.is_initialized &&
        bus->status.is_busy &&
        bus->abort_is_in_progress &&
        (hal_i2c == bus->hal_i2c))
    {
        bus->interrupt_event = I2C_BUS_INTERRUPT_EVENT_ABORT_COMPLETE;
    }
}
