/*
 * @brief Implementation of CAN module.
 *
 * This module ...
 *
 * The following console commands are provided:
 * > can status
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
#include <string.h>

#include "config.h"
#include CONFIG_STM32_LL_CORTEX_HDR // Used to get CAN stuff.

#include "can.h"
#include "log.h"
#include "module.h"

////////////////////////////////////////////////////////////////////////////////
// Common macros
////////////////////////////////////////////////////////////////////////////////

// Much of the CAN configuration is hardcoded. This is expressed in the #defines
// below. These are generally register fields.

// Register BTR parameters related to bus speed of 125000. The values for
// presale, time seg 1, and time seg 1 were gotten from this website:
//   http://www.bittiming.can-wiki.info/
// Note we subtract off 1 due to how the values are coded in the register.

#define BTR_BRP_40_SHIFTED_VAL ((40-1) << 0)
#define BTR_BRP_SHIFTED_VAL BTR_BRP_40_SHIFTED_VAL

////////////////////////////////////////////////////////////////////////////////
// Type definitions
////////////////////////////////////////////////////////////////////////////////

struct can_state {
    struct can_cfg cfg;
    I2C_TypeDef* can_reg_base;
};

////////////////////////////////////////////////////////////////////////////////
// Private (static) function declarations
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Private (static) variables
////////////////////////////////////////////////////////////////////////////////

static struct can_state can_states[CAN_NUM_INSTANCES];

static int32_t log_level = LOG_DEFAULT;

////////////////////////////////////////////////////////////////////////////////
// Public (global) variables and externs
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Public (global) functions
////////////////////////////////////////////////////////////////////////////////

/*
 * @brief Get default can configuration.
 *
 * @param[in] instance_id Identifies the can instance.
 * @param[out] cfg The can configuration with defaults filled in.
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 */
int32_t can_get_def_cfg(enum can_instance_id instance_id, struct can_cfg* cfg)
{
    return 0;
}

/*
 * @brief Initialize can instance.
 *
 * @param[in] instance_id Identifies the can instance.
 * @param[in] cfg The can configuration. (FUTURE)
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * This function initializes a can module instance. Generally, it should not
 * access other modules as they might not have been initialized yet.  An
 * exception is the log module.
 */
int32_t can_init(enum can_instance_id instance_id, struct can_cfg* cfg)
{
    if (instance_id >= CAN_NUM_INSTANCES)
        return MOD_ERR_BAD_INSTANCE;
    memset(&can_states[instance_id], 0, sizeof(can_states[instance_id]));
    return 0;
}

/*
 * @brief Start can instance.
 *
 * @param[in] instance_id Identifies the can instance.
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * This function starts a can module instance, to enter normal operation.
 */
int32_t can_start(enum can_instance_id instance_id)
{
    return 0;
}

/*
 * @brief Run can instance.
 *
 * @param[in] instance_id Identifies the can instance.
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * @note This function should not block.
 *
 * This function runs a can module instance, during normal operation.
 */
