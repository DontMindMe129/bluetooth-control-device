/**
 * @file adxl345.c
 * @brief Hiện thực driver ADXL345 qua I2C interrupt, không polling hoàn thành.
 */

#include "adxl345.h"

#include <limits.h>
#include <stddef.h>

#define ADXL345_PRIMARY_ADDRESS_7BIT       (0x53U)
#define ADXL345_ALTERNATE_ADDRESS_7BIT     (0x1DU)
#define ADXL345_EXPECTED_DEVICE_ID         (0xE5U)

#define ADXL345_REGISTER_DEVID             (0x00U)
#define ADXL345_REGISTER_BW_RATE           (0x2CU)
#define ADXL345_REGISTER_POWER_CTL         (0x2DU)
#define ADXL345_REGISTER_INT_ENABLE        (0x2EU)
#define ADXL345_REGISTER_INT_MAP           (0x2FU)
#define ADXL345_REGISTER_DATA_FORMAT       (0x31U)
#define ADXL345_REGISTER_DATAX0            (0x32U)

#define ADXL345_BW_RATE_100_HZ             (0x0AU)
#define ADXL345_DATA_FORMAT_FULL_RES_2G    (0x08U)
#define ADXL345_INT_MAP_DATA_READY_TO_INT1 (0x00U)
#define ADXL345_POWER_CTL_MEASURE           (0x08U)
#define ADXL345_INT_ENABLE_DATA_READY       (0x80U)
#define ADXL345_AXIS_DATA_LENGTH            (6U)

/** @brief Kiểm tra tick elapsed theo cách an toàn khi uint32_t tràn. */
static bool adxl345_time_has_elapsed(uint32_t now,
                                     uint32_t start,
                                     uint32_t duration)
{
    return ((uint32_t)(now - start) >= duration);
}

/** @brief Kiểm tra bitmask chứa đúng một GPIO pin. */
static bool adxl345_is_single_pin(uint16_t pin)
{
    return ((pin != 0U) && ((pin & (uint16_t)(pin - 1U)) == 0U));
}

/** @brief Lấy và xóa cờ DATA_READY do ISR ghi trong critical section ngắn. */
static bool adxl345_take_data_ready(Adxl345_t *sensor)
{
    uint32_t previous_primask = __get_PRIMASK();
    bool pending;

    __disable_irq();
    pending = sensor->data_ready_pending;
    sensor->data_ready_pending = false;
    __set_PRIMASK(previous_primask);
    return pending;
}

/** @brief Ánh xạ kết quả I2C bus sang lỗi driver có ý nghĩa. */
static Adxl345_Error_t adxl345_error_from_bus_result(I2cBus_Result_t result)
{
    if (result == I2C_BUS_RESULT_TIMEOUT)
    {
        return ADXL345_ERROR_I2C_TIMEOUT;
    }
    if (result == I2C_BUS_RESULT_ABORT_FAILED)
    {
        return ADXL345_ERROR_I2C_ABORT;
    }
    return ADXL345_ERROR_I2C_TRANSFER;
}

/** @brief Bắt đầu đọc một hoặc nhiều thanh ghi 8-bit. */
static bool adxl345_start_read(Adxl345_t *sensor,
                               uint8_t register_address,
                               uint8_t *data,
                               uint16_t length,
                               uint32_t current_tick_ms)
{
    I2cBus_StartResult_t start_result = I2cBus_StartMemoryRead(
        sensor->config.i2c_bus,
        sensor->status.selected_address_7bit,
        register_address,
        I2C_BUS_MEMORY_ADDRESS_8BIT,
        data,
        length,
        current_tick_ms,
        sensor->config.transfer_timeout_ms);

    sensor->status.transaction_is_pending =
        (start_result == I2C_BUS_START_ACCEPTED);
    return sensor->status.transaction_is_pending;
}

