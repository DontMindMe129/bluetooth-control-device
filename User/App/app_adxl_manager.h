/**
 * @file app_adxl_manager.h
 * @brief Quản lý vòng đời, stale và tự kết nối lại cảm biến ADXL345.
 */

#ifndef USER_APP_APP_ADXL_MANAGER_H_
#define USER_APP_APP_ADXL_MANAGER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "adxl345.h"

/** @brief State machine cấp App bao quanh driver ADXL345. */
typedef enum
{
    APP_ADXL_MANAGER_NOT_STARTED = 0,
    APP_ADXL_MANAGER_INITIALIZING,
    APP_ADXL_MANAGER_CONFIRMING,
    APP_ADXL_MANAGER_ACTIVE,
    APP_ADXL_MANAGER_RESTART_WAIT,
    APP_ADXL_MANAGER_OFFLINE_WAIT,
    APP_ADXL_MANAGER_WAITING_BUS_RECOVERY,
    APP_ADXL_MANAGER_ERROR
} AppAdxlManager_State_t;

/** @brief Lý do manager gần nhất chủ động restart cảm biến. */
typedef enum
{
    APP_ADXL_MANAGER_RESTART_NONE = 0,
    APP_ADXL_MANAGER_RESTART_READ_ERRORS,
    APP_ADXL_MANAGER_RESTART_STALE,
    APP_ADXL_MANAGER_RESTART_SHARED_BUS
} AppAdxlManager_RestartReason_t;

/** @brief Cấu hình driver và policy tự phục hồi riêng của ADXL345. */
typedef struct
{
    I2cBus_t *i2c_bus;
    uint8_t address_7bit;
    uint16_t data_ready_pin;
    uint32_t transfer_timeout_ms;
    uint32_t driver_initialize_retry_delay_ms;
    uint32_t driver_stale_timeout_ms;
    uint8_t driver_initialize_max_attempts;
    uint32_t persistent_stale_restart_ms;
    uint32_t device_restart_delay_ms;
    uint32_t offline_probe_period_ms;
    uint8_t consecutive_read_error_limit;
    uint8_t recovery_good_sample_count;
} AppAdxlManager_Config_t;

/** @brief Snapshot vòng đời ADXL345 và driver bên dưới. */
typedef struct
{
    bool is_initialized;
    bool is_online;
    bool bus_recovery_request_pending;
    uint8_t consecutive_read_error_count;
    uint8_t good_sample_count;
    uint32_t device_restart_count;
    AppAdxlManager_State_t state;
    AppAdxlManager_RestartReason_t last_restart_reason;
    Adxl345_InitializeResult_t driver_initialize_result;
    Adxl345_Status_t driver;
} AppAdxlManager_Status_t;

/** @brief Context tĩnh sở hữu driver và bộ đếm phục hồi ADXL345. */
typedef struct
{
    AppAdxlManager_Config_t config;
    Adxl345_t sensor;
    AppAdxlManager_Status_t status;
    uint32_t state_start_tick_ms;
    uint32_t previous_successful_sample_count;
    uint32_t previous_failed_sample_count;
} AppAdxlManager_t;

/** @brief Khởi tạo manager và bắt đầu state machine driver ADXL345. */
bool AppAdxlManager_Initialize(AppAdxlManager_t *manager,
                               const AppAdxlManager_Config_t *config,
                               uint32_t current_tick_ms);

/** @brief Tiến driver và policy phục hồi đúng một bước, không blocking. */
void AppAdxlManager_Service(AppAdxlManager_t *manager,
                            uint32_t current_tick_ms);

/** @brief Lấy một mẫu mới từ driver được sở hữu đúng một lần. */
bool AppAdxlManager_TakeNewSample(AppAdxlManager_t *manager,
                                  Adxl345_Sample_t *output_sample);

/** @brief Chuyển tiếp EXTI DATA_READY tới driver được sở hữu. */
void AppAdxlManager_HandleDataReadyInterrupt(AppAdxlManager_t *manager,
                                             uint16_t gpio_pin);

/** @brief Lấy một lần yêu cầu phục hồi shared I2C bus. */
bool AppAdxlManager_TakeBusRecoveryRequest(AppAdxlManager_t *manager);

/** @brief Báo kết quả shared bus recovery để manager khởi tạo lại cảm biến. */
void AppAdxlManager_ReportBusRecoveryResult(AppAdxlManager_t *manager,
                                             bool recovery_succeeded,
                                             uint32_t current_tick_ms);

/** @brief Sao chép snapshot manager và driver ADXL345. */
void AppAdxlManager_GetStatus(const AppAdxlManager_t *manager,
                              AppAdxlManager_Status_t *output_status);

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_APP_ADXL_MANAGER_H_ */
