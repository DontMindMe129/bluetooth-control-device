/**
 * @file app_oled_manager.h
 * @brief Quản lý vòng đời OLED SSD1306, framebuffer và chính sách phục hồi giao tiếp.
 *
 * Module sở hữu driver SSD1306 và canvas đồ họa nhưng không sở hữu bus I2C vật lý,
 * bộ bus-clear, ADXL345, UART hay nội dung từng page. Caller tiếp tục service I2C bus,
 * thực thi bus recovery được yêu cầu và cung cấp callback render giao diện.
 */

#ifndef USER_APP_APP_OLED_MANAGER_H_
#define USER_APP_APP_OLED_MANAGER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "mono_graphics.h"
#include "ssd1306.h"

/** @brief Các bước vòng đời OLED do manager điều phối. */
typedef enum
{
    APP_OLED_MANAGER_NOT_STARTED = 0, /**< Chưa có cấu hình hợp lệ để khởi động. */
    APP_OLED_MANAGER_INITIALIZING, /**< SSD1306 đang chạy chuỗi lệnh khởi tạo. */
    APP_OLED_MANAGER_ACTIVE, /**< OLED sẵn sàng render và refresh framebuffer. */
    APP_OLED_MANAGER_RETRY_WAIT, /**< Chờ trước lần khởi tạo nhanh kế tiếp. */
    APP_OLED_MANAGER_WAITING_BUS_RECOVERY, /**< Đang chờ shared-I2C manager phục hồi bus. */
    APP_OLED_MANAGER_OFFLINE_WAIT, /**< OLED offline; chờ tới lần probe kế tiếp. */
    APP_OLED_MANAGER_OFFLINE_PROBING, /**< Đang chờ kết quả probe địa chỉ OLED. */
    APP_OLED_MANAGER_ERROR /**< Lỗi cấu hình/phần mềm không tự phục hồi. */
} AppOledManager_State_t;

/** @brief Nguyên nhân lỗi gần nhất, kể cả lỗi đang được tự phục hồi. */
typedef enum
{
    APP_OLED_MANAGER_ERROR_NONE = 0, /**< Chưa có lỗi. */
    APP_OLED_MANAGER_ERROR_INVALID_I2C, /**< Không nhận được I2C bus hợp lệ. */
    APP_OLED_MANAGER_ERROR_BUS_INIT, /**< Context I2C bus chưa được khởi tạo. */
    APP_OLED_MANAGER_ERROR_DISPLAY_INIT, /**< SSD1306 không thể bắt đầu/hoàn tất init. */
    APP_OLED_MANAGER_ERROR_DRAW, /**< Callback không thể vẽ frame vào canvas. */
    APP_OLED_MANAGER_ERROR_REFRESH_REQUEST, /**< SSD1306 từ chối yêu cầu refresh. */
    APP_OLED_MANAGER_ERROR_REFRESH_RESULT /**< Refresh đã chạy nhưng kết thúc với lỗi. */
} AppOledManager_Error_t;

/** @brief Kết quả chuẩn hóa mà callback render trả về cho manager. */
typedef enum
{
    APP_OLED_MANAGER_RENDER_IDLE = 0, /**< Page chưa cần vẽ trong vòng hiện tại. */
    APP_OLED_MANAGER_RENDER_DRAWN, /**< Framebuffer vừa được vẽ thành công. */
    APP_OLED_MANAGER_RENDER_FAILED /**< Render thất bại vì dữ liệu hoặc canvas không hợp lệ. */
} AppOledManager_RenderResult_t;

/** @brief Kết quả shared bus recovery do App báo ngược về manager. */
typedef enum
{
    APP_OLED_MANAGER_BUS_RECOVERY_SUCCEEDED = 0,
    APP_OLED_MANAGER_BUS_RECOVERY_FAILED
} AppOledManager_BusRecoveryResult_t;

/** @brief Thông báo one-shot để App log khi trạng thái OLED thay đổi. */
typedef enum
{
    APP_OLED_MANAGER_NOTIFICATION_NONE = 0,
    APP_OLED_MANAGER_NOTIFICATION_ONLINE,
    APP_OLED_MANAGER_NOTIFICATION_RETRY_PENDING,
    APP_OLED_MANAGER_NOTIFICATION_BUS_RECOVERY_PENDING,
    APP_OLED_MANAGER_NOTIFICATION_OFFLINE,
    APP_OLED_MANAGER_NOTIFICATION_PROBE_COULD_NOT_START,
    APP_OLED_MANAGER_NOTIFICATION_PROBE_BUS_ERROR
} AppOledManager_Notification_t;

/** @brief Callback xóa canvas được manager cung cấp cho tầng render. */
typedef bool (*AppOledManager_ClearCanvasFunction_t)(void *context);

/**
 * @brief Callback vẽ nội dung page hiện tại vào canvas do manager sở hữu.
 * @param context Context của tầng render do App cung cấp.
 * @param canvas Canvas đang ánh xạ vào framebuffer SSD1306.
 * @param clear_canvas Primitive xóa framebuffer trước khi vẽ page.
 * @param current_tick_ms HAL tick hiện tại.
 * @return IDLE, DRAWN hoặc FAILED sau lần thử render.
 */
typedef AppOledManager_RenderResult_t
(*AppOledManager_RenderFrameFunction_t)(
    void *context,
    const MonoGraphics_Canvas_t *canvas,
    AppOledManager_ClearCanvasFunction_t clear_canvas,
    uint32_t current_tick_ms);

