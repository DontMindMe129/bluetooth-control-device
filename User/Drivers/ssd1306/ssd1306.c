/**
 * @file ssd1306.c
 * @brief Hiện thực SSD1306 I2C 128x64 với framebuffer tĩnh và state machine bất đồng bộ.
 */

#include "ssd1306.h"

#include <stddef.h>

/** @brief Control byte báo các byte tiếp theo là command. */
#define SSD1306_CONTROL_COMMAND                    0x00U

/** @brief Control byte báo các byte tiếp theo là dữ liệu GDDRAM. */
#define SSD1306_CONTROL_GDDRAM_DATA                0x40U

/** @brief Offset byte đầu tiên của framebuffer trong frame_packet. */
#define SSD1306_FRAMEBUFFER_OFFSET                 1U

/** @brief Các command SSD1306 được dùng trong initialization và refresh. */
#define SSD1306_CMD_DISPLAY_OFF                    0xAEU
#define SSD1306_CMD_DISPLAY_ON                     0xAFU
#define SSD1306_CMD_SET_DISPLAY_CLOCK              0xD5U
#define SSD1306_CMD_SET_MULTIPLEX_RATIO            0xA8U
#define SSD1306_CMD_SET_DISPLAY_OFFSET             0xD3U
#define SSD1306_CMD_SET_START_LINE_0               0x40U
#define SSD1306_CMD_SET_CHARGE_PUMP                0x8DU
#define SSD1306_CMD_SET_MEMORY_ADDRESSING_MODE     0x20U
#define SSD1306_CMD_SET_COLUMN_ADDRESS             0x21U
#define SSD1306_CMD_SET_PAGE_ADDRESS               0x22U
#define SSD1306_CMD_SEGMENT_NORMAL                 0xA0U
#define SSD1306_CMD_SEGMENT_REMAP                  0xA1U
#define SSD1306_CMD_COM_SCAN_NORMAL                0xC0U
#define SSD1306_CMD_COM_SCAN_REVERSE               0xC8U
#define SSD1306_CMD_SET_COM_PINS                   0xDAU
#define SSD1306_CMD_SET_CONTRAST                    0x81U
#define SSD1306_CMD_SET_PRECHARGE                   0xD9U
#define SSD1306_CMD_SET_VCOMH                       0xDBU
#define SSD1306_CMD_RESUME_GDDRAM_DISPLAY           0xA4U
#define SSD1306_CMD_NORMAL_DISPLAY                  0xA6U
#define SSD1306_CMD_DEACTIVATE_SCROLL               0x2EU

/** @brief Tham số initialization dành cho panel 128x64 dùng charge pump nội. */
#define SSD1306_DISPLAY_CLOCK_DEFAULT               0x80U
#define SSD1306_MULTIPLEX_64_ROWS                   0x3FU
#define SSD1306_DISPLAY_OFFSET_ZERO                 0x00U
#define SSD1306_CHARGE_PUMP_ENABLE                  0x14U
#define SSD1306_ADDRESSING_MODE_HORIZONTAL          0x00U
#define SSD1306_COM_PINS_128X64                     0x12U
#define SSD1306_PRECHARGE_INTERNAL_VCC              0xF1U
#define SSD1306_VCOMH_0_77_VCC                      0x40U
#define SSD1306_FIRST_COLUMN                        0x00U
#define SSD1306_LAST_COLUMN                         0x7FU
#define SSD1306_FIRST_PAGE                          0x00U
#define SSD1306_LAST_PAGE                           0x07U

/** @brief Kết quả nội bộ khi thử bắt đầu một gói I2C. */
typedef enum
{
    SSD1306_START_TRANSFER_STARTED = 0,
    SSD1306_START_TRANSFER_WAIT,
    SSD1306_START_TRANSFER_FAILED
} Ssd1306_StartTransferResult_t;