/** @brief Bắt đầu ghi một thanh ghi bằng buffer [địa chỉ, giá trị]. */
static bool adxl345_start_register_write(Adxl345_t *sensor,
                                         uint8_t register_address,
                                         uint8_t value,
                                         uint32_t current_tick_ms)
{
    I2cBus_StartResult_t start_result;

    sensor->transfer_buffer[0] = register_address;
    sensor->transfer_buffer[1] = value;
    start_result = I2cBus_StartTransmit(
        sensor->config.i2c_bus,
        sensor->status.selected_address_7bit,
        sensor->transfer_buffer,
        2U,
        current_tick_ms,
        sensor->config.transfer_timeout_ms);

    sensor->status.transaction_is_pending =
        (start_result == I2C_BUS_START_ACCEPTED);
    return sensor->status.transaction_is_pending;
}

/** @brief Chuyển một raw count full-resolution sang mg với scale danh định 3,9 mg/LSB. */
static int32_t adxl345_raw_to_mg(int16_t raw_value)
{
    return ((int32_t)raw_value * 39L) / 10L;
}

/** @brief Chuyển hai byte little-endian thành int16_t two's-complement. */
static int16_t adxl345_join_axis_bytes(uint8_t low_byte, uint8_t high_byte)
{
    return (int16_t)((uint16_t)low_byte | ((uint16_t)high_byte << 8U));
}

/** @brief Đưa driver vào retry wait hoặc ERROR sau một lỗi khởi tạo. */
static void adxl345_fail_initialization(Adxl345_t *sensor,
                                        Adxl345_Error_t error,
                                        uint32_t current_tick_ms)
{
    sensor->status.last_error = error;
    sensor->status.has_error = true;
    sensor->status.is_ready = false;
    sensor->status.transaction_is_pending = false;

    if (sensor->status.initialize_attempt_count <
        sensor->config.initialize_max_attempts)
    {
        sensor->status.state = ADXL345_STATE_RETRY_WAIT;
        sensor->state_start_tick_ms = current_tick_ms;
    }
    else
    {
        sensor->status.state = ADXL345_STATE_ERROR;
    }
}

/** @brief Bắt đầu probe một trong hai địa chỉ hợp lệ của ADXL345. */
static bool adxl345_start_probe(Adxl345_t *sensor,
                                uint8_t address_7bit,
                                Adxl345_State_t state,
                                uint32_t current_tick_ms)
{
    sensor->status.selected_address_7bit = address_7bit;
    sensor->status.state = state;
    return adxl345_start_read(sensor,
                              ADXL345_REGISTER_DEVID,
                              &sensor->status.device_id,
                              1U,
                              current_tick_ms);
}

/** @brief Bắt đầu một lần khởi tạo đầy đủ tại địa chỉ ưu tiên 0x53. */
static bool adxl345_start_initialization_attempt(Adxl345_t *sensor,
                                                 uint32_t current_tick_ms)
{
    if (sensor->status.initialize_attempt_count < UINT8_MAX)
    {
        sensor->status.initialize_attempt_count++;
    }

    sensor->probe_saw_invalid_device = false;
    sensor->status.device_id = 0U;
    return adxl345_start_probe(sensor,
                               ADXL345_PRIMARY_ADDRESS_7BIT,
                               ADXL345_STATE_PROBING_PRIMARY,
                               current_tick_ms);
}

/** @brief Tiếp tục chuỗi cấu hình sau khi DEVID hợp lệ. */
static bool adxl345_start_rate_configuration(Adxl345_t *sensor,
                                              uint32_t current_tick_ms)
{
    sensor->status.state = ADXL345_STATE_CONFIGURING_RATE;
    return adxl345_start_register_write(sensor,
                                         ADXL345_REGISTER_BW_RATE,
                                         ADXL345_BW_RATE_100_HZ,
                                         current_tick_ms);
}

