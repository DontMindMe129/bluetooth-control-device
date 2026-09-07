/**
 * @file warning_history.h
 * @brief Ring buffer RAM lưu các lần warning bắt đầu, đổi nguồn và kết thúc.
 *
 * Service chạy trong main context, không truy cập HAL và chỉ tạo record khi
 * source mask thay đổi. Vì vậy một warning kéo dài không tạo record lặp lại.
 */

#ifndef USER_SERVICES_WARNING_HISTORY_WARNING_HISTORY_H_
#define USER_SERVICES_WARNING_HISTORY_WARNING_HISTORY_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/** @brief Số event warning gần nhất được giữ trong RAM. */
#define WARNING_HISTORY_CAPACITY (64U)

/** @brief Loại chuyển tiếp của vòng đời warning. */
typedef enum
{
    WARNING_HISTORY_EVENT_STARTED = 0, /**< Source mask chuyển từ 0 sang khác 0. */
    WARNING_HISTORY_EVENT_SOURCE_CHANGED, /**< Hai source mask khác 0 nhưng khác nhau. */
    WARNING_HISTORY_EVENT_CLEARED /**< Source mask chuyển từ khác 0 về 0. */
} WarningHistory_Event_t;

/** @brief Một mốc warning được lưu trong ring buffer. */
typedef struct
{
    uint32_t tick_ms; /**< HAL tick khi App phát hiện source mask thay đổi. */
    WarningHistory_Event_t event; /**< STARTED, SOURCE_CHANGED hoặc CLEARED. */
    uint16_t session_id; /**< Phiên hiện tại; bằng 0 cho manual warning ngoài phiên. */
    uint8_t source_mask; /**< Tổ hợp nguồn warning sau chuyển tiếp. */
} WarningHistory_Record_t;

/** @brief Snapshot trạng thái bộ lưu trữ dành cho App và debugger. */
typedef struct
{
    bool is_initialized; /**< Ring buffer đã được khởi tạo. */
    uint16_t count; /**< Số record hợp lệ hiện có. */
    uint16_t write_index; /**< Vị trí vật lý sẽ được ghi kế tiếp. */
    uint8_t current_source_mask; /**< Source mask đã xử lý gần nhất. */
    uint32_t total_event_count; /**< Tổng event từng ghi, tăng bão hòa. */
    uint32_t overwritten_event_count; /**< Số record cũ đã bị ghi đè. */
} WarningHistory_Status_t;

/** @brief Context ring buffer tĩnh do caller sở hữu. */
typedef struct
{
    WarningHistory_Record_t records[WARNING_HISTORY_CAPACITY]; /**< Vùng lưu record có dung lượng cố định. */
    WarningHistory_Status_t status; /**< Snapshot quản lý ring buffer hiện tại. */
    uint16_t current_warning_session_id; /**< Session gắn với warning đang hoạt động. */
} WarningHistory_t;

/**
 * @brief Khởi tạo ring buffer rỗng với source mask ban đầu bằng 0.
 * @param history Context do caller cấp phát tĩnh.
 * @return true khi context hợp lệ và đã được khởi tạo.
 */
bool WarningHistory_Initialize(WarningHistory_t *history);

/**
 * @brief So sánh source mask và ghi record nếu trạng thái warning thay đổi.
 *
 * @param history Context đã khởi tạo.
 * @param current_tick_ms Tick tại vòng App đang xử lý warning.
 * @param source_mask Tổ hợp nguồn warning hiện tại.
 * @param session_id Phiên hiện tại, hoặc 0 nếu monitoring đang INACTIVE.
 * @param output_record Nếu khác NULL, nhận bản sao record vừa tạo.
 * @return true khi một record mới được tạo; false nếu mask giữ nguyên hoặc chưa init.
 */
bool WarningHistory_Service(WarningHistory_t *history,
                            uint32_t current_tick_ms,
                            uint8_t source_mask,
                            uint16_t session_id,
                            WarningHistory_Record_t *output_record);

/**
 * @brief Trả về số record hợp lệ hiện có.
 * @param history Context nguồn.
 */
uint16_t WarningHistory_GetCount(const WarningHistory_t *history);

/**
 * @brief Đọc record theo thứ tự thời gian; index 0 là record cũ nhất.
 * @param history Context nguồn.
 * @param index Vị trí logic tính từ record cũ nhất.
 * @param output_record Vùng nhớ nhận bản sao record.
 */
bool WarningHistory_GetRecord(const WarningHistory_t *history,
                              uint16_t index,
                              WarningHistory_Record_t *output_record);

/**
 * @brief Xóa record và đưa baseline source mask về trạng thái hiện tại.
 * @param history Context cần xóa.
 * @param current_source_mask Source mask dùng làm baseline mới.
 * @param current_session_id Session gắn với warning đang hoạt động, nếu có.
 */
void WarningHistory_Clear(WarningHistory_t *history,
                          uint8_t current_source_mask,
                          uint16_t current_session_id);

/**
 * @brief Sao chép trạng thái ring buffer cho App hoặc debugger.
 * @param history Context nguồn.
 * @param output_status Vùng nhớ nhận snapshot; NULL sẽ được bỏ qua.
 */
void WarningHistory_GetStatus(const WarningHistory_t *history,
                              WarningHistory_Status_t *output_status);

#ifdef __cplusplus
}
#endif

#endif