/** @brief Kết quả nội bộ khi chờ một gói I2C đã được chấp nhận. */
typedef enum
{
    SSD1306_WAIT_TRANSFER_PENDING = 0,
    SSD1306_WAIT_TRANSFER_SUCCESS,
    SSD1306_WAIT_TRANSFER_FAILED
} Ssd1306_WaitTransferResult_t;

/** @brief Kiểm tra elapsed time an toàn khi HAL tick tràn số. */
static bool ssd1306_timeout_has_elapsed(uint32_t current_tick_ms,
                                        uint32_t start_tick_ms,
                                        uint32_t timeout_ms)
{
    return ((uint32_t)(current_tick_ms - start_tick_ms) >= timeout_ms);
}

/** @brief Kiểm tra operation pixel thuộc tập giá trị công khai hợp lệ. */
static bool ssd1306_pixel_operation_is_valid(Ssd1306_PixelOperation_t operation)
{
    return ((operation == SSD1306_PIXEL_OFF) ||
            (operation == SSD1306_PIXEL_ON) ||
            (operation == SSD1306_PIXEL_TOGGLE));
}

/** @brief Chuyển state và ghi tick bắt đầu chờ giành bus của state mới. */
static void ssd1306_enter_state(Ssd1306_t *display,
                                Ssd1306_State_t state,
                                uint32_t current_tick_ms)
{
    display->status.state = state;
    display->state_start_tick_ms = current_tick_ms;
}

/** @brief Ánh xạ kết quả I2cBus sang kết quả công khai của SSD1306. */
static Ssd1306_Result_t ssd1306_map_i2c_result(I2cBus_Result_t i2c_result)
{
    switch (i2c_result)
    {
        case I2C_BUS_RESULT_TIMEOUT:
            return SSD1306_RESULT_I2C_TIMEOUT;

        case I2C_BUS_RESULT_ABORT_FAILED:
            return SSD1306_RESULT_I2C_ABORT_FAILED;

        case I2C_BUS_RESULT_SUCCESS:
            return SSD1306_RESULT_SUCCESS;

        case I2C_BUS_RESULT_HAL_ERROR:
        case I2C_BUS_RESULT_NACK:
        case I2C_BUS_RESULT_NONE:
        default:
            return SSD1306_RESULT_I2C_ERROR;
    }
}

/** @brief Dừng driver khi một bước init hoặc refresh thất bại. */
static void ssd1306_enter_error(Ssd1306_t *display,
                                Ssd1306_Result_t result,
                                bool failure_belongs_to_refresh,
                                uint32_t current_tick_ms)
{
    display->status.is_ready = false;
    display->status.has_error = true;
    display->status.framebuffer_is_locked = false;
    display->status.refresh_is_pending = false;
    display->status.refresh_is_running = false;

    if (failure_belongs_to_refresh)
    {
        display->status.last_refresh_result = result;
        display->refresh_result_is_new = true;
    }
    else
    {
        display->status.initialization_result = result;
    }

    ssd1306_enter_state(display, SSD1306_STATE_ERROR, current_tick_ms);
}

/** @brief Thêm một byte vào command buffer có sức chứa cố định. */
static bool ssd1306_append_command_byte(Ssd1306_t *display, uint8_t value)
{
    if (display->command_length >= SSD1306_COMMAND_BUFFER_SIZE_BYTES)
    {
        return false;
    }

    display->command_buffer[display->command_length] = value;
    display->command_length++;
    return true;
}