/** @brief Xử lý probe hiện tại và chuyển địa chỉ hoặc bắt đầu cấu hình. */
static void adxl345_handle_probe_result(Adxl345_t *sensor,
                                        I2cBus_Result_t result,
                                        uint32_t current_tick_ms)
{
    I2cBus_Status_t bus_status;
    bool current_is_primary =
        (sensor->status.state == ADXL345_STATE_PROBING_PRIMARY);

    I2cBus_GetStatus(sensor->config.i2c_bus, &bus_status);

    if (result == I2C_BUS_RESULT_SUCCESS)
    {
        if (sensor->status.device_id == ADXL345_EXPECTED_DEVICE_ID)
        {
            if (!adxl345_start_rate_configuration(sensor, current_tick_ms))
            {
                adxl345_fail_initialization(sensor,
                                            ADXL345_ERROR_I2C_START,
                                            current_tick_ms);
            }
            return;
        }
        sensor->probe_saw_invalid_device = true;
    }
    else if ((result != I2C_BUS_RESULT_NACK) &&
             ((result != I2C_BUS_RESULT_HAL_ERROR) ||
              (bus_status.last_hal_error != HAL_I2C_ERROR_AF)))
    {
        adxl345_fail_initialization(sensor,
                                    adxl345_error_from_bus_result(result),
                                    current_tick_ms);
        return;
    }

    if (current_is_primary)
    {
        if (!adxl345_start_probe(sensor,
                                 ADXL345_ALTERNATE_ADDRESS_7BIT,
                                 ADXL345_STATE_PROBING_ALTERNATE,
                                 current_tick_ms))
        {
            adxl345_fail_initialization(sensor,
                                        ADXL345_ERROR_I2C_START,
                                        current_tick_ms);
        }
        return;
    }

    adxl345_fail_initialization(
        sensor,
        sensor->probe_saw_invalid_device
            ? ADXL345_ERROR_INVALID_DEVICE_ID
            : ADXL345_ERROR_DEVICE_NOT_FOUND,
        current_tick_ms);
}

/** @brief Chuyển sang bước cấu hình kế tiếp sau một register write thành công. */
static void adxl345_advance_configuration(Adxl345_t *sensor,
                                          uint32_t current_tick_ms)
{
    bool started = false;

    switch (sensor->status.state)
    {
        case ADXL345_STATE_CONFIGURING_RATE:
            sensor->status.state = ADXL345_STATE_CONFIGURING_FORMAT;
            started = adxl345_start_register_write(
                sensor,
                ADXL345_REGISTER_DATA_FORMAT,
                ADXL345_DATA_FORMAT_FULL_RES_2G,
                current_tick_ms);
            break;

        case ADXL345_STATE_CONFIGURING_FORMAT:
            sensor->status.state = ADXL345_STATE_CONFIGURING_INT_MAP;
            started = adxl345_start_register_write(
                sensor,
                ADXL345_REGISTER_INT_MAP,
                ADXL345_INT_MAP_DATA_READY_TO_INT1,
                current_tick_ms);
            break;

        case ADXL345_STATE_CONFIGURING_INT_MAP:
            sensor->status.state = ADXL345_STATE_ENABLING_MEASUREMENT;
            started = adxl345_start_register_write(
                sensor,
                ADXL345_REGISTER_POWER_CTL,
                ADXL345_POWER_CTL_MEASURE,
                current_tick_ms);
            break;

        case ADXL345_STATE_ENABLING_MEASUREMENT:
            sensor->status.state = ADXL345_STATE_ENABLING_DATA_READY;
            started = adxl345_start_register_write(
                sensor,
                ADXL345_REGISTER_INT_ENABLE,
                ADXL345_INT_ENABLE_DATA_READY,
                current_tick_ms);
            break;

        case ADXL345_STATE_ENABLING_DATA_READY:
            sensor->status.state = ADXL345_STATE_READY;
            sensor->status.is_ready = true;
            sensor->status.has_error = false;
            sensor->status.last_error = ADXL345_ERROR_NONE;
            sensor->state_start_tick_ms = current_tick_ms;

            /*
             * Đọc một mẫu ngay sau khi bật DATA_READY để xóa trạng thái
             * interrupt có thể đã được chốt trước lúc MCU bắt được cạnh lên.
             * Các mẫu tiếp theo vẫn được kích hoạt bởi EXTI từ chân INT1.
             */
            sensor->data_ready_pending = true;
            return;

        default:
            adxl345_fail_initialization(sensor,
                                        ADXL345_ERROR_UNEXPECTED_RESULT,
                                        current_tick_ms);
            return;
    }

    if (!started)
    {
        adxl345_fail_initialization(sensor,
                                    ADXL345_ERROR_I2C_START,
                                    current_tick_ms);
    }
}