/** @brief Cấu hình cố định cho vòng đời một OLED. */
typedef struct
{
    I2cBus_t *i2c_bus; /**< Bus dùng chung do App sở hữu và service. */
    uint8_t address_7bit; /**< Địa chỉ SSD1306 dạng 7-bit. */
    uint32_t probe_timeout_ms; /**< Timeout mỗi lần probe OLED. */
    uint32_t transfer_timeout_ms; /**< Timeout mỗi giao dịch SSD1306. */
    uint32_t retry_delay_ms; /**< Thời gian chờ giữa các lần thử nhanh. */
    uint32_t offline_probe_period_ms; /**< Chu kỳ probe khi OLED offline. */
    uint8_t fast_attempt_limit; /**< Số lần thử nhanh trước khi bus-clear. */
    uint8_t nack_offline_threshold; /**< Số NACK liên tiếp trước khi đánh dấu offline. */
    uint8_t contrast; /**< Contrast SSD1306 từ 0 đến 255. */
    Ssd1306_Orientation_t orientation; /**< Hướng ánh xạ vật lý của panel. */
    AppOledManager_RenderFrameFunction_t render_frame; /**< Callback render page hiện tại. */
    void *render_context; /**< Context chuyển nguyên vẹn tới render_frame. */
} AppOledManager_Config_t;

/** @brief Snapshot tập trung để quan sát toàn bộ luồng OLED bằng debugger. */
typedef struct
{
    bool is_initialized; /**< Manager đã nhận cấu hình nội bộ hợp lệ. */
    bool driver_initialized; /**< Context SSD1306 hiện đang hoạt động. */
    bool canvas_initialized; /**< Canvas đã ghép với framebuffer SSD1306. */
    bool redraw_request_pending; /**< App chưa lấy yêu cầu redraw one-shot. */
    bool bus_recovery_request_pending; /**< App chưa lấy yêu cầu bus-clear one-shot. */
    uint8_t address_7bit; /**< Địa chỉ SSD1306 đang sử dụng. */
    uint8_t fast_attempt_count; /**< Số lần thử nhanh trong đợt lỗi hiện tại. */
    uint8_t consecutive_nack_count; /**< Số NACK OLED liên tiếp. */
    AppOledManager_State_t state; /**< Bước hiện tại của state machine OLED. */
    AppOledManager_Error_t error; /**< Nguyên nhân lỗi gần nhất. */
    Ssd1306_InitializeResult_t driver_initialize_result; /**< Kết quả gọi init SSD1306 gần nhất. */
    Ssd1306_Status_t driver; /**< Snapshot chi tiết của driver SSD1306. */
    AppOledManager_Notification_t last_notification; /**< Thông báo đổi trạng thái gần nhất. */
} AppOledManager_Status_t;

/** @brief Context tĩnh sở hữu SSD1306, canvas và state machine vòng đời OLED. */
typedef struct
{
    AppOledManager_Config_t config;
    Ssd1306_t display;
    MonoGraphics_Canvas_t canvas;
    AppOledManager_Status_t status;
    uint32_t state_start_tick_ms;
    uint16_t pending_notification_mask;
} AppOledManager_t;

/**
 * @brief Khởi tạo canvas và bắt đầu init SSD1306 nếu bus sẵn sàng.
 * @param manager Context do caller cấp phát tĩnh.
 * @param config Cấu hình bus, timeout, retry và callback render.
 * @param current_tick_ms HAL tick hiện tại.
 * @return true khi manager bắt đầu được luồng OLED; false khi cấu hình không hợp lệ.
 */
bool AppOledManager_Initialize(AppOledManager_t *manager,
                               const AppOledManager_Config_t *config,
                               uint32_t current_tick_ms);

/**
 * @brief Tiến state machine OLED đúng một bước, không blocking.
 * @note Caller phải service I2cBus trước hàm này trong mỗi vòng superloop.
 */
void AppOledManager_Service(AppOledManager_t *manager,
                            uint32_t current_tick_ms);

/** @brief Lấy một lần yêu cầu App thực thi bus-clear vật lý. */
bool AppOledManager_TakeBusRecoveryRequest(AppOledManager_t *manager);

/** @brief Báo kết quả bus-clear và tạo lại shared I2C bus cho manager. */
void AppOledManager_ReportBusRecoveryResult(
    AppOledManager_t *manager,
    AppOledManager_BusRecoveryResult_t result,
    uint32_t current_tick_ms);

/** @brief Lấy một lần yêu cầu vẽ lại page hiện tại sau init/lỗi refresh. */
bool AppOledManager_TakeRedrawRequest(AppOledManager_t *manager);

/** @brief Lấy lần lượt các thông báo đổi trạng thái chưa được App xử lý. */
bool AppOledManager_TakeNotification(
    AppOledManager_t *manager,
    AppOledManager_Notification_t *notification);

/** @brief true khi OLED đang chờ kết quả phục hồi từ shared-I2C manager. */
bool AppOledManager_IsWaitingForBusRecovery(
    const AppOledManager_t *manager);

/** @brief Sao chép snapshot manager để App hoặc debugger quan sát. */
void AppOledManager_GetStatus(const AppOledManager_t *manager,
                              AppOledManager_Status_t *output_status);

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_APP_OLED_MANAGER_H_ */
