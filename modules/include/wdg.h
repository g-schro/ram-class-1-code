#ifndef _WDG_H_
#define _WDG_H_

/*
 * @brief Interface declaration of wdg module.
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

////////////////////////////////////////////////////////////////////////////////
// Common macros
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Type definitions
////////////////////////////////////////////////////////////////////////////////

typedef void (*wdg_triggered_cb)(uint32_t wdg_id);

struct wdg_cfg
{
    // FUTURE.
};

////////////////////////////////////////////////////////////////////////////////
// Public (global) externs
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Public (global) function declarations
////////////////////////////////////////////////////////////////////////////////

// Core module interface functions.
int32_t wdg_init(struct wdg_cfg* cfg);
int32_t wdg_start(void);

// Other APIs.
int32_t wdg_register(uint32_t wdg_id, uint32_t period_ms);
int32_t wdg_feed(uint32_t wdg_id);
int32_t wdg_register_triggered_cb(wdg_triggered_cb triggered_cb);
void wdg_start_init_hdw_wdg(void);
void wdg_init_successful(void);
int32_t wdg_start_hdw_wdg(uint32_t timeout_ms);
void wdg_feed_hdw(void);

#endif // _WDG_H_