/** @brief Tạo gói command cấu hình SSD1306 128x64 dùng charge pump nội. */
static bool ssd1306_prepare_initialization_commands(Ssd1306_t *display)
{
    uint8_t segment_command;
    uint8_t com_scan_command;

    if (display->config.orientation == SSD1306_ORIENTATION_0_DEGREES)
    {
        segment_command = SSD1306_CMD_SEGMENT_REMAP;
        com_scan_command = SSD1306_CMD_COM_SCAN_REVERSE;
    }
    else
    {
        segment_command = SSD1306_CMD_SEGMENT_NORMAL;
        com_scan_command = SSD1306_CMD_COM_SCAN_NORMAL;
    }

    display->command_length = 0U;

    return
        ssd1306_append_command_byte(display, SSD1306_CONTROL_COMMAND) &&
        ssd1306_append_command_byte(display, SSD1306_CMD_DISPLAY_OFF) &&
        ssd1306_append_command_byte(display, SSD1306_CMD_SET_DISPLAY_CLOCK) &&
        ssd1306_append_command_byte(display, SSD1306_DISPLAY_CLOCK_DEFAULT) &&
        ssd1306_append_command_byte(display, SSD1306_CMD_SET_MULTIPLEX_RATIO) &&
        ssd1306_append_command_byte(display, SSD1306_MULTIPLEX_64_ROWS) &&
        ssd1306_append_command_byte(display, SSD1306_CMD_SET_DISPLAY_OFFSET) &&
        ssd1306_append_command_byte(display, SSD1306_DISPLAY_OFFSET_ZERO) &&
        ssd1306_append_command_byte(display, SSD1306_CMD_SET_START_LINE_0) &&
        ssd1306_append_command_byte(display, SSD1306_CMD_SET_CHARGE_PUMP) &&
        ssd1306_append_command_byte(display, SSD1306_CHARGE_PUMP_ENABLE) &&
        ssd1306_append_command_byte(display, SSD1306_CMD_SET_MEMORY_ADDRESSING_MODE) &&
        ssd1306_append_command_byte(display, SSD1306_ADDRESSING_MODE_HORIZONTAL) &&
        ssd1306_append_command_byte(display, segment_command) &&
        ssd1306_append_command_byte(display, com_scan_command) &&
        ssd1306_append_command_byte(display, SSD1306_CMD_SET_COM_PINS) &&
        ssd1306_append_command_byte(display, SSD1306_COM_PINS_128X64) &&
        ssd1306_append_command_byte(display, SSD1306_CMD_SET_CONTRAST) &&
        ssd1306_append_command_byte(display, display->config.contrast) &&
        ssd1306_append_command_byte(display, SSD1306_CMD_SET_PRECHARGE) &&
        ssd1306_append_command_byte(display, SSD1306_PRECHARGE_INTERNAL_VCC) &&
        ssd1306_append_command_byte(display, SSD1306_CMD_SET_VCOMH) &&
        ssd1306_append_command_byte(display, SSD1306_VCOMH_0_77_VCC) &&
        ssd1306_append_command_byte(display, SSD1306_CMD_RESUME_GDDRAM_DISPLAY) &&
        ssd1306_append_command_byte(display, SSD1306_CMD_NORMAL_DISPLAY) &&
        ssd1306_append_command_byte(display, SSD1306_CMD_DEACTIVATE_SCROLL);
}

/** @brief Tạo gói đặt toàn bộ vùng cột/page trước khi truyền framebuffer. */
static void ssd1306_prepare_full_window_commands(Ssd1306_t *display)
{
    display->command_length = 0U;
    (void)ssd1306_append_command_byte(display, SSD1306_CONTROL_COMMAND);
    (void)ssd1306_append_command_byte(display, SSD1306_CMD_SET_COLUMN_ADDRESS);
    (void)ssd1306_append_command_byte(display, SSD1306_FIRST_COLUMN);
    (void)ssd1306_append_command_byte(display, SSD1306_LAST_COLUMN);
    (void)ssd1306_append_command_byte(display, SSD1306_CMD_SET_PAGE_ADDRESS);
    (void)ssd1306_append_command_byte(display, SSD1306_FIRST_PAGE);
    (void)ssd1306_append_command_byte(display, SSD1306_LAST_PAGE);
}

/** @brief Tạo gói command bật panel sau khi GDDRAM đã được xóa. */
static void ssd1306_prepare_display_on_command(Ssd1306_t *display)
{
    display->command_length = 0U;
    (void)ssd1306_append_command_byte(display, SSD1306_CONTROL_COMMAND);
    (void)ssd1306_append_command_byte(display, SSD1306_CMD_DISPLAY_ON);
}