/** @brief Chốt một mẫu sáu byte và cập nhật trạng thái public. */
static void adxl345_store_sample(Adxl345_t *sensor, uint32_t current_tick_ms)
{
    Adxl345_Sample_t *sample = &sensor->status.latest_sample;

    sample->raw_x = adxl345_join_axis_bytes(sensor->transfer_buffer[0],
                                            sensor->transfer_buffer[1]);
    sample->raw_y = adxl345_join_axis_bytes(sensor->transfer_buffer[2],
                                            sensor->transfer_buffer[3]);
    sample->raw_z = adxl345_join_axis_bytes(sensor->transfer_buffer[4],
                                            sensor->transfer_buffer[5]);
    sample->x_mg = adxl345_raw_to_mg(sample->raw_x);
    sample->y_mg = adxl345_raw_to_mg(sample->raw_y);
    sample->z_mg = adxl345_raw_to_mg(sample->raw_z);
    sample->sample_tick_ms = current_tick_ms;

    sensor->status.has_sample = true;
    sensor->status.sample_is_fresh = true;
    sensor->status.has_error = false;
    sensor->status.last_error = ADXL345_ERROR_NONE;
    sensor->sample_is_new = true;
    if (sensor->status.successful_sample_count < UINT32_MAX)
    {
        sensor->status.successful_sample_count++;
    }
}

Adxl345_InitializeResult_t Adxl345_Initialize(
    Adxl345_t *sensor,
    const Adxl345_Config_t *config,
    uint32_t current_tick_ms)
{
    if ((sensor == NULL) ||
        (config == NULL) ||
        (config->i2c_bus == NULL) ||
        !config->i2c_bus->status.is_initialized ||
        !adxl345_is_single_pin(config->data_ready_pin) ||
        (config->transfer_timeout_ms == 0U) ||
        (config->initialize_retry_delay_ms == 0U) ||
        (config->stale_timeout_ms == 0U) ||
        (config->initialize_max_attempts == 0U))
    {
        if (sensor != NULL)
        {
            *sensor = (Adxl345_t){0};
            sensor->status.last_error = ADXL345_ERROR_INVALID_CONFIG;
            sensor->status.has_error = true;
        }
        return ADXL345_INITIALIZE_INVALID_CONFIG;
    }

    *sensor = (Adxl345_t){0};
    sensor->config = *config;
    sensor->status.is_initialized = true;

    if (!adxl345_start_initialization_attempt(sensor, current_tick_ms))
    {
        adxl345_fail_initialization(sensor,
                                    ADXL345_ERROR_I2C_START,
                                    current_tick_ms);
    }

    return ADXL345_INITIALIZE_ACCEPTED;
}