int32_t can_run(enum can_instance_id instance_id)
{
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Private (static) functions
////////////////////////////////////////////////////////////////////////////////

int32_t can_hdw_init(enum can_instance_id instance_id)
{
    log_verbose("can_hdw_init\n"); // TODO REMOVE
    return 0;
#if 0
    struct can_state* st;
    struct dio_direct_cfg dio_cfg;
    uint32_t rc;

    if (instance_id >= CAN_NUM_INSTANCES)
        return MOD_ERR_BAD_INSTANCE;
        
    st = &can_states[instance_id];

#if CONFIG_CAN_1_PRESENT
    if (instance_id == CAN_INSTANCE_1) {
        SET_BIT(RCC->APB1ENR1, RCC_APB1ENR1_CAN1EN);
    }
#endif

#if CONFIG_CAN_2_PRESENT
    else if (instance_id == CAN_INSTANCE_2) {
        SET_BIT(RCC->APB1ENR1, RCC_APB1ENR1_CAN2EN);
    }
#endif

    else {
        return MOD_ERR_BAD_INSTANCE;
    }

    // TODO Need delay here?

    dio_cfg.port = st->cfg.can_tx_pin_port;
    dio_cfg.pin_mask = st->cfg.can_tx_pin;
    dio_cfg.mode = DIO_MODE_FUNCTION;
    dio_cfg.pull = DIO_PULL_NO;
    dio_cfg.init_value = -1;
    dio_cfg.speed = DIO_SPEED_FREQ_VERY_HIGH;
    dio_cfg.output_type = DIO_OUTPUT_PUSHPULL;
    dio_cfg.function = DIO_GPIO_FUNC_4;
    rc = dio_direct_cfg(&dio_cfg);
    if (rc != 0) {
        log_error("can_hdw_init: dio_direct_cfg error tx %d\n", rc);
        return rc;
    }
    dio_cfg.port = st->cfg.can_rx_pin_port;
    dio_cfg.pin_mask = st->cfg.can_rx_pin;
    rc = dio_direct_cfg(&dio_cfg);
    if (rc != 0) {
        log_error("can_hdw_init: dio_direct_cfg error rx %d\n", rc);
        return rc;
    }
    return 0;
#endif
}


#if 0
    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**CAN1 GPIO Configuration
    PB12     ------> CAN1_RX
    PB13     ------> CAN1_TX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_12|GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF10_CAN1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN CAN1_MspInit 1 */

  /* USER CODE END CAN1_MspInit 1 */
  }


    {
      hcan->MspInitCallback = HAL_CAN_MspInit; /* Legacy weak MspInit */
    }

    /* Init the low level hardware: CLOCK, NVIC */
    hcan->MspInitCallback(hcan);
  }

#if 0
#else
  if (hcan->State == HAL_CAN_STATE_RESET)
  {
    /* Init the low level hardware: CLOCK, NVIC */
    HAL_CAN_MspInit(hcan);
  }
#endif /* (USE_HAL_CAN_REGISTER_CALLBACKS) */

  /* Exit from sleep mode */
  CLEAR_BIT(hcan->Instance->MCR, CAN_MCR_SLEEP);

  /* Get tick */
  tickstart = HAL_GetTick();

  /* Check Sleep mode leave acknowledge */
  while ((hcan->Instance->MSR & CAN_MSR_SLAK) != 0U)
  {
    if ((HAL_GetTick() - tickstart) > CAN_TIMEOUT_VALUE)
    {
      /* Update error code */
      hcan->ErrorCode |= HAL_CAN_ERROR_TIMEOUT;

      /* Change CAN state */
      hcan->State = HAL_CAN_STATE_ERROR;

      return HAL_ERROR;
    }
  }

  /* Request initialisation */
  SET_BIT(hcan->Instance->MCR, CAN_MCR_INRQ);

  /* Get tick */
  tickstart = HAL_GetTick();

  /* Wait initialisation acknowledge */
  while ((hcan->Instance->MSR & CAN_MSR_INAK) == 0U)
  {
    if ((HAL_GetTick() - tickstart) > CAN_TIMEOUT_VALUE)
    {
      /* Update error code */
      hcan->ErrorCode |= HAL_CAN_ERROR_TIMEOUT;

      /* Change CAN state */
      hcan->State = HAL_CAN_STATE_ERROR;

      return HAL_ERROR;
    }
  }

  /* Set the time triggered communication mode */
  if (hcan->Init.TimeTriggeredMode == ENABLE)
  {
    SET_BIT(hcan->Instance->MCR, CAN_MCR_TTCM);
  }
  else
  {
    CLEAR_BIT(hcan->Instance->MCR, CAN_MCR_TTCM);
  }

  /* Set the automatic bus-off management */
  if (hcan->Init.AutoBusOff == ENABLE)
  {
    SET_BIT(hcan->Instance->MCR, CAN_MCR_ABOM);
  }
  else
  {
    CLEAR_BIT(hcan->Instance->MCR, CAN_MCR_ABOM);
  }

  /* Set the automatic wake-up mode */
  if (hcan->Init.AutoWakeUp == ENABLE)
  {
    SET_BIT(hcan->Instance->MCR, CAN_MCR_AWUM);
  }
  else
  {
    CLEAR_BIT(hcan->Instance->MCR, CAN_MCR_AWUM);
  }

  /* Set the automatic retransmission */
  if (hcan->Init.AutoRetransmission == ENABLE)
  {
    CLEAR_BIT(hcan->Instance->MCR, CAN_MCR_NART);
  }
  else
  {
    SET_BIT(hcan->Instance->MCR, CAN_MCR_NART);
  }

  /* Set the receive FIFO locked mode */
  if (hcan->Init.ReceiveFifoLocked == ENABLE)
  {
    SET_BIT(hcan->Instance->MCR, CAN_MCR_RFLM);
  }
  else
  {
    CLEAR_BIT(hcan->Instance->MCR, CAN_MCR_RFLM);
  }

  /* Set the transmit FIFO priority */
  if (hcan->Init.TransmitFifoPriority == ENABLE)
  {
    SET_BIT(hcan->Instance->MCR, CAN_MCR_TXFP);
  }
  else
  {
    CLEAR_BIT(hcan->Instance->MCR, CAN_MCR_TXFP);
  }

  /* Set the bit timing register */
  WRITE_REG(hcan->Instance->BTR, (uint32_t)(hcan->Init.Mode           |
                                            hcan->Init.SyncJumpWidth  |
                                            hcan->Init.TimeSeg1       |
                                            hcan->Init.TimeSeg2       |
                                            (hcan->Init.Prescaler - 1U)));

  /* Initialize the error code */
  hcan->ErrorCode = HAL_CAN_ERROR_NONE;

  /* Initialize the CAN state */
  hcan->State = HAL_CAN_STATE_READY;

  /* Return function status */
  return HAL_OK;
}
#endif

