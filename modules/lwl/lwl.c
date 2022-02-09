/*
 * @brief Implementation of lwl module.
 *
 * This module provides a "lightweight log" feature. The logs are very compact,
 * and are stored in a circular buffer. A python program prints "user friendly"
 * log output.
 *
 * The following console commands are provided:
 * > lwl status
 * > lwl test
 * > lwl on
 * > lwl dump
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

#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#include "cmd.h"
#include "console.h"
#include "log.h"
#include "lwl.h"
#include "module.h"

////////////////////////////////////////////////////////////////////////////////
// Common macros
////////////////////////////////////////////////////////////////////////////////

#define LWL_BASE_ID 1
#define LWL_NUM 4

#ifdef CONFIG_LWL_BUF_SIZE
    #define LWL_BUF_SIZE (CONFIG_LWL_BUF_SIZE)
#else
    #define LWL_BUF_SIZE 1008
#endif

////////////////////////////////////////////////////////////////////////////////
// Type definitions
////////////////////////////////////////////////////////////////////////////////

// For writing to flash, this structure needs to be a multiple of 8 bytes.
struct lwl_data
{
    uint32_t magic;
    uint32_t num_section_bytes;
    uint32_t buf_size;
    uint32_t put_idx;
    uint8_t buf[LWL_BUF_SIZE];
};

_Static_assert((sizeof(struct lwl_data) % CONFIG_FLASH_WRITE_BYTES) == 0,
               "struct lwl_data not a multiple of flash write size");

////////////////////////////////////////////////////////////////////////////////
// Private (static) function declarations
////////////////////////////////////////////////////////////////////////////////

static void prepare_data_for_output(void);
static int32_t cmd_lwl_status(int32_t argc, const char** argv);
static int32_t cmd_lwl_test(int32_t argc, const char** argv);
static int32_t cmd_lwl_enable(int32_t argc, const char** argv);
static int32_t cmd_lwl_dump(int32_t argc, const char** argv);

////////////////////////////////////////////////////////////////////////////////
// Private (static) variables
////////////////////////////////////////////////////////////////////////////////

static struct lwl_data lwl_data;

static int32_t log_level = LOG_DEFAULT;

// Data structure with console command info.
static struct cmd_cmd_info cmds[] = {
    {
        .name = "status",
        .func = cmd_lwl_status,
        .help = "Status",
    },
    {
        .name = "test",
        .func = cmd_lwl_test,
        .help = "Test logging",
    },
    {
        .name = "enable",
        .func = cmd_lwl_enable,
        .help = "Recording on/off (1/0)",
    },
    {
        .name = "dump",
        .func = cmd_lwl_dump,
        .help = "Dump buffer",
    },
};

// Data structure passed to cmd module for console interaction.
static struct cmd_client_info cmd_info = {
    .name = "lwl",
    .num_cmds = ARRAY_SIZE(cmds),
    .cmds = cmds,
    .log_level_ptr = &log_level,
};

////////////////////////////////////////////////////////////////////////////////
// Public (global) variables and externs
////////////////////////////////////////////////////////////////////////////////

bool _lwl_active = false;
uint32_t lwl_off_cnt = 0;

////////////////////////////////////////////////////////////////////////////////
// Public (global) functions
////////////////////////////////////////////////////////////////////////////////

/*
 * @brief Start lwl instance.
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * This function starts the lwl singleton module, to enter normal operation.
 */
int32_t lwl_start(void)
{
    int32_t rc = cmd_register(&cmd_info);
    if (rc < 0) {
        log_error("lwl_start: cmd error %d\n", rc);
        return rc;
    }
    return 0;
}

/*
 * @brief Record a lightweight log.
 *
 * @param[in] id The log ID.
 * @param[in] num_arg_bytes The number of argument bytes (0 if no arguments).
 */
