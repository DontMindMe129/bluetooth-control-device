/**
 * @file ssd1306.h
 * @brief Driver framebuffer không blocking cho OLED SSD1306 I2C 128x64 không có chân reset.
 *
 * Driver quản lý command của controller, framebuffer 1-bit và quá trình refresh qua
 * I2cBus. Các thuật toán vẽ đường, hình, bitmap và chữ không thuộc module này.
 */

#ifndef USER_DRIVERS_SSD1306_SSD1306_H_
#define USER_DRIVERS_SSD1306_SSD1306_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "i2c_bus.h"

/** @brief Chiều rộng cố định của panel được driver hỗ trợ. */
#define SSD1306_WIDTH_PIXELS             128U

/** @brief Chiều cao cố định của panel được driver hỗ trợ. */
#define SSD1306_HEIGHT_PIXELS            64U

/** @brief Số page GDDRAM; mỗi page chứa 8 hàng pixel. */
#define SSD1306_PAGE_COUNT               8U

/** @brief Số byte ảnh 1-bit của panel 128x64. */
#define SSD1306_FRAMEBUFFER_SIZE_BYTES   1024U

/** @brief Tổng số byte gói refresh gồm control byte và framebuffer. */
#define SSD1306_FRAME_PACKET_SIZE_BYTES  1025U

/** @brief Kích thước tối đa của buffer command nội bộ. */
#define SSD1306_COMMAND_BUFFER_SIZE_BYTES 32U

/** @brief Hai địa chỉ 7-bit mà SSD1306 cho phép qua chân SA0. */
#define SSD1306_ADDRESS_LOW_7BIT         0x3CU
#define SSD1306_ADDRESS_HIGH_7BIT        0x3DU

/** @brief Hướng ánh xạ framebuffer lên panel. */
typedef enum
{
    SSD1306_ORIENTATION_0_DEGREES = 0, /**< Ánh xạ segment/COM theo hướng mặc định của module phổ biến. */
    SSD1306_ORIENTATION_180_DEGREES    /**< Xoay nội dung vật lý 180 độ bằng command controller. */
} Ssd1306_Orientation_t;

/** @brief Phép toán áp dụng lên một hoặc toàn bộ pixel trong framebuffer. */
typedef enum
{
    SSD1306_PIXEL_OFF = 0, /**< Xóa pixel về 0. */
    SSD1306_PIXEL_ON,      /**< Đặt pixel thành 1. */
    SSD1306_PIXEL_TOGGLE   /**< Đảo trạng thái hiện tại của pixel. */
} Ssd1306_PixelOperation_t;

/** @brief Kết quả trả về ngay khi cấu hình context SSD1306. */
typedef enum
{
    SSD1306_INITIALIZE_ACCEPTED = 0, /**< Cấu hình hợp lệ; state machine sẽ khởi tạo bất đồng bộ. */
    SSD1306_INITIALIZE_INVALID_ARGUMENT /**< Con trỏ, địa chỉ, timeout hoặc orientation không hợp lệ. */
} Ssd1306_InitializeResult_t;

/** @brief Kết quả trả về khi yêu cầu refresh framebuffer. */
typedef enum
{
    SSD1306_REFRESH_ACCEPTED = 0, /**< Yêu cầu đã được ghi nhận. */
    SSD1306_REFRESH_NOT_READY,    /**< OLED chưa khởi tạo xong hoặc đã vào trạng thái lỗi. */
    SSD1306_REFRESH_ALREADY_PENDING, /**< Một refresh đã chờ hoặc đang chạy. */
    SSD1306_REFRESH_RESULT_PENDING, /**< Caller chưa lấy kết quả của refresh trước. */
    SSD1306_REFRESH_NO_CHANGES    /**< Framebuffer không thay đổi từ lần refresh thành công gần nhất. */
} Ssd1306_RefreshRequestResult_t;

/** @brief Kết quả cuối cùng của initialization hoặc refresh. */
typedef enum
{
    SSD1306_RESULT_NONE = 0,       /**< Chưa có kết quả. */
    SSD1306_RESULT_SUCCESS,        /**< Thao tác hoàn thành thành công. */
    SSD1306_RESULT_BUS_WAIT_TIMEOUT, /**< Không giành được bus trong thời gian cho phép. */
    SSD1306_RESULT_I2C_TIMEOUT,    /**< I2C đã bắt đầu nhưng vượt timeout. */
    SSD1306_RESULT_I2C_ERROR,      /**< I2C báo lỗi truyền. */
    SSD1306_RESULT_I2C_ABORT_FAILED /**< I2C không thể phục hồi an toàn sau timeout. */
} Ssd1306_Result_t;

