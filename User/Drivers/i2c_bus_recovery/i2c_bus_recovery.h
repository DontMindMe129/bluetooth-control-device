/**
 * @file i2c_bus_recovery.h
 * @brief Khôi phục bus I2C bị giữ thấp bằng GPIO và state machine không blocking.
 */
#ifndef USER_DRIVERS_I2C_BUS_RECOVERY_I2C_BUS_RECOVERY_H_
#define USER_DRIVERS_I2C_BUS_RECOVERY_I2C_BUS_RECOVERY_H_

#include <stdbool.h>
#include <stdint.h>
#include "stm32f1xx_hal.h"

/** @brief Trạng thái quan sát được của một lần bus-clear. */
typedef enum
{
    I2C_BUS_RECOVERY_IDLE = 0,
    I2C_BUS_RECOVERY_RELEASING_LINES,
    I2C_BUS_RECOVERY_CLOCK_LOW,
    I2C_BUS_RECOVERY_CLOCK_HIGH,
    I2C_BUS_RECOVERY_STOP_LOW,
    I2C_BUS_RECOVERY_STOP_CLOCK_HIGH,
    I2C_BUS_RECOVERY_STOP_DATA_HIGH,
    I2C_BUS_RECOVERY_SUCCEEDED,
    I2C_BUS_RECOVERY_FAILED
} I2cBusRecovery_State_t;

/** @brief Cấu hình phần cứng của bus cần khôi phục. */
typedef struct
{
    I2C_HandleTypeDef *hal_i2c; /**< HAL handle sẽ được DeInit/Init lại. */
    GPIO_TypeDef *scl_port;     /**< GPIO port của SCL. */
    uint16_t scl_pin;           /**< GPIO pin của SCL. */
    GPIO_TypeDef *sda_port;     /**< GPIO port của SDA. */
    uint16_t sda_pin;           /**< GPIO pin của SDA. */
} I2cBusRecovery_Config_t;

/** @brief Context tĩnh của state machine bus-clear. */
typedef struct
{
    I2cBusRecovery_Config_t config;
    I2cBusRecovery_State_t state;
    uint32_t state_start_tick_ms;
    uint8_t clock_pulse_count;
    bool lines_were_released;
} I2cBusRecovery_t;

/** @brief Khởi tạo context; không thay đổi phần cứng. */
bool I2cBusRecovery_Initialize(I2cBusRecovery_t *recovery,
                              const I2cBusRecovery_Config_t *config);

/**
 * @brief Bắt đầu một lần bus-clear sau khi không còn giao dịch I2C đang chạy.
 * @return true nếu HAL I2C đã được tạm DeInit và state machine đã bắt đầu.
 */
bool I2cBusRecovery_Start(I2cBusRecovery_t *recovery,
                          uint32_t current_tick_ms);

/** @brief Tiến state machine đúng một bước; mỗi mức GPIO giữ tối thiểu 1 ms. */
void I2cBusRecovery_Service(I2cBusRecovery_t *recovery,
                            uint32_t current_tick_ms);

/** @brief Trả về true khi một lần bus-clear đang chiếm các chân I2C. */
bool I2cBusRecovery_IsRunning(const I2cBusRecovery_t *recovery);

#endif