void Adxl345_Service(Adxl345_t *sensor, uint32_t current_tick_ms)
{
    I2cBus_Operation_t completed_operation;
    I2cBus_Result_t bus_result;

    if ((sensor == NULL) || !sensor->status.is_initialized)
    {
        return;
    }

    if (sensor->status.transaction_is_pending)
    {
        if (!I2cBus_TakeResult(sensor->config.i2c_bus,
                               &completed_operation,
                               &bus_result))
        {
            return;
        }

        sensor->status.transaction_is_pending = false;

        if ((sensor->status.state == ADXL345_STATE_PROBING_PRIMARY) ||
            (sensor->status.state == ADXL345_STATE_PROBING_ALTERNATE))
        {
            if (completed_operation != I2C_BUS_OPERATION_MEMORY_READ)
            {
                adxl345_fail_initialization(sensor,
                                            ADXL345_ERROR_UNEXPECTED_RESULT,
                                            current_tick_ms);
            }
            else
            {
                adxl345_handle_probe_result(sensor,
                                            bus_result,
                                            current_tick_ms);
            }
            return;
        }

        if (sensor->status.state == ADXL345_STATE_READING_SAMPLE)
        {
            sensor->status.state = ADXL345_STATE_READY;
            if ((completed_operation == I2C_BUS_OPERATION_MEMORY_READ) &&
                (bus_result == I2C_BUS_RESULT_SUCCESS))
            {
                adxl345_store_sample(sensor, current_tick_ms);
            }
            else
            {
                sensor->status.has_error = true;
                sensor->status.last_error =
                    (completed_operation == I2C_BUS_OPERATION_MEMORY_READ)
                        ? adxl345_error_from_bus_result(bus_result)
                        : ADXL345_ERROR_UNEXPECTED_RESULT;
                if (sensor->status.failed_sample_count < UINT32_MAX)
                {
                    sensor->status.failed_sample_count++;
                }
            }
        }
        else if (completed_operation != I2C_BUS_OPERATION_TRANSMIT)
        {
            adxl345_fail_initialization(sensor,
                                        ADXL345_ERROR_UNEXPECTED_RESULT,
                                        current_tick_ms);
            return;
        }
        else if (bus_result != I2C_BUS_RESULT_SUCCESS)
        {
            adxl345_fail_initialization(sensor,
                                        adxl345_error_from_bus_result(bus_result),
                                        current_tick_ms);
            return;
        }
        else
        {
            adxl345_advance_configuration(sensor, current_tick_ms);
            return;
        }
    }

    if ((sensor->status.state == ADXL345_STATE_RETRY_WAIT) &&
        adxl345_time_has_elapsed(current_tick_ms,
                                 sensor->state_start_tick_ms,
                                 sensor->config.initialize_retry_delay_ms))
    {
        if (!adxl345_start_initialization_attempt(sensor, current_tick_ms))
        {
            adxl345_fail_initialization(sensor,
                                        ADXL345_ERROR_I2C_START,
                                        current_tick_ms);
        }
        return;
    }

    if ((sensor->status.state == ADXL345_STATE_READY) &&
        adxl345_take_data_ready(sensor))
    {
        sensor->status.state = ADXL345_STATE_READING_SAMPLE;
        if (!adxl345_start_read(sensor,
                                ADXL345_REGISTER_DATAX0,
                                sensor->transfer_buffer,
                                ADXL345_AXIS_DATA_LENGTH,
                                current_tick_ms))
        {
            sensor->status.state = ADXL345_STATE_READY;
            sensor->status.has_error = true;
            sensor->status.last_error = ADXL345_ERROR_I2C_START;
            if (sensor->status.failed_sample_count < UINT32_MAX)
            {
                sensor->status.failed_sample_count++;
            }
        }
    }

    if (sensor->status.is_ready)
    {
        uint32_t freshness_start = sensor->status.has_sample
            ? sensor->status.latest_sample.sample_tick_ms
            : sensor->state_start_tick_ms;

        sensor->status.sample_is_fresh =
            sensor->status.has_sample &&
            !adxl345_time_has_elapsed(current_tick_ms,
                                      freshness_start,
                                      sensor->config.stale_timeout_ms);

        if (adxl345_time_has_elapsed(current_tick_ms,
                                     freshness_start,
                                     sensor->config.stale_timeout_ms))
        {
            sensor->status.has_error = true;
            sensor->status.last_error = ADXL345_ERROR_DATA_STALE;
        }
    }
}

void Adxl345_HandleDataReadyInterrupt(Adxl345_t *sensor, uint16_t gpio_pin)
{
    if ((sensor != NULL) &&
        sensor->status.is_initialized &&
        (gpio_pin == sensor->config.data_ready_pin))
    {
        sensor->data_ready_pending = true;
    }
}

bool Adxl345_TakeNewSample(Adxl345_t *sensor, Adxl345_Sample_t *output_sample)
{
    if ((sensor == NULL) ||
        (output_sample == NULL) ||
        !sensor->sample_is_new)
    {
        return false;
    }

    *output_sample = sensor->status.latest_sample;
    sensor->sample_is_new = false;
    return true;
}

void Adxl345_GetStatus(const Adxl345_t *sensor, Adxl345_Status_t *output_status)
{
    if ((sensor != NULL) && (output_status != NULL))
    {
        *output_status = sensor->status;
    }
}