void lwl_rec(uint8_t id, int32_t num_arg_bytes, ...)
{
    CRIT_STATE_VAR;
    va_list ap;
    uint32_t put_idx;

    va_start(ap, num_arg_bytes);

    CRIT_BEGIN_NEST();
    if (lwl_off_cnt != 0 && --lwl_off_cnt == 0)
        _lwl_active = false;
    put_idx = lwl_data.put_idx % LWL_BUF_SIZE;
    lwl_data.put_idx = (put_idx + 1 + num_arg_bytes) % LWL_BUF_SIZE;

    // We could end the critical section here, since the put index has been
    // updated. But there is a chance that a fault occurs before all of the
    // argument bytes have been written, such that the buffer would contain
    // invalid data.
    //
    // For now we play it safe.

    lwl_data.buf[put_idx] = id;
    while (num_arg_bytes-- > 0) {
        put_idx = (put_idx + 1) % LWL_BUF_SIZE;
        lwl_data.buf[put_idx] = (uint8_t)va_arg(ap, unsigned);
    }
    CRIT_END_NEST();
}

/*
 * @brief Enable/disable lightweight logs.
 *
 * @param[in] on Enable/disable.
 */
void lwl_enable(bool on)
{
    _lwl_active = on;
}

/*
 * @brief Get LWL buffer information (including header).
 *
 * @param[out] len Number of data bytes.
 *
 * @return Address of buffer.
 */
uint8_t* lwl_get_buffer(uint32_t* len)
{
    prepare_data_for_output();
    *len = sizeof(lwl_data);
    return (uint8_t*)&lwl_data;
}

////////////////////////////////////////////////////////////////////////////////
// Private (static) functions
////////////////////////////////////////////////////////////////////////////////

/*
 * @brief Prepare data to be output.
 */
static void prepare_data_for_output(void)
{
    lwl_data.magic = MOD_MAGIC_LWL;
    lwl_data.num_section_bytes = sizeof(lwl_data);
    lwl_data.buf_size = LWL_BUF_SIZE;
}

/*
 * @brief Console command function for "lwl status".
 *
 * @param[in] argc Number of arguments, including "lwl"
 * @param[in] argv Argument values, including "lwl"
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * Command usage: lwl status
 */
static int32_t cmd_lwl_status(int32_t argc, const char** argv)
{
    printc("on=%d put_idx=%lu\n", _lwl_active, lwl_data.put_idx);
    return 0;
}

/*
 * @brief Console command function for "lwl test".
 *
 * @param[in] argc Number of arguments, including "lwl"
 * @param[in] argv Argument values, including "lwl"
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * Command usage: lwl test
 */
static int32_t cmd_lwl_test(int32_t argc, const char** argv)
{
    LWL("test 1", 0);
    LWL("test 2 %d", 1, LWL_1(10));
    LWL("test 3 %d %d", 3, LWL_1(10), LWL_2(1000));
    LWL("test 4 %d %d %d", 7, LWL_1(10), LWL_2(1000), LWL_4(100000));

    lwl_enable(false);
    return 0;
}

/*
 * @brief Console command function for "lwl enable".
 *
 * @param[in] argc Number of arguments, including "lwl"
 * @param[in] argv Argument values, including "lwl"
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * Command usage: lwl on {0|1}
 */
static int32_t cmd_lwl_enable(int32_t argc, const char** argv)
{
    struct cmd_arg_val arg_vals[1];

    if (argc != 3)
    {
        printc("Invalid arguments\n");
        return MOD_ERR_ARG;
    }
        
    if (cmd_parse_args(argc-2, argv+2, "u", arg_vals) != 1)
            return MOD_ERR_BAD_CMD;

    lwl_enable(arg_vals[0].val.u != 0);

    return 0;
}

/*
 * @brief Console command function for "lwl dump".
 *
 * @param[in] argc Number of arguments, including "lwl"
 * @param[in] argv Argument values, including "lwl"
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * Command usage: lwl dump
 */
static int32_t cmd_lwl_dump(int32_t argc, const char** argv)
{
    prepare_data_for_output();
    console_data_print((uint8_t*)&lwl_data, sizeof(lwl_data));
    return 0;
}
