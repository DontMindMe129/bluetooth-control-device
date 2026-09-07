/**
 * @file app_shared_i2c_manager.h
 * @brief Quản lý I2C bus dùng chung và bus-clear cho nhiều device manager.
 */

#ifndef USER_APP_APP_SHARED_I2C_MANAGER_H_
#define USER_APP_APP_SHARED_I2C_MANAGER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "i2c_bus.h"
#include "i2c_bus_recovery.h"

/** @brief Nguồn yêu cầu phục hồi I2C1; có thể OR nhiều giá trị. */
typedef enum
{
    APP_SHARED_I2C_RECOVERY_SOURCE_NONE = 0U,
    APP_SHARED_I2C_RECOVERY_SOURCE_OLED = (1U << 0),
    APP_SHARED_I2C_RECOVERY_SOURCE_ADXL345 = (1U << 1)
} AppSharedI2cManager_RecoverySource_t;

/** @brief State machine của bus I2C dùng chung. */
typedef enum
{
    APP_SHARED_I2C_NOT_STARTED = 0,
    APP_SHARED_I2C_READY,
    APP_SHARED_I2C_RECOVERY_WAIT,
    APP_SHARED_I2C_RECOVERING,
    APP_SHARED_I2C_ERROR
} AppSharedI2cManager_State_t;

/** @brief Lỗi cấu hình hoặc phục hồi gần nhất của shared bus. */
typedef enum
{
    APP_SHARED_I2C_ERROR_NONE = 0,
    APP_SHARED_I2C_ERROR_INVALID_CONFIG,
    APP_SHARED_I2C_ERROR_BUS_INIT,
    APP_SHARED_I2C_ERROR_RECOVERY_FAILED
} AppSharedI2cManager_Error_t;

/** @brief Kết quả one-shot sau một đợt bus recovery. */
typedef enum
{
    APP_SHARED_I2C_RECOVERY_SUCCEEDED = 0,
    APP_SHARED_I2C_RECOVERY_FAILED
} AppSharedI2cManager_RecoveryResult_t;

/** @brief Thông báo one-shot để App log đúng lúc state thay đổi. */
typedef enum
{
    APP_SHARED_I2C_NOTIFICATION_NONE = 0,
    APP_SHARED_I2C_NOTIFICATION_RECOVERY_PENDING,
    APP_SHARED_I2C_NOTIFICATION_RECOVERY_STARTED,
    APP_SHARED_I2C_NOTIFICATION_RECOVERY_RETRY_PENDING,
    APP_SHARED_I2C_NOTIFICATION_RECOVERY_SUCCEEDED,
    APP_SHARED_I2C_NOTIFICATION_RECOVERY_FAILED
} AppSharedI2cManager_Notification_t;

/** @brief Cấu hình phần cứng và policy phục hồi của một bus dùng chung. */
typedef struct
{
    I2C_HandleTypeDef *hal_i2c;
    GPIO_TypeDef *scl_port;
    uint16_t scl_pin;
    GPIO_TypeDef *sda_port;
    uint16_t sda_pin;
    uint32_t recovery_delay_ms;
    uint8_t recovery_attempt_limit;
} AppSharedI2cManager_Config_t;

/** @brief Snapshot tập trung của bus và bộ bus-clear. */
typedef struct
{
    bool is_initialized;
    bool bus_is_initialized;
    bool recovery_result_pending;
    uint8_t requested_source_mask;
    uint8_t completed_source_mask;
    uint8_t recovery_attempt_count;
    AppSharedI2cManager_State_t state;
    AppSharedI2cManager_Error_t error;
    AppSharedI2cManager_RecoveryResult_t last_recovery_result;
    AppSharedI2cManager_Notification_t last_notification;
    I2cBus_Status_t bus;
    I2cBusRecovery_State_t physical_recovery_state;
} AppSharedI2cManager_Status_t;

/** @brief Context tĩnh sở hữu I2cBus và state machine bus-clear. */
typedef struct
{
    AppSharedI2cManager_Config_t config;
    I2cBus_t bus;
    I2cBusRecovery_t physical_recovery;
    AppSharedI2cManager_Status_t status;
    uint32_t state_start_tick_ms;
    uint16_t pending_notification_mask;
} AppSharedI2cManager_t;

/** @brief Khởi tạo shared bus và context bus-clear nhưng chưa thay đổi chân I2C. */
bool AppSharedI2cManager_Initialize(
    AppSharedI2cManager_t *manager,
    const AppSharedI2cManager_Config_t *config);

/** @brief Tiến I2C bus hoặc bus recovery đúng một bước, không blocking. */
void AppSharedI2cManager_Service(AppSharedI2cManager_t *manager,
                                 uint32_t current_tick_ms);

/** @brief Gộp một yêu cầu phục hồi từ OLED hoặc ADXL345. */
bool AppSharedI2cManager_RequestRecovery(
    AppSharedI2cManager_t *manager,
    uint8_t source_mask,
    uint32_t current_tick_ms);

/** @brief Lấy kết quả cuối của một đợt recovery đúng một lần. */
bool AppSharedI2cManager_TakeRecoveryResult(
    AppSharedI2cManager_t *manager,
    AppSharedI2cManager_RecoveryResult_t *result,
    uint8_t *source_mask);

/** @brief Lấy lần lượt notification chưa được App xử lý. */
bool AppSharedI2cManager_TakeNotification(
    AppSharedI2cManager_t *manager,
    AppSharedI2cManager_Notification_t *notification);

/** @brief true trong cả khoảng chờ và lúc bus-clear đang điều khiển chân I2C. */
bool AppSharedI2cManager_IsRecoveryActive(
    const AppSharedI2cManager_t *manager);

/** @brief Trả con trỏ bus dùng cho device driver; NULL nếu manager chưa sẵn sàng. */
I2cBus_t *AppSharedI2cManager_GetBus(AppSharedI2cManager_t *manager);

/** @brief Sao chép snapshot phục vụ App và debugger. */
void AppSharedI2cManager_GetStatus(
    const AppSharedI2cManager_t *manager,
    AppSharedI2cManager_Status_t *output_status);

/** @brief Chuyển tiếp callback master transmit complete tới bus được sở hữu. */
void AppSharedI2cManager_HandleMasterTransmitCompleteInterrupt(
    AppSharedI2cManager_t *manager,
    I2C_HandleTypeDef *hal_i2c);

/** @brief Chuyển tiếp callback memory read complete tới bus được sở hữu. */
void AppSharedI2cManager_HandleMemoryReadCompleteInterrupt(
    AppSharedI2cManager_t *manager,
    I2C_HandleTypeDef *hal_i2c);

/** @brief Chuyển tiếp callback lỗi I2C tới bus được sở hữu. */
void AppSharedI2cManager_HandleErrorInterrupt(
    AppSharedI2cManager_t *manager,
    I2C_HandleTypeDef *hal_i2c);

/** @brief Chuyển tiếp callback abort complete tới bus được sở hữu. */
void AppSharedI2cManager_HandleAbortCompleteInterrupt(
    AppSharedI2cManager_t *manager,
    I2C_HandleTypeDef *hal_i2c);

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_APP_SHARED_I2C_MANAGER_H_ */
