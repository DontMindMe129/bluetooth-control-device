/**
 * @file output_control.h
 * @brief Quản lý lựa chọn và trạng thái logic của năm ngõ ra số.
 *
 * Service này thuần phần mềm, không truy cập GPIO hay HAL. Application chịu trách
 * nhiệm ánh xạ trạng thái logic do service cung cấp sang driver phần cứng.
 */

#ifndef USER_SERVICES_OUTPUT_CONTROL_OUTPUT_CONTROL_H_
#define USER_SERVICES_OUTPUT_CONTROL_OUTPUT_CONTROL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/** @brief Số ngõ ra có thể điều khiển trên trang Outputs. */
#define OUTPUT_CONTROL_COUNT (5U)

/** @brief Snapshot công khai phục vụ application, giao diện và debugger. */
typedef struct
{
    bool is_initialized;       /**< true sau khi service được khởi tạo thành công. */
    uint8_t selected_output;   /**< Chỉ số ngõ ra đang chọn, từ 0 đến 4. */
    uint8_t output_on_mask;    /**< Bit n bằng 1 khi ngõ ra n đang ON. */
    uint32_t change_count;     /**< Tổng số lần lựa chọn hoặc trạng thái bị thay đổi. */
} OutputControl_Status_t;

/** @brief Context tĩnh do application sở hữu. */
typedef struct
{
    OutputControl_Status_t status; /**< Trạng thái hiện tại của service. */
} OutputControl_t;

/**
 * @brief Khởi tạo service với OUT1 được chọn và toàn bộ ngõ ra ở trạng thái OFF.
 * @param control Context do caller cấp phát tĩnh.
 * @return true nếu context hợp lệ; false nếu control là NULL.
 */
bool OutputControl_Initialize(OutputControl_t *control);

/** @brief Chọn ngõ ra phía trên; từ OUT1 sẽ quay vòng về OUT5. */
void OutputControl_SelectPrevious(OutputControl_t *control);

/** @brief Chọn ngõ ra phía dưới; từ OUT5 sẽ quay vòng về OUT1. */
void OutputControl_SelectNext(OutputControl_t *control);

/** @brief Đảo trạng thái ON/OFF của ngõ ra đang được chọn. */
void OutputControl_ToggleSelected(OutputControl_t *control);

/**
 * @brief Đọc trạng thái logic của một ngõ ra.
 * @param control Context đã khởi tạo.
 * @param output_index Chỉ số từ 0 đến 4.
 * @return true khi ngõ ra hợp lệ và đang ON; false trong các trường hợp còn lại.
 */
bool OutputControl_IsOutputOn(const OutputControl_t *control,
                              uint8_t output_index);

/** @brief Sao chép snapshot để application, UI hoặc debugger sử dụng. */
void OutputControl_GetStatus(const OutputControl_t *control,
                             OutputControl_Status_t *output_status);

#ifdef __cplusplus
}
#endif

#endif /* USER_SERVICES_OUTPUT_CONTROL_OUTPUT_CONTROL_H_ */
