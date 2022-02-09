#ifndef _CAN_H_
#define _CAN_H_

/*
 * @brief Interface declaration of can module.
 *
 * See implementation file for information about this module.
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

#include "module.h"
#include "config.h"

#include "dio.h"

////////////////////////////////////////////////////////////////////////////////
// Common macros
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Type definitions
////////////////////////////////////////////////////////////////////////////////

enum can_instance_id {

#if CONFIG_CAN_1_PRESENT
     CAN_INSTANCE_1,

#endif
#if CONFIG_CAN_2_PRESENT
     CAN_INSTANCE_2,
#endif

     CAN_NUM_INSTANCES
};

struct can_cfg
{
    dio_port* can_tx_pin_port;
    uint32_t can_tx_pin;
    dio_port* can_rx_pin_port;
    uint32_t can_rx_pin;

    // FUTURE.
};

////////////////////////////////////////////////////////////////////////////////
// Public (global) externs
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Public (global) function declarations
////////////////////////////////////////////////////////////////////////////////

// Core module interface functions.
int32_t can_get_def_cfg(enum can_instance_id instance_id, struct can_cfg* cfg);
int32_t can_init(enum can_instance_id instance_id, struct can_cfg* cfg);
int32_t can_start(enum can_instance_id instance_id);
int32_t can_run(enum can_instance_id instance_id);

// Other APIs.

#endif // _CAN_H_
