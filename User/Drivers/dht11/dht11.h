/**
 * @file dht11.h
 * @brief Giao diện điều khiển cảm biến nhiệt độ và độ ẩm DHT11.
 *
 * Module triển khai giao tiếp không blocking với một cảm biến DHT11. Main
 * context quản lý chu kỳ đọc, start signal, timeout và checksum; ngắt input
 * capture chỉ thu nhận ACK và 40 bit dữ liệu.
 */

#ifndef USER_DRIVERS_DHT11_DHT11_H_
#define USER_DRIVERS_DHT11_DHT11_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "stm32f1xx_hal.h"

/**
 * @brief Cấu hình phần cứng của một kết nối DHT11.
 *
 * Driver sao chép cấu hình khi khởi tạo và không phụ thuộc trực tiếp vào
 * board_config.h. Port, pin, timer và channel phải được CubeMX cấu hình phù hợp.
 * Clock counter sau prescaler phải từ 1 MHz trở lên và là bội số nguyên của
 * 1 MHz; chu kỳ tràn timer phải dài hơn cửa sổ ACK tối đa 220 microsecond.
 * Với clock TIM3 8 MHz và prescaler 7 của board hiện tại, mỗi count đúng 1 us.
 */
typedef struct
{
    TIM_HandleTypeDef *timer;    /**< Handle timer input capture đã khởi tạo. */
    TIM_TypeDef *timer_instance; /**< Instance timer mong đợi để phát hiện ghép sai handle. */
    uint32_t timer_channel;      /**< Một trong TIM_CHANNEL_1 đến TIM_CHANNEL_4. */
    GPIO_TypeDef *data_port;     /**< GPIO port của đường DATA. */
    uint16_t data_pin;           /**< Một GPIO pin duy nhất của đường DATA. */
} DHT11_Config_t;

/* Kiểu dữ liệu công khai dành cho tầng ứng dụng. */

/**
 * @brief Kết quả trả về khi khởi tạo module DHT11.
 */
typedef enum
{
    DHT11_RESULT_OK = 0,              /**< Khởi tạo module thành công. */
    DHT11_RESULT_INVALID_ARGUMENT,    /**< Cấu hình chứa con trỏ hoặc pin không hợp lệ. */
    DHT11_RESULT_INVALID_CONFIG,      /**< Cấu hình timer không đáp ứng yêu cầu của module. */
    DHT11_RESULT_TIMER_START_FAILED   /**< Không thể khởi động input capture channel. */
} DHT11_Result_t;

/**
 * @brief Trạng thái hiện tại của state machine DHT11.
 */
typedef enum
{
    DHT11_STATE_UNINITIALIZED = 0,       /**< Module chưa được khởi tạo hoặc khởi tạo thất bại. */
    DHT11_STATE_WAIT_PERIOD,             /**< Đang chờ đến chu kỳ đọc tiếp theo. */
    DHT11_STATE_START_LOW,               /**< MCU đang giữ đường DATA ở mức thấp để phát start signal. */
    DHT11_STATE_WAIT_ACK_FIRST_EDGE,     /**< Đang chờ cạnh xuống đầu tiên của ACK. */
    DHT11_STATE_WAIT_ACK_SECOND_EDGE,    /**< Đang chờ cạnh xuống kết thúc chuỗi ACK. */
    DHT11_STATE_RECEIVING_BITS,          /**< Đang nhận 40 bit dữ liệu từ DHT11. */
    DHT11_STATE_FRAME_READY,             /**< ISR đã nhận đủ khung và main chưa xử lý checksum. */
    DHT11_STATE_ERROR                    /**< ISR đã phát hiện lỗi và chờ main thực hiện phục hồi. */
} DHT11_State_t;

/**
 * @brief Mã lỗi gần nhất của module DHT11.
 */
typedef enum
{
    DHT11_ERROR_NONE = 0,             /**< Không có lỗi trong giao dịch gần nhất. */
    DHT11_ERROR_INVALID_CONFIG,       /**< Timer, channel hoặc độ phân giải timer không hợp lệ. */
    DHT11_ERROR_TIMER_START,          /**< HAL không thể khởi động input capture. */
    DHT11_ERROR_BUS_STUCK_LOW,        /**< Đường DATA vẫn thấp trước khi bắt đầu giao dịch. */
    DHT11_ERROR_START_DRIVE,          /**< MCU không kéo được đường DATA xuống thấp. */
    DHT11_ERROR_ACK_TIMEOUT,          /**< Không nhận đủ chuỗi ACK trước khi hết timeout. */
    DHT11_ERROR_ACK_TIMING,           /**< Khoảng thời gian ACK nằm ngoài cửa sổ hợp lệ. */
    DHT11_ERROR_BIT_TIMING,           /**< Khoảng thời gian của một bit nằm ngoài cửa sổ hợp lệ. */
    DHT11_ERROR_CAPTURE_OVERRUN,      /**< Timer ghi đè capture cũ trước khi ISR xử lý xong. */
    DHT11_ERROR_FRAME_TIMEOUT,        /**< Đã bắt đầu nhận nhưng không đủ 40 bit trước timeout. */
    DHT11_ERROR_CHECKSUM,             /**< Byte checksum không khớp tổng bốn byte dữ liệu. */
    DHT11_ERROR_INTERNAL_STATE        /**< Capture xuất hiện tại một trạng thái không hợp lệ. */
} DHT11_Error_t;

/**
 * @brief Bốn byte dữ liệu hợp lệ lấy trực tiếp từ khung DHT11.
 *
 * Module không chuyển đổi các byte này sang số thập phân. Việc biểu diễn hoặc
 * tính toán giá trị nhiệt độ, độ ẩm thuộc tầng sử dụng dữ liệu.
 */