/** @brief Các bước nội bộ của quá trình khởi tạo và refresh không blocking. */
typedef enum
{
    SSD1306_STATE_UNINITIALIZED = 0, /**< Chưa nhận cấu hình hợp lệ. */
    SSD1306_STATE_INIT_SEND_CONFIG,  /**< Đang chờ gửi command cấu hình controller. */
    SSD1306_STATE_INIT_WAIT_CONFIG,  /**< Đang chờ kết quả command cấu hình. */
    SSD1306_STATE_INIT_SEND_WINDOW,  /**< Đang chờ đặt vùng GDDRAM 128x64. */
    SSD1306_STATE_INIT_WAIT_WINDOW,  /**< Đang chờ kết quả đặt vùng GDDRAM. */
    SSD1306_STATE_INIT_SEND_CLEAR,   /**< Đang chờ gửi framebuffer rỗng ban đầu. */
    SSD1306_STATE_INIT_WAIT_CLEAR,   /**< Đang chờ kết quả xóa GDDRAM. */
    SSD1306_STATE_INIT_SEND_DISPLAY_ON, /**< Đang chờ gửi command bật panel. */
    SSD1306_STATE_INIT_WAIT_DISPLAY_ON, /**< Đang chờ kết quả bật panel. */
    SSD1306_STATE_READY,             /**< OLED sẵn sàng nhận thao tác framebuffer. */
    SSD1306_STATE_REFRESH_SEND_WINDOW, /**< Đang chờ đặt lại vùng GDDRAM trước refresh. */
    SSD1306_STATE_REFRESH_WAIT_WINDOW, /**< Đang chờ kết quả đặt vùng refresh. */
    SSD1306_STATE_REFRESH_SEND_DATA, /**< Đang chờ bắt đầu gửi framebuffer. */
    SSD1306_STATE_REFRESH_WAIT_DATA, /**< Framebuffer đang được I2C truyền. */
    SSD1306_STATE_ERROR              /**< Driver dừng vì lỗi; không tự retry. */
} Ssd1306_State_t;

/** @brief Cấu hình cố định của một OLED SSD1306 trong suốt vòng đời instance. */
typedef struct
{
    I2cBus_t *i2c_bus;              /**< Bus I2C đã được khởi tạo và service riêng trong superloop. */
    uint8_t address_7bit;           /**< Địa chỉ SSD1306 7-bit do tầng Board/App cung cấp. */
    uint32_t transfer_timeout_ms;   /**< Timeout cho mỗi lần giành bus và mỗi giao dịch I2C. */
    uint8_t contrast;               /**< Contrast controller từ 0 đến 255. */
    Ssd1306_Orientation_t orientation; /**< Hướng ánh xạ vật lý mong muốn. */
} Ssd1306_Config_t;

/** @brief Snapshot trạng thái công khai phục vụ tầng application và debugger. */
typedef struct
{
    bool is_initialized;            /**< true sau khi cấu hình được chấp nhận. */
    bool is_ready;                  /**< true sau khi init, clear GDDRAM và bật panel thành công. */
    bool has_error;                 /**< true khi state machine đã dừng vì lỗi. */
    bool framebuffer_is_dirty;      /**< true khi RAM STM32 khác frame đã gửi thành công gần nhất. */
    bool framebuffer_is_locked;     /**< true khi I2C đang đọc trực tiếp frame packet. */
    bool refresh_is_pending;        /**< true khi caller đã yêu cầu refresh nhưng chưa bắt đầu. */
    bool refresh_is_running;        /**< true khi chuỗi set-window/transfer đang diễn ra. */
    uint8_t address_7bit;           /**< Địa chỉ SSD1306 instance đang sử dụng. */
    Ssd1306_State_t state;          /**< Bước state machine hiện tại. */
    Ssd1306_Result_t initialization_result; /**< Kết quả khởi tạo gần nhất. */
    Ssd1306_Result_t last_refresh_result; /**< Kết quả refresh gần nhất. */
    I2cBus_Result_t last_i2c_result; /**< Kết quả thấp hơn do I2cBus trả về. */
} Ssd1306_Status_t;

/**
 * @brief Context đầy đủ của một OLED SSD1306 128x64.
 *
 * Caller cấp phát tĩnh context này. Không sửa trực tiếp các trường sau khi khởi tạo.
 */
typedef struct
{
    Ssd1306_Config_t config;        /**< Bản sao cấu hình dùng trong suốt vòng đời instance. */
    Ssd1306_Status_t status;        /**< Trạng thái có thể sao chép để debug. */

    uint8_t command_buffer[SSD1306_COMMAND_BUFFER_SIZE_BYTES]; /**< Gói command đang chờ/gửi. */
    uint8_t command_length;         /**< Số byte hợp lệ hiện có trong command_buffer. */
    uint8_t frame_packet[SSD1306_FRAME_PACKET_SIZE_BYTES]; /**< Byte 0 là 0x40; phần còn lại là framebuffer. */

    uint32_t state_start_tick_ms;   /**< Tick bắt đầu chờ giành bus của send-state hiện tại. */
    bool refresh_result_is_new;     /**< true khi có kết quả refresh caller chưa lấy. */
} Ssd1306_t;

