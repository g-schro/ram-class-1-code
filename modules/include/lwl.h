#ifndef _LWL_H_
#define _LWL_H_

/*
 * @brief Interface declaration of lwl module.
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

#include <stdbool.h>
#include <stdint.h>

////////////////////////////////////////////////////////////////////////////////
// Common macros
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Type definitions
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Public (global) externs
////////////////////////////////////////////////////////////////////////////////

// Following variable is global to allow efficient access by macros,
// but it is considered private.

extern bool _lwl_active;

////////////////////////////////////////////////////////////////////////////////
// Public (global) function declarations
////////////////////////////////////////////////////////////////////////////////

// Core module interface functions.
int32_t lwl_start(void);

// Other APIs.
void lwl_rec(uint8_t id, int32_t num_arg_bytes, ...);
void lwl_enable(bool on);
void lwl_dump(void);
uint8_t* lwl_get_buffer(uint32_t* len);

// The special __COUNTER__ macro (not official C but supported by many
// compilers) is used to generate LWL IDs.

#define LWL(fmt, num_arg_bytes, ...) LWL_CNT(__COUNTER__, fmt, num_arg_bytes, ##__VA_ARGS__)

#define LWL_CNT(counter, fmt, num_arg_bytes, ...) do {                  \
        _Static_assert((counter) < LWL_NUM);                            \
        if (_lwl_active)                                                \
            lwl_rec(LWL_BASE_ID+(counter), num_arg_bytes, ##__VA_ARGS__); \
    } while (0)

// The argument macros convert arguments to bytes, which makes copying them to the
// circular buffer efficient.

#define LWL_1(a) (uint32_t)(a)
#define LWL_2(a) (uint32_t)(a) >> 8,  (uint32_t)(a)
#define LWL_3(a) (uint32_t)(a) >> 16, (uint32_t)(a) >> 8,  (uint32_t)(a)
#define LWL_4(a) (uint32_t)(a) >> 24, (uint32_t)(a) >> 16, (uint32_t)(a) >> 8, (uint32_t)(a)

#endif // _LWL_H_
