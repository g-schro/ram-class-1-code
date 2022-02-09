/*
 * @brief Implementation of i2c module.
 *
 * This module ...
 *
 * The following console commands are provided:
 * > i2c status
 * See code for details.
 *
 * MIT License
 * 
 * Copyright (c) 2021 Eugene R Schroeder
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdint.h>
#include <stdio.h>

#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_i2c.h"

#include "module.h"
#include "tmr.h"

#include "i2c.h"

////////////////////////////////////////////////////////////////////////////////
// Common macros
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Type definitions
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Private (static) function declarations
////////////////////////////////////////////////////////////////////////////////

static void fix_analog_filter(I2C_HandleTypeDef* handle);

////////////////////////////////////////////////////////////////////////////////
// Private (static) variables
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Public (global) variables and externs
////////////////////////////////////////////////////////////////////////////////

extern I2C_HandleTypeDef hi2c3;

////////////////////////////////////////////////////////////////////////////////
// Public (global) functions
////////////////////////////////////////////////////////////////////////////////

/*
 * @brief Get default i2c configuration.
 *
 * @param[in] instance_id Identifies the i2c instance.
 * @param[out] cfg The i2c configuration with defaults filled in.
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 */
int32_t i2c_get_def_cfg(i2c_instance_id instance_id, struct i2c_cfg* cfg)
{
    return 0;
}

/*
 * @brief Initialize i2c instance.
 *
 * @param[in] instance_id Identifies the i2c instance.
 * @param[in] cfg The i2c configuration. (FUTURE)
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * This function initializes a i2c module instance. Generally, it should not
 * access other modules as they might not have been initialized yet.  An
 * exception is the log module.
 */
int32_t i2c_init(i2c_instance_id instance_id, struct i2c_cfg* cfg)
{
    return 0;
}

/*
 * @brief Start i2c instance.
 *
 * @param[in] instance_id Identifies the i2c instance.
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * This function starts a i2c module instance, to enter normal operation.
 */
int32_t i2c_start(i2c_instance_id instance_id)
{
    return 0;
}

const uint16_t sensor_i2c_addr = 0x44 * 2;

void start_meas()
{
    uint8_t msg[2] = { 0x2c, 0x0d };
    HAL_StatusTypeDef rc;

    rc = HAL_I2C_Master_Transmit(&hi2c3, sensor_i2c_addr, msg, 2, 1000);
    printf("start meas returns %d ErrorCode=%lu\n", rc, hi2c3.ErrorCode);
}

/*
 * @brief Run i2c instance.
 *
 * @param[in] instance_id Identifies the i2c instance.
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * @note This function should not block.
 *
 * This function runs a i2c module instance, during normal operation.
 */
