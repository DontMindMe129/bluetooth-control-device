/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define BOARD_STATUS_LED_Pin GPIO_PIN_13
#define BOARD_STATUS_LED_GPIO_Port GPIOC
#define ANALOG_INPUT_1_Pin GPIO_PIN_0
#define ANALOG_INPUT_1_GPIO_Port GPIOA
#define ANALOG_INPUT_2_Pin GPIO_PIN_1
#define ANALOG_INPUT_2_GPIO_Port GPIOA
#define ANALOG_INPUT_3_Pin GPIO_PIN_2
#define ANALOG_INPUT_3_GPIO_Port GPIOA
#define ANALOG_INPUT_4_Pin GPIO_PIN_3
#define ANALOG_INPUT_4_GPIO_Port GPIOA
#define SPI_PORT_1_CS_1_Pin GPIO_PIN_4
#define SPI_PORT_1_CS_1_GPIO_Port GPIOA
#define SPI_PORT_1_SCK_Pin GPIO_PIN_5
#define SPI_PORT_1_SCK_GPIO_Port GPIOA
#define SPI_PORT_1_MISO_Pin GPIO_PIN_6
#define SPI_PORT_1_MISO_GPIO_Port GPIOA
#define SPI_PORT_1_MOSI_Pin GPIO_PIN_7
#define SPI_PORT_1_MOSI_GPIO_Port GPIOA
#define DIGITAL_INPUT_1_Pin GPIO_PIN_0
#define DIGITAL_INPUT_1_GPIO_Port GPIOB
#define DIGITAL_INPUT_1_EXTI_IRQn EXTI0_IRQn
#define CAPTURE_INPUT_1_Pin GPIO_PIN_1
#define CAPTURE_INPUT_1_GPIO_Port GPIOB
#define DIGITAL_OUTPUT_3_Pin GPIO_PIN_10
#define DIGITAL_OUTPUT_3_GPIO_Port GPIOB
#define DIGITAL_OUTPUT_4_Pin GPIO_PIN_11
#define DIGITAL_OUTPUT_4_GPIO_Port GPIOB
#define DIGITAL_OUTPUT_5_Pin GPIO_PIN_12
#define DIGITAL_OUTPUT_5_GPIO_Port GPIOB
#define INTERRUPT_INPUT_1_Pin GPIO_PIN_13
#define INTERRUPT_INPUT_1_GPIO_Port GPIOB
#define INTERRUPT_INPUT_1_EXTI_IRQn EXTI15_10_IRQn
#define SPI_PORT_1_CS_2_Pin GPIO_PIN_14
#define SPI_PORT_1_CS_2_GPIO_Port GPIOB
#define SPI_PORT_1_CS_3_Pin GPIO_PIN_15
#define SPI_PORT_1_CS_3_GPIO_Port GPIOB
#define PWM_OUTPUT_1_Pin GPIO_PIN_8
#define PWM_OUTPUT_1_GPIO_Port GPIOA
#define UART_PORT_1_TX_Pin GPIO_PIN_9
#define UART_PORT_1_TX_GPIO_Port GPIOA
#define UART_PORT_1_RX_Pin GPIO_PIN_10
#define UART_PORT_1_RX_GPIO_Port GPIOA
#define DIGITAL_INPUT_5_Pin GPIO_PIN_15
#define DIGITAL_INPUT_5_GPIO_Port GPIOA
#define DIGITAL_INPUT_5_EXTI_IRQn EXTI15_10_IRQn
#define DIGITAL_INPUT_2_Pin GPIO_PIN_3
#define DIGITAL_INPUT_2_GPIO_Port GPIOB
#define DIGITAL_INPUT_2_EXTI_IRQn EXTI3_IRQn
#define DIGITAL_INPUT_3_Pin GPIO_PIN_4
#define DIGITAL_INPUT_3_GPIO_Port GPIOB
#define DIGITAL_INPUT_3_EXTI_IRQn EXTI4_IRQn
#define DIGITAL_INPUT_4_Pin GPIO_PIN_5
#define DIGITAL_INPUT_4_GPIO_Port GPIOB
#define DIGITAL_INPUT_4_EXTI_IRQn EXTI9_5_IRQn
#define I2C_PORT_1_SCL_Pin GPIO_PIN_6
#define I2C_PORT_1_SCL_GPIO_Port GPIOB
#define I2C_PORT_1_SDA_Pin GPIO_PIN_7
#define I2C_PORT_1_SDA_GPIO_Port GPIOB
#define DIGITAL_OUTPUT_1_Pin GPIO_PIN_8
#define DIGITAL_OUTPUT_1_GPIO_Port GPIOB
#define DIGITAL_OUTPUT_2_Pin GPIO_PIN_9
#define DIGITAL_OUTPUT_2_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