/** @brief Thử bắt đầu truyền một buffer; BUSY của bus chỉ làm state machine chờ. */
static Ssd1306_StartTransferResult_t ssd1306_try_start_transfer(
    Ssd1306_t *display,
    const uint8_t *data,
    uint16_t data_length,
    uint32_t current_tick_ms)
{
    I2cBus_StartResult_t start_result;

    start_result = I2cBus_StartTransmit(display->config.i2c_bus,
                                        display->config.address_7bit,
                                        data,
                                        data_length,
                                        current_tick_ms,
                                        display->config.transfer_timeout_ms);

    if (start_result == I2C_BUS_START_ACCEPTED)
    {
        return SSD1306_START_TRANSFER_STARTED;
    }

    if ((start_result == I2C_BUS_START_BUSY) ||
        (start_result == I2C_BUS_START_RESULT_PENDING))
    {
        return SSD1306_START_TRANSFER_WAIT;
    }

    return SSD1306_START_TRANSFER_FAILED;
}

/** @brief Lấy kết quả gói I2C mà SSD1306 đang sở hữu. */
static Ssd1306_WaitTransferResult_t ssd1306_wait_for_transfer(
    Ssd1306_t *display)
{
    I2cBus_Operation_t operation;
    I2cBus_Result_t i2c_result;

    if (!I2cBus_TakeResult(display->config.i2c_bus,
                           &operation,
                           &i2c_result))
    {
        return SSD1306_WAIT_TRANSFER_PENDING;
    }

    display->status.last_i2c_result = i2c_result;

    if ((operation == I2C_BUS_OPERATION_TRANSMIT) &&
        (i2c_result == I2C_BUS_RESULT_SUCCESS))
    {
        return SSD1306_WAIT_TRANSFER_SUCCESS;
    }

    return SSD1306_WAIT_TRANSFER_FAILED;
}

/** @brief Chuyển lỗi I2C vừa lấy thành lỗi SSD1306 và dừng state machine. */
static void ssd1306_finish_failed_transfer(Ssd1306_t *display,
                                           bool failure_belongs_to_refresh,
                                           uint32_t current_tick_ms)
{
    ssd1306_enter_error(display,
                        ssd1306_map_i2c_result(display->status.last_i2c_result),
                        failure_belongs_to_refresh,
                        current_tick_ms);
}

/** @brief Xử lý một send-state có timeout riêng cho thời gian chờ giành bus. */
static bool ssd1306_bus_wait_has_failed(Ssd1306_t *display,
                                        uint32_t current_tick_ms,
                                        bool failure_belongs_to_refresh)
{
    if (ssd1306_timeout_has_elapsed(current_tick_ms,
                                    display->state_start_tick_ms,
                                    display->config.transfer_timeout_ms))
    {
        ssd1306_enter_error(display,
                            SSD1306_RESULT_BUS_WAIT_TIMEOUT,
                            failure_belongs_to_refresh,
                            current_tick_ms);
        return true;
    }

    return false;
}

Ssd1306_InitializeResult_t Ssd1306_Initialize(Ssd1306_t *display,
                                              const Ssd1306_Config_t *config,
                                              uint32_t current_tick_ms)
{
    uint16_t index;
    I2cBus_Status_t i2c_bus_status;

    if ((display == NULL) ||
        (config == NULL) ||
        (config->i2c_bus == NULL) ||
        ((config->address_7bit != SSD1306_ADDRESS_LOW_7BIT) &&
         (config->address_7bit != SSD1306_ADDRESS_HIGH_7BIT)) ||
        (config->transfer_timeout_ms == 0U) ||
        ((config->orientation != SSD1306_ORIENTATION_0_DEGREES) &&
         (config->orientation != SSD1306_ORIENTATION_180_DEGREES)))
    {
        return SSD1306_INITIALIZE_INVALID_ARGUMENT;
    }

    I2cBus_GetStatus(config->i2c_bus, &i2c_bus_status);
    if (!i2c_bus_status.is_initialized)
    {
        return SSD1306_INITIALIZE_INVALID_ARGUMENT;
    }

    *display = (Ssd1306_t){0};
    display->config = *config;
    display->status.is_initialized = true;
    display->status.address_7bit = config->address_7bit;
    display->frame_packet[0] = SSD1306_CONTROL_GDDRAM_DATA;

    for (index = SSD1306_FRAMEBUFFER_OFFSET;
         index < SSD1306_FRAME_PACKET_SIZE_BYTES;
         index++)
    {
        display->frame_packet[index] = 0U;
    }

    ssd1306_enter_state(display,
                        SSD1306_STATE_INIT_SEND_CONFIG,
                        current_tick_ms);
    return SSD1306_INITIALIZE_ACCEPTED;
}