#if 0
void HAL_CAN_MspInit(CAN_HandleTypeDef* hcan)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(hcan->Instance==CAN1)
  {
  /* USER CODE BEGIN CAN1_MspInit 0 */

  /* USER CODE END CAN1_MspInit 0 */
    /* Peripheral clock enable */
    __HAL_RCC_CAN1_CLK_ENABLE();

    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**CAN1 GPIO Configuration
    PB12     ------> CAN1_RX
    PB13     ------> CAN1_TX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_12|GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF10_CAN1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN CAN1_MspInit 1 */

  /* USER CODE END CAN1_MspInit 1 */
  }

}
#endif

#if 0
HAL_StatusTypeDef HAL_CAN_Init(CAN_HandleTypeDef *hcan)
{
  uint32_t tickstart;

  /* Check CAN handle */
  if (hcan == NULL)
  {
    return HAL_ERROR;
  }

  /* Check the parameters */
  assert_param(IS_CAN_ALL_INSTANCE(hcan->Instance));
  assert_param(IS_FUNCTIONAL_STATE(hcan->Init.TimeTriggeredMode));
  assert_param(IS_FUNCTIONAL_STATE(hcan->Init.AutoBusOff));
  assert_param(IS_FUNCTIONAL_STATE(hcan->Init.AutoWakeUp));
  assert_param(IS_FUNCTIONAL_STATE(hcan->Init.AutoRetransmission));
  assert_param(IS_FUNCTIONAL_STATE(hcan->Init.ReceiveFifoLocked));
  assert_param(IS_FUNCTIONAL_STATE(hcan->Init.TransmitFifoPriority));
  assert_param(IS_CAN_MODE(hcan->Init.Mode));
  assert_param(IS_CAN_SJW(hcan->Init.SyncJumpWidth));
  assert_param(IS_CAN_BS1(hcan->Init.TimeSeg1));
  assert_param(IS_CAN_BS2(hcan->Init.TimeSeg2));
  assert_param(IS_CAN_PRESCALER(hcan->Init.Prescaler));

#if USE_HAL_CAN_REGISTER_CALLBACKS == 1
  if (hcan->State == HAL_CAN_STATE_RESET)
  {
    /* Reset callbacks to legacy functions */
    hcan->RxFifo0MsgPendingCallback  =  HAL_CAN_RxFifo0MsgPendingCallback;  /* Legacy weak RxFifo0MsgPendingCallback  */
    hcan->RxFifo0FullCallback        =  HAL_CAN_RxFifo0FullCallback;        /* Legacy weak RxFifo0FullCallback        */
    hcan->RxFifo1MsgPendingCallback  =  HAL_CAN_RxFifo1MsgPendingCallback;  /* Legacy weak RxFifo1MsgPendingCallback  */
    hcan->RxFifo1FullCallback        =  HAL_CAN_RxFifo1FullCallback;        /* Legacy weak RxFifo1FullCallback        */
    hcan->TxMailbox0CompleteCallback =  HAL_CAN_TxMailbox0CompleteCallback; /* Legacy weak TxMailbox0CompleteCallback */
    hcan->TxMailbox1CompleteCallback =  HAL_CAN_TxMailbox1CompleteCallback; /* Legacy weak TxMailbox1CompleteCallback */
    hcan->TxMailbox2CompleteCallback =  HAL_CAN_TxMailbox2CompleteCallback; /* Legacy weak TxMailbox2CompleteCallback */
    hcan->TxMailbox0AbortCallback    =  HAL_CAN_TxMailbox0AbortCallback;    /* Legacy weak TxMailbox0AbortCallback    */
    hcan->TxMailbox1AbortCallback    =  HAL_CAN_TxMailbox1AbortCallback;    /* Legacy weak TxMailbox1AbortCallback    */
    hcan->TxMailbox2AbortCallback    =  HAL_CAN_TxMailbox2AbortCallback;    /* Legacy weak TxMailbox2AbortCallback    */
    hcan->SleepCallback              =  HAL_CAN_SleepCallback;              /* Legacy weak SleepCallback              */
    hcan->WakeUpFromRxMsgCallback    =  HAL_CAN_WakeUpFromRxMsgCallback;    /* Legacy weak WakeUpFromRxMsgCallback    */
    hcan->ErrorCallback              =  HAL_CAN_ErrorCallback;              /* Legacy weak ErrorCallback              */

    if (hcan->MspInitCallback == NULL)
    {
      hcan->MspInitCallback = HAL_CAN_MspInit; /* Legacy weak MspInit */
    }

    /* Init the low level hardware: CLOCK, NVIC */
    hcan->MspInitCallback(hcan);
  }

#else
  if (hcan->State == HAL_CAN_STATE_RESET)
  {
    /* Init the low level hardware: CLOCK, NVIC */
    HAL_CAN_MspInit(hcan);
  }
#endif /* (USE_HAL_CAN_REGISTER_CALLBACKS) */

  /* Exit from sleep mode */
  CLEAR_BIT(hcan->Instance->MCR, CAN_MCR_SLEEP);

  /* Get tick */
  tickstart = HAL_GetTick();

  /* Check Sleep mode leave acknowledge */
  while ((hcan->Instance->MSR & CAN_MSR_SLAK) != 0U)
  {
    if ((HAL_GetTick() - tickstart) > CAN_TIMEOUT_VALUE)
    {
      /* Update error code */
      hcan->ErrorCode |= HAL_CAN_ERROR_TIMEOUT;

      /* Change CAN state */
      hcan->State = HAL_CAN_STATE_ERROR;

      return HAL_ERROR;
    }
  }

  /* Request initialisation */
  SET_BIT(hcan->Instance->MCR, CAN_MCR_INRQ);

  /* Get tick */
  tickstart = HAL_GetTick();

  /* Wait initialisation acknowledge */
  while ((hcan->Instance->MSR & CAN_MSR_INAK) == 0U)
  {
    if ((HAL_GetTick() - tickstart) > CAN_TIMEOUT_VALUE)
    {
      /* Update error code */
      hcan->ErrorCode |= HAL_CAN_ERROR_TIMEOUT;

      /* Change CAN state */
      hcan->State = HAL_CAN_STATE_ERROR;

      return HAL_ERROR;
    }
  }

  /* Set the time triggered communication mode */
  if (hcan->Init.TimeTriggeredMode == ENABLE)
  {
    SET_BIT(hcan->Instance->MCR, CAN_MCR_TTCM);
  }
  else
  {
    CLEAR_BIT(hcan->Instance->MCR, CAN_MCR_TTCM);
  }

  /* Set the automatic bus-off management */
  if (hcan->Init.AutoBusOff == ENABLE)
  {
    SET_BIT(hcan->Instance->MCR, CAN_MCR_ABOM);
  }
  else
  {
    CLEAR_BIT(hcan->Instance->MCR, CAN_MCR_ABOM);
  }

  /* Set the automatic wake-up mode */
  if (hcan->Init.AutoWakeUp == ENABLE)
  {
    SET_BIT(hcan->Instance->MCR, CAN_MCR_AWUM);
  }
  else
  {
    CLEAR_BIT(hcan->Instance->MCR, CAN_MCR_AWUM);
  }

  /* Set the automatic retransmission */
  if (hcan->Init.AutoRetransmission == ENABLE)
  {
    CLEAR_BIT(hcan->Instance->MCR, CAN_MCR_NART);
  }
  else
  {
    SET_BIT(hcan->Instance->MCR, CAN_MCR_NART);
  }

  /* Set the receive FIFO locked mode */
  if (hcan->Init.ReceiveFifoLocked == ENABLE)
  {
    SET_BIT(hcan->Instance->MCR, CAN_MCR_RFLM);
  }
  else
  {
    CLEAR_BIT(hcan->Instance->MCR, CAN_MCR_RFLM);
  }

  /* Set the transmit FIFO priority */
  if (hcan->Init.TransmitFifoPriority == ENABLE)
  {
    SET_BIT(hcan->Instance->MCR, CAN_MCR_TXFP);
  }
  else
  {
    CLEAR_BIT(hcan->Instance->MCR, CAN_MCR_TXFP);
  }

  /* Set the bit timing register */
  WRITE_REG(hcan->Instance->BTR, (uint32_t)(hcan->Init.Mode           |
                                            hcan->Init.SyncJumpWidth  |
                                            hcan->Init.TimeSeg1       |
                                            hcan->Init.TimeSeg2       |
                                            (hcan->Init.Prescaler - 1U)));

  /* Initialize the error code */
  hcan->ErrorCode = HAL_CAN_ERROR_NONE;

  /* Initialize the CAN state */
  hcan->State = HAL_CAN_STATE_READY;

  /* Return function status */
  return HAL_OK;
}
#endif

#if 0
static void MX_CAN1_Init(void)
{

  /* USER CODE BEGIN CAN1_Init 0 */

  /* USER CODE END CAN1_Init 0 */

  /* USER CODE BEGIN CAN1_Init 1 */

  /* USER CODE END CAN1_Init 1 */
  hcan1.Instance = CAN1;
  hcan1.Init.Prescaler = 40;
  hcan1.Init.Mode = CAN_MODE_NORMAL;
  hcan1.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan1.Init.TimeSeg1 = CAN_BS1_13TQ;
  hcan1.Init.TimeSeg2 = CAN_BS2_2TQ;
  hcan1.Init.TimeTriggeredMode = DISABLE;
  hcan1.Init.AutoBusOff = DISABLE;
  hcan1.Init.AutoWakeUp = DISABLE;
  hcan1.Init.AutoRetransmission = DISABLE;
  hcan1.Init.ReceiveFifoLocked = DISABLE;
  hcan1.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN1_Init 2 */

  /* USER CODE END CAN1_Init 2 */

}
#endif