/**
 * @brief Cấu hình context và bắt đầu quá trình khởi tạo SSD1306 bất đồng bộ.
 * @param display Context do caller cấp phát tĩnh.
 * @param config Bus, địa chỉ, timeout, contrast và orientation.
 * @param current_tick_ms HAL tick hiện tại.
 * @return Kết quả kiểm tra và chấp nhận cấu hình.
 * @note I2cBus_Service() phải được gọi trước Ssd1306_Service() trong mỗi vòng superloop.
 */
Ssd1306_InitializeResult_t Ssd1306_Initialize(Ssd1306_t *display,
                                              const Ssd1306_Config_t *config,
                                              uint32_t current_tick_ms);

/**
 * @brief Tiến state machine khởi tạo hoặc refresh đúng một bước.
 * @param display Context đã được cấu hình.
 * @param current_tick_ms HAL tick hiện tại.
 * @note Hàm không chờ bus hoặc phần cứng hoàn thành.
 */
void Ssd1306_Service(Ssd1306_t *display, uint32_t current_tick_ms);

/**
 * @brief Xóa toàn bộ pixel trong framebuffer STM32.
 * @param display Context OLED đã sẵn sàng.
 * @return true khi framebuffer có thể được truy cập; false nếu chưa ready hoặc đang bị khóa.
 * @note Cần gọi Ssd1306_RequestRefresh() để thay đổi xuất hiện trên panel.
 */
bool Ssd1306_Clear(Ssd1306_t *display);

/**
 * @brief Áp dụng một phép toán lên toàn bộ pixel trong framebuffer STM32.
 * @param display Context OLED đã sẵn sàng.
 * @param operation OFF, ON hoặc TOGGLE.
 * @return true khi thao tác hợp lệ; false nếu chưa ready, đang bị khóa hoặc operation sai.
 * @note Cần gọi Ssd1306_RequestRefresh() để thay đổi xuất hiện trên panel.
 */
bool Ssd1306_Fill(Ssd1306_t *display,
                  Ssd1306_PixelOperation_t operation);

/**
 * @brief Áp dụng một phép toán lên một pixel trong framebuffer STM32.
 * @param display Context OLED đã sẵn sàng.
 * @param x Tọa độ ngang từ 0 đến 127.
 * @param y Tọa độ dọc từ 0 đến 63.
 * @param operation OFF, ON hoặc TOGGLE.
 * @return true khi tọa độ và trạng thái context hợp lệ; false nếu không thể sửa framebuffer.
 * @note Cần gọi Ssd1306_RequestRefresh() để thay đổi xuất hiện trên panel.
 */
bool Ssd1306_SetPixel(Ssd1306_t *display,
                      uint8_t x,
                      uint8_t y,
                      Ssd1306_PixelOperation_t operation);

/**
 * @brief Đọc trạng thái một pixel từ framebuffer STM32, không đọc ngược GDDRAM.
 * @param display Context OLED đã sẵn sàng.
 * @param x Tọa độ ngang từ 0 đến 127.
 * @param y Tọa độ dọc từ 0 đến 63.
 * @param pixel_is_on Nơi nhận true nếu bit framebuffer đang bằng 1.
 * @return true khi đọc thành công; false nếu tham số hoặc context không hợp lệ.
 */
bool Ssd1306_GetPixel(const Ssd1306_t *display,
                      uint8_t x,
                      uint8_t y,
                      bool *pixel_is_on);

/**
 * @brief Ghi nhận yêu cầu gửi framebuffer hiện tại ra panel.
 * @param display Context OLED đã sẵn sàng.
 * @return Trạng thái chấp nhận yêu cầu.
 */
Ssd1306_RefreshRequestResult_t Ssd1306_RequestRefresh(Ssd1306_t *display);

/**
 * @brief Lấy đúng một lần kết quả refresh mới nhất.
 * @param display Context OLED đã khởi tạo.
 * @param result Nơi nhận kết quả; không được là NULL.
 * @return true khi có kết quả mới; false nếu refresh chưa hoàn thành hoặc đã được lấy.
 */
bool Ssd1306_TakeRefreshResult(Ssd1306_t *display,
                              Ssd1306_Result_t *result);

/**
 * @brief Sao chép snapshot trạng thái SSD1306 để tầng trên sử dụng hoặc debug.
 * @param display Context cần đọc.
 * @param output_status Vùng nhớ nhận snapshot; NULL sẽ được bỏ qua.
 */
void Ssd1306_GetStatus(const Ssd1306_t *display,
                       Ssd1306_Status_t *output_status);

#ifdef __cplusplus
}
#endif

#endif /* USER_DRIVERS_SSD1306_SSD1306_H_ */
