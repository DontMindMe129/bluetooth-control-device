/**
 * @file environment_history.h
 * @brief Ring buffer RAM lưu kết quả các giao dịch DHT11 trong phiên giám sát.
 */

#ifndef USER_SERVICES_ENVIRONMENT_HISTORY_ENVIRONMENT_HISTORY_H_
#define USER_SERVICES_ENVIRONMENT_HISTORY_ENVIRONMENT_HISTORY_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "environment_monitor.h"

/** @brief Số record gần nhất được giữ trong RAM. */
#define ENVIRONMENT_HISTORY_CAPACITY (256U)

/**
 * @brief Kết quả gọn của đúng một giao dịch DHT11 trong phiên giám sát.
 *
 * @note measured_data, temperature_level và condition chỉ hợp lệ khi
 *       measurement_error bằng DHT11_ERROR_NONE. Khi giao dịch lỗi,
 *       cached_data_state cho biết mẫu hợp lệ gần nhất đang FRESH, STALE hay
 *       chưa từng tồn tại; record không giả định dữ liệu cũ là dữ liệu mới.
 */
typedef struct
{
    uint32_t completed_tick_ms; /**< Tick khi giao dịch DHT11 hoàn tất. */
    DHT11_Data_t measured_data; /**< Bốn byte của chính giao dịch thành công này. */
    uint16_t session_id; /**< Phiên giám sát sở hữu record này. */
    uint16_t consecutive_error_count; /**< Số lỗi liên tiếp sau giao dịch này. */
    uint8_t measurement_error; /**< Mã DHT11_Error_t; NONE nghĩa là thành công. */
    uint8_t cached_data_state; /**< Mã EnvironmentMonitor_DataState_t tại thời điểm ghi. */
    uint8_t temperature_level; /**< Mã EnvironmentMonitor_TemperatureLevel_t của mẫu thành công. */
    uint8_t condition; /**< Mã EnvironmentMonitor_Condition_t của mẫu thành công. */
} EnvironmentHistory_Record_t;

/** @brief Snapshot trạng thái ring buffer dành cho application/debugger. */
typedef struct
{
    bool is_initialized; /**< Vùng lưu trữ đã sẵn sàng. */
    uint16_t count; /**< Số record hợp lệ hiện có. */
    uint16_t write_index; /**< Vị trí vật lý sẽ được ghi kế tiếp. */
    uint32_t total_record_count; /**< Tổng record đã nhận, tăng bão hòa. */
} EnvironmentHistory_Status_t;

/** @brief Context ring buffer tĩnh do caller sở hữu. */
typedef struct
{
    EnvironmentHistory_Record_t records[ENVIRONMENT_HISTORY_CAPACITY]; /**< Vùng lưu record cố định. */
    EnvironmentHistory_Status_t status; /**< Trạng thái quản lý ring buffer hiện tại. */
} EnvironmentHistory_t;

/**
 * @brief Khởi tạo vùng lưu trữ rỗng.
 * @param history Context do caller cấp phát tĩnh.
 * @return true khi context hợp lệ và đã được khởi tạo.
 */
bool EnvironmentHistory_Initialize(EnvironmentHistory_t *history);

/**
 * @brief Ghi kết quả một giao dịch và ghi đè record cũ nhất nếu buffer đã đầy.
 * @param history Context đã khởi tạo.
 * @param measurement_result Kết quả trực tiếp của giao dịch DHT11 vừa hoàn tất.
 * @param session_id Phiên giám sát sở hữu giao dịch; phải khác 0.
 * @param environment Snapshot monitor sau khi đã xử lý chính giao dịch này.
 */
bool EnvironmentHistory_Append(
                               EnvironmentHistory_t *history,
                               const DHT11_MeasurementResult_t *measurement_result,
                               uint16_t session_id,
                               const EnvironmentMonitor_Status_t *environment);

/**
 * @brief Trả về số record hợp lệ đang có.
 * @param history Context nguồn.
 */
uint16_t EnvironmentHistory_GetCount(const EnvironmentHistory_t *history);

/**
 * @brief Đọc record theo thứ tự thời gian; index 0 là record cũ nhất.
 * @param history Context nguồn.
 * @param index Vị trí logic tính từ record cũ nhất.
 * @param output_record Vùng nhớ nhận bản sao record.
 */
bool EnvironmentHistory_GetRecord(const EnvironmentHistory_t *history,
                                  uint16_t index,
                                  EnvironmentHistory_Record_t *output_record);

/**
 * @brief Xóa record nhưng giữ nguyên bộ đếm tổng để phục vụ chẩn đoán.
 * @param history Context cần xóa.
 */
void EnvironmentHistory_Clear(EnvironmentHistory_t *history);

/**
 * @brief Sao chép trạng thái bộ lưu trữ cho application/debugger.
 * @param history Context nguồn.
 * @param output_status Vùng nhớ nhận snapshot; NULL sẽ được bỏ qua.
 */
void EnvironmentHistory_GetStatus(const EnvironmentHistory_t *history,
                                  EnvironmentHistory_Status_t *output_status);

#ifdef __cplusplus
}
#endif

#endif