void Ssd1306_Service(Ssd1306_t *display, uint32_t current_tick_ms)
{
    Ssd1306_StartTransferResult_t start_result;
    Ssd1306_WaitTransferResult_t wait_result;

    if ((display == NULL) || !display->status.is_initialized)
    {
        return;
    }

    switch (display->status.state)
    {
        case SSD1306_STATE_INIT_SEND_CONFIG:
            if (ssd1306_bus_wait_has_failed(display, current_tick_ms, false))
            {
                break;
            }

            if (!ssd1306_prepare_initialization_commands(display))
            {
                ssd1306_enter_error(display,
                                    SSD1306_RESULT_I2C_ERROR,
                                    false,
                                    current_tick_ms);
                break;
            }

            start_result = ssd1306_try_start_transfer(
                display,
                display->command_buffer,
                display->command_length,
                current_tick_ms);

            if (start_result == SSD1306_START_TRANSFER_STARTED)
            {
                ssd1306_enter_state(display,
                                    SSD1306_STATE_INIT_WAIT_CONFIG,
                                    current_tick_ms);
            }
            else if (start_result == SSD1306_START_TRANSFER_FAILED)
            {
                ssd1306_enter_error(display,
                                    SSD1306_RESULT_I2C_ERROR,
                                    false,
                                    current_tick_ms);
            }
            break;

        case SSD1306_STATE_INIT_WAIT_CONFIG:
            wait_result = ssd1306_wait_for_transfer(display);
            if (wait_result == SSD1306_WAIT_TRANSFER_SUCCESS)
            {
                ssd1306_enter_state(display,
                                    SSD1306_STATE_INIT_SEND_WINDOW,
                                    current_tick_ms);
            }
            else if (wait_result == SSD1306_WAIT_TRANSFER_FAILED)
            {
                ssd1306_finish_failed_transfer(display, false, current_tick_ms);
            }
            break;

        case SSD1306_STATE_INIT_SEND_WINDOW:
            if (ssd1306_bus_wait_has_failed(display, current_tick_ms, false))
            {
                break;
            }

            ssd1306_prepare_full_window_commands(display);
            start_result = ssd1306_try_start_transfer(
                display,
                display->command_buffer,
                display->command_length,
                current_tick_ms);

            if (start_result == SSD1306_START_TRANSFER_STARTED)
            {
                ssd1306_enter_state(display,
                                    SSD1306_STATE_INIT_WAIT_WINDOW,
                                    current_tick_ms);
            }
            else if (start_result == SSD1306_START_TRANSFER_FAILED)
            {
                ssd1306_enter_error(display,
                                    SSD1306_RESULT_I2C_ERROR,
                                    false,
                                    current_tick_ms);
            }
            break;

        case SSD1306_STATE_INIT_WAIT_WINDOW:
            wait_result = ssd1306_wait_for_transfer(display);
            if (wait_result == SSD1306_WAIT_TRANSFER_SUCCESS)
            {
                ssd1306_enter_state(display,
                                    SSD1306_STATE_INIT_SEND_CLEAR,
                                    current_tick_ms);
            }
            else if (wait_result == SSD1306_WAIT_TRANSFER_FAILED)
            {
                ssd1306_finish_failed_transfer(display, false, current_tick_ms);
            }
            break;

        case SSD1306_STATE_INIT_SEND_CLEAR:
            if (ssd1306_bus_wait_has_failed(display, current_tick_ms, false))
            {
                break;
            }

            start_result = ssd1306_try_start_transfer(
                display,
                display->frame_packet,
                SSD1306_FRAME_PACKET_SIZE_BYTES,
                current_tick_ms);

            if (start_result == SSD1306_START_TRANSFER_STARTED)
            {
                display->status.framebuffer_is_locked = true;
                ssd1306_enter_state(display,
                                    SSD1306_STATE_INIT_WAIT_CLEAR,
                                    current_tick_ms);
            }
            else if (start_result == SSD1306_START_TRANSFER_FAILED)
            {
                ssd1306_enter_error(display,
                                    SSD1306_RESULT_I2C_ERROR,
                                    false,
                                    current_tick_ms);
            }
            break;

        case SSD1306_STATE_INIT_WAIT_CLEAR:
            wait_result = ssd1306_wait_for_transfer(display);
            if (wait_result == SSD1306_WAIT_TRANSFER_SUCCESS)
            {
                display->status.framebuffer_is_locked = false;
                ssd1306_enter_state(display,
                                    SSD1306_STATE_INIT_SEND_DISPLAY_ON,
                                    current_tick_ms);
            }
            else if (wait_result == SSD1306_WAIT_TRANSFER_FAILED)
            {
                ssd1306_finish_failed_transfer(display, false, current_tick_ms);
            }
            break;

        case SSD1306_STATE_INIT_SEND_DISPLAY_ON:
            if (ssd1306_bus_wait_has_failed(display, current_tick_ms, false))
            {
                break;
            }

            ssd1306_prepare_display_on_command(display);
            start_result = ssd1306_try_start_transfer(
                display,
                display->command_buffer,
                display->command_length,
                current_tick_ms);

            if (start_result == SSD1306_START_TRANSFER_STARTED)
            {
                ssd1306_enter_state(display,
                                    SSD1306_STATE_INIT_WAIT_DISPLAY_ON,
                                    current_tick_ms);
            }
            else if (start_result == SSD1306_START_TRANSFER_FAILED)
            {
                ssd1306_enter_error(display,
                                    SSD1306_RESULT_I2C_ERROR,
                                    false,
                                    current_tick_ms);
            }
            break;

        case SSD1306_STATE_INIT_WAIT_DISPLAY_ON:
            wait_result = ssd1306_wait_for_transfer(display);
            if (wait_result == SSD1306_WAIT_TRANSFER_SUCCESS)
            {
                display->status.is_ready = true;
                display->status.initialization_result = SSD1306_RESULT_SUCCESS;
                display->status.framebuffer_is_dirty = false;
                ssd1306_enter_state(display,
                                    SSD1306_STATE_READY,
                                    current_tick_ms);
            }
            else if (wait_result == SSD1306_WAIT_TRANSFER_FAILED)
            {
                ssd1306_finish_failed_transfer(display, false, current_tick_ms);
            }
            break;

        case SSD1306_STATE_READY:
            if (display->status.refresh_is_pending)
            {
                display->status.refresh_is_pending = false;
                display->status.refresh_is_running = true;
                display->status.last_refresh_result = SSD1306_RESULT_NONE;
                display->refresh_result_is_new = false;
                ssd1306_enter_state(display,
                                    SSD1306_STATE_REFRESH_SEND_WINDOW,
                                    current_tick_ms);
            }
            break;

        case SSD1306_STATE_REFRESH_SEND_WINDOW:
            if (ssd1306_bus_wait_has_failed(display, current_tick_ms, true))
            {
                break;
            }

            ssd1306_prepare_full_window_commands(display);
            start_result = ssd1306_try_start_transfer(
                display,
                display->command_buffer,
                display->command_length,
                current_tick_ms);

            if (start_result == SSD1306_START_TRANSFER_STARTED)
            {
                ssd1306_enter_state(display,
                                    SSD1306_STATE_REFRESH_WAIT_WINDOW,
                                    current_tick_ms);
            }
            else if (start_result == SSD1306_START_TRANSFER_FAILED)
            {
                ssd1306_enter_error(display,
                                    SSD1306_RESULT_I2C_ERROR,
                                    true,
                                    current_tick_ms);
            }
            break;

        case SSD1306_STATE_REFRESH_WAIT_WINDOW:
            wait_result = ssd1306_wait_for_transfer(display);
            if (wait_result == SSD1306_WAIT_TRANSFER_SUCCESS)
            {
                ssd1306_enter_state(display,
                                    SSD1306_STATE_REFRESH_SEND_DATA,
                                    current_tick_ms);
            }
            else if (wait_result == SSD1306_WAIT_TRANSFER_FAILED)
            {
                ssd1306_finish_failed_transfer(display, true, current_tick_ms);
            }
            break;

        case SSD1306_STATE_REFRESH_SEND_DATA:
            if (ssd1306_bus_wait_has_failed(display, current_tick_ms, true))
            {
                break;
            }

            start_result = ssd1306_try_start_transfer(
                display,
                display->frame_packet,
                SSD1306_FRAME_PACKET_SIZE_BYTES,
                current_tick_ms);

            if (start_result == SSD1306_START_TRANSFER_STARTED)
            {
                display->status.framebuffer_is_locked = true;
                ssd1306_enter_state(display,
                                    SSD1306_STATE_REFRESH_WAIT_DATA,
                                    current_tick_ms);
            }
            else if (start_result == SSD1306_START_TRANSFER_FAILED)
            {
                ssd1306_enter_error(display,
                                    SSD1306_RESULT_I2C_ERROR,
                                    true,
                                    current_tick_ms);
            }
            break;

        case SSD1306_STATE_REFRESH_WAIT_DATA:
            wait_result = ssd1306_wait_for_transfer(display);
            if (wait_result == SSD1306_WAIT_TRANSFER_SUCCESS)
            {
                display->status.framebuffer_is_locked = false;
                display->status.framebuffer_is_dirty = false;
                display->status.refresh_is_running = false;
                display->status.last_refresh_result = SSD1306_RESULT_SUCCESS;
                display->refresh_result_is_new = true;
                ssd1306_enter_state(display,
                                    SSD1306_STATE_READY,
                                    current_tick_ms);
            }
            else if (wait_result == SSD1306_WAIT_TRANSFER_FAILED)
            {
                ssd1306_finish_failed_transfer(display, true, current_tick_ms);
            }
            break;

        case SSD1306_STATE_UNINITIALIZED:
        case SSD1306_STATE_ERROR:
        default:
            break;
    }
}