int32_t i2c_run(i2c_instance_id instance_id)
{
    static uint32_t last_run = 0;
    static int fix_done = 0;

    if (tmr_get_ms() - last_run > 1000*10) {
        if (!fix_done) {
            fix_done = 1;
            start_meas();
            //fix_analog_filter(&hi2c3);
        } else {
            if (!fix_done) fix_analog_filter(&hi2c3);
            start_meas();
        }
        last_run = tmr_get_ms();
    }
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Private (static) functions
////////////////////////////////////////////////////////////////////////////////

/*
 * @brief Fix analog filter.
 *
 * @param[in] instance_id Identifies the i2c instance.
 *
 * See https://www.st.com/content/ccc/resource/technical/document/
 *     errata_sheet/7f/05/b0/bc/34/2f/4c/21/CD00288116.pdf/files/
 *     CD00288116.pdf/jcr:content/translations/en.CD00288116.pdf
 *
 * Procedue from that document:
 *
 *  1. Disable the I2C peripheral by clearing the PE bit in I2Cx_CR1 register.
 *  2. Configure the SCL and SDA I/Os as General Purpose Output Open-Drain, High
 *     level (Write 1 to GPIOx_ODR).
 *  3. Check SCL and SDA High level in GPIOx_IDR.
 *  4. Configure the SDA I/O as General Purpose Output Open-Drain, Low level
 *     (Write 0 to GPIOx_ODR).
 *  5. Check SDA Low level in GPIOx_IDR.
 *  6. Configure the SCL I/O as General Purpose Output Open-Drain, Low level
 *     (Write 0 to GPIOx_ODR).
 *  7. Check SCL Low level in GPIOx_IDR.
 *  8. Configure the SCL I/O as General Purpose Output Open-Drain, High level
 *     (Write 1 to GPIOx_ODR).
 *  9. Check SCL High level in GPIOx_IDR.
 * 10. Configure the SDA I/O as General Purpose Output Open-Drain , High level
 *     (Write 1 to GPIOx_ODR).
 * 11. Check SDA High level in GPIOx_IDR.
 * 12. Configure the SCL and SDA I/Os as Alternate function Open-Drain.
 * 13. Set SWRST bit in I2Cx_CR1 register.
 * 14. Clear SWRST bit in I2Cx_CR1 register.
 * 15. Enable the I2C peripheral by setting the PE bit in I2Cx_CR1 register.
 */

static void fix_analog_filter(I2C_HandleTypeDef* handle)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    printf("Doing fix\n");

    // 1. Disable the I2C peripheral by clearing the PE bit in I2Cx_CR1 register.
    __HAL_I2C_DISABLE(handle);

    // 2. Configure the SCL and SDA I/Os as General Purpose Output Open-Drain, High
    //    level (Write 1 to GPIOx_ODR).
    GPIO_InitStruct.Pin = GPIO_PIN_8;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = 0;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_4;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF9_I2C3;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_SET);

    // 3. Check SCL and SDA High level in GPIOx_IDR.
    HAL_Delay(2);
    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_8) != GPIO_PIN_SET)
        printf("Step 3: A8 not 1\n");
    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_4) != GPIO_PIN_SET)
        printf("Step 3: B4 not 1\n");
    
    // 4. Configure the SDA I/O as General Purpose Output Open-Drain, Low level
    //    (Write 0 to GPIOx_ODR).
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_RESET);

    // 5. Check SDA Low level in GPIOx_IDR.
    HAL_Delay(2);
    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_4) != GPIO_PIN_RESET)
        printf("Step 5: B4 not 0\n");

    // 6. Configure the SCL I/O as General Purpose Output Open-Drain, Low level
    //    (Write 0 to GPIOx_ODR).
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);

    // 7. Check SCL Low level in GPIOx_IDR.
    HAL_Delay(2);
    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_8) != GPIO_PIN_RESET)
        printf("Step 7: A8 not 0\n");

    // 8. Configure the SCL I/O as General Purpose Output Open-Drain, High level
    //    (Write 1 to GPIOx_ODR).
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_SET);

    // 9. Check SCL High level in GPIOx_IDR.
    HAL_Delay(2);
    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_8) != GPIO_PIN_SET)
        printf("Step 9: A8 not 0\n");

    // 10. Configure the SDA I/O as General Purpose Output Open-Drain , High level
    //     (Write 1 to GPIOx_ODR).
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_SET);

    // 11. Check SDA High level in GPIOx_IDR.
    HAL_Delay(2);
    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_4) != GPIO_PIN_SET)
        printf("Step 11: B4 not 1\n");

    // 12. Configure the SCL and SDA I/Os as Alternate function Open-Drain.
    GPIO_InitStruct.Pin = GPIO_PIN_8;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF4_I2C3;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_4;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF9_I2C3;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    // 13. Set SWRST bit in I2Cx_CR1 register.
    handle->Instance->CR1 |= I2C_CR1_SWRST;

    // 14. Clear SWRST bit in I2Cx_CR1 register.
    handle->Instance->CR1 &= ~I2C_CR1_SWRST;

    // 15. Enable the I2C peripheral by setting the PE bit in I2Cx_CR1 register.
    __HAL_I2C_ENABLE(handle);
}