typedef struct
{
    uint8_t humidity_integer;       /**< Phần nguyên của độ ẩm. */
    uint8_t humidity_decimal;       /**< Phần thập phân của độ ẩm. */
    uint8_t temperature_integer;    /**< Phần nguyên của nhiệt độ. */
    uint8_t temperature_decimal;    /**< Phần thập phân của nhiệt độ. */
} DHT11_Data_t;

/**
 * @brief Thông tin trạng thái và chẩn đoán của module DHT11.
 */
typedef struct
{
    DHT11_State_t state;                  /**< Trạng thái state machine tại thời điểm lấy snapshot. */
    DHT11_Error_t last_error;             /**< Lỗi của giao dịch gần nhất; NONE nếu lần gần nhất thành công. */
    uint16_t consecutive_error_count;     /**< Số lỗi liên tiếp, tăng bão hòa tại UINT16_MAX. */
    uint32_t last_success_tick_ms;        /**< HAL tick tại lần cập nhật dữ liệu hợp lệ gần nhất. */
    bool has_latest_valid_data;  /**< true khi module đang lưu một khung đã vượt qua checksum. */
    bool has_unread_data;        /**< true khi khung mới nhất chưa được lấy bằng DHT11_TakeNewData(). */
} DHT11_Status_t;

/* API điều khiển module từ main context. */

/**
 * @brief Khởi tạo module DHT11 và input capture channel.
 *
 * Hàm chỉ khởi động input capture một lần rồi tắt riêng interrupt capture.
 * Timer vẫn tiếp tục chạy để không ảnh hưởng các channel khác dùng chung instance.
 * Lần đọc đầu tiên được thực hiện sau một chu kỳ 2 giây.
 * Driver tự đọc clock, prescaler và ARR lúc khởi tạo để đổi các ngưỡng
 * microsecond sang counter tick; không hard-code prescaler của board.
 *
 * @param config Cấu hình timer, channel và chân DATA của kết nối DHT11.
 * @param current_tick_ms Giá trị HAL tick hiện tại, thường lấy từ HAL_GetTick().
 *
 * @return DHT11_RESULT_OK nếu khởi tạo thành công.
 * @return DHT11_RESULT_INVALID_ARGUMENT nếu config hoặc một trường bắt buộc không hợp lệ.
 * @return DHT11_RESULT_INVALID_CONFIG nếu timer hoặc clock timer không hợp lệ.
 * @return DHT11_RESULT_TIMER_START_FAILED nếu HAL không thể bật input capture.
 *
 * @note Chỉ gọi hàm này một lần sau khi timer và GPIO liên quan đã được CubeMX khởi tạo.
 */
DHT11_Result_t DHT11_Initialize(const DHT11_Config_t *config,
                                uint32_t current_tick_ms);

/**
 * @brief Tiến state machine DHT11 trong main context.
 *
 * Hàm quản lý chu kỳ 2 giây, start signal 20 ms, timeout khung 10 ms, checksum
 * và phục hồi sau lỗi. Hàm không sử dụng delay hoặc vòng lặp chờ.
 *
 * @param current_tick_ms Giá trị HAL tick hiện tại, thường lấy từ HAL_GetTick().
 *
 * @note Gọi hàm này thường xuyên trong superloop.
 */
void DHT11_Service(uint32_t current_tick_ms);

/* Điểm tích hợp dành riêng cho HAL input-capture callback. */

/**
 * @brief Xử lý một sự kiện input capture của timer trong interrupt context.
 *
 * Hàm xác nhận timer/channel đã cấu hình, đọc capture, giải mã ACK và ghi từng bit vào
 * bộ đệm received frame. Checksum và cập nhật dữ liệu hợp lệ không chạy trong ISR.
 *
 * @param timer Con trỏ timer do HAL_TIM_IC_CaptureCallback() cung cấp.
 *
 * @note Gọi hàm này từ HAL_TIM_IC_CaptureCallback().
 */
void DHT11_HandleInputCaptureInterrupt(TIM_HandleTypeDef *timer);

/* API đọc dữ liệu và trạng thái từ main context. */

/**
 * @brief Sao chép dữ liệu hợp lệ gần nhất mà không xóa cờ dữ liệu mới.
 *
 * @param output_data Vùng nhớ của caller dùng để nhận bốn byte dữ liệu hợp lệ.
 *
 * @return true nếu đã sao chép dữ liệu hợp lệ.
 * @return false nếu output_data là NULL hoặc chưa có dữ liệu hợp lệ.
 */
bool DHT11_GetLatestData(DHT11_Data_t *output_data);

/**
 * @brief Lấy dữ liệu mới gần nhất và xác nhận đã tiêu thụ dữ liệu đó.
 *
 * @param output_data Vùng nhớ của caller dùng để nhận bốn byte dữ liệu hợp lệ mới.
 *
 * @return true nếu có dữ liệu mới và đã sao chép thành công.
 * @return false nếu output_data là NULL hoặc không có dữ liệu mới.
 */
bool DHT11_TakeNewData(DHT11_Data_t *output_data);

/**
 * @brief Lấy snapshot trạng thái hiện tại của module.
 *
 * @param output_status Vùng nhớ của caller dùng để nhận trạng thái và thông tin chẩn đoán.
 *
 * @note Hàm không làm thay đổi state machine hoặc các cờ trạng thái.
 */
void DHT11_GetStatus(DHT11_Status_t *output_status);

#ifdef __cplusplus
}
#endif

#endif /* USER_DRIVERS_DHT11_DHT11_H_ */