bool Ssd1306_Clear(Ssd1306_t *display)
{
    return Ssd1306_Fill(display, SSD1306_PIXEL_OFF);
}

bool Ssd1306_Fill(Ssd1306_t *display,
                  Ssd1306_PixelOperation_t operation)
{
    uint16_t index;
    uint8_t new_value;
    bool changed = false;

    if ((display == NULL) ||
        !display->status.is_ready ||
        display->status.framebuffer_is_locked ||
        !ssd1306_pixel_operation_is_valid(operation))
    {
        return false;
    }

    for (index = SSD1306_FRAMEBUFFER_OFFSET;
         index < SSD1306_FRAME_PACKET_SIZE_BYTES;
         index++)
    {
        if (operation == SSD1306_PIXEL_OFF)
        {
            new_value = 0x00U;
        }
        else if (operation == SSD1306_PIXEL_ON)
        {
            new_value = 0xFFU;
        }
        else
        {
            new_value = (uint8_t)~display->frame_packet[index];
        }

        if (display->frame_packet[index] != new_value)
        {
            display->frame_packet[index] = new_value;
            changed = true;
        }
    }

    if (changed)
    {
        display->status.framebuffer_is_dirty = true;
    }

    return true;
}

bool Ssd1306_SetPixel(Ssd1306_t *display,
                      uint8_t x,
                      uint8_t y,
                      Ssd1306_PixelOperation_t operation)
{
    uint16_t framebuffer_index;
    uint8_t pixel_mask;
    uint8_t old_value;

    if ((display == NULL) ||
        !display->status.is_ready ||
        display->status.framebuffer_is_locked ||
        (x >= SSD1306_WIDTH_PIXELS) ||
        (y >= SSD1306_HEIGHT_PIXELS) ||
        !ssd1306_pixel_operation_is_valid(operation))
    {
        return false;
    }

    framebuffer_index = (uint16_t)(SSD1306_FRAMEBUFFER_OFFSET +
                                   x +
                                   (((uint16_t)y / 8U) * SSD1306_WIDTH_PIXELS));
    pixel_mask = (uint8_t)(1U << (y % 8U));
    old_value = display->frame_packet[framebuffer_index];

    if (operation == SSD1306_PIXEL_OFF)
    {
        display->frame_packet[framebuffer_index] =
            (uint8_t)(old_value & (uint8_t)~pixel_mask);
    }
    else if (operation == SSD1306_PIXEL_ON)
    {
        display->frame_packet[framebuffer_index] =
            (uint8_t)(old_value | pixel_mask);
    }
    else
    {
        display->frame_packet[framebuffer_index] =
            (uint8_t)(old_value ^ pixel_mask);
    }

    if (display->frame_packet[framebuffer_index] != old_value)
    {
        display->status.framebuffer_is_dirty = true;
    }

    return true;
}

bool Ssd1306_GetPixel(const Ssd1306_t *display,
                      uint8_t x,
                      uint8_t y,
                      bool *pixel_is_on)
{
    uint16_t framebuffer_index;
    uint8_t pixel_mask;

    if ((display == NULL) ||
        (pixel_is_on == NULL) ||
        !display->status.is_ready ||
        (x >= SSD1306_WIDTH_PIXELS) ||
        (y >= SSD1306_HEIGHT_PIXELS))
    {
        return false;
    }

    framebuffer_index = (uint16_t)(SSD1306_FRAMEBUFFER_OFFSET +
                                   x +
                                   (((uint16_t)y / 8U) * SSD1306_WIDTH_PIXELS));
    pixel_mask = (uint8_t)(1U << (y % 8U));
    *pixel_is_on = ((display->frame_packet[framebuffer_index] & pixel_mask) != 0U);
    return true;
}

Ssd1306_RefreshRequestResult_t Ssd1306_RequestRefresh(Ssd1306_t *display)
{
    if ((display == NULL) ||
        !display->status.is_ready ||
        display->status.has_error)
    {
        return SSD1306_REFRESH_NOT_READY;
    }

    if (display->status.refresh_is_pending ||
        display->status.refresh_is_running)
    {
        return SSD1306_REFRESH_ALREADY_PENDING;
    }

    if (display->refresh_result_is_new)
    {
        return SSD1306_REFRESH_RESULT_PENDING;
    }

    if (!display->status.framebuffer_is_dirty)
    {
        return SSD1306_REFRESH_NO_CHANGES;
    }

    display->status.refresh_is_pending = true;
    return SSD1306_REFRESH_ACCEPTED;
}

bool Ssd1306_TakeRefreshResult(Ssd1306_t *display,
                              Ssd1306_Result_t *result)
{
    if ((display == NULL) ||
        (result == NULL) ||
        !display->refresh_result_is_new)
    {
        return false;
    }

    *result = display->status.last_refresh_result;
    display->refresh_result_is_new = false;
    return true;
}

void Ssd1306_GetStatus(const Ssd1306_t *display,
                       Ssd1306_Status_t *output_status)
{
    if ((display != NULL) && (output_status != NULL))
    {
        *output_status = display->status;
    }
}
