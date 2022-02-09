/*
 * @brief Implementation of os module.
 *
 * This module ...
 *
 * The following console commands are provided:
 * > os status
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
#include CONFIG_STM32_LL_CORTEX_HDR 

#include "cmd.h"
#include "console.h"
#include "log.h"
#include "module.h"
#include "tmr.h"

#include "os.h"

////////////////////////////////////////////////////////////////////////////////
// Common macros
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Type definitions
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Private (static) function declarations
////////////////////////////////////////////////////////////////////////////////

static enum tmr_cb_action timer_callback(int32_t tmr_id, uint32_t user_data);
static int32_t cmd_os_status(int32_t argc, const char** argv);
static int32_t cmd_os_test(int32_t argc, const char** argv);

////////////////////////////////////////////////////////////////////////////////
// Private (static) variables
////////////////////////////////////////////////////////////////////////////////

static int32_t log_level = LOG_DEFAULT;

// Data structure with console command info.
static struct cmd_cmd_info cmds[] = {
    {
        .name = "status",
        .func = cmd_os_status,
        .help = "Get module status, usage: os status",
    },
    {
        .name = "test",
        .func = cmd_os_test,
        .help = "Run test, usage: os test [<op> [<arg>]] (enter no op for help)",
    }
};

// Data structure passed to cmd module for console interaction.
static struct cmd_client_info cmd_info = {
    .name = "os",
    .num_cmds = ARRAY_SIZE(cmds),
    .cmds = cmds,
    .log_level_ptr = &log_level,
};

static bool get_systick_basepri = false;

////////////////////////////////////////////////////////////////////////////////
// Public (global) variables and externs
////////////////////////////////////////////////////////////////////////////////

extern void* g_pfnVectors[];

////////////////////////////////////////////////////////////////////////////////
// Public (glg_bal) functions
////////////////////////////////////////////////////////////////////////////////

/*
 * @brief Get default os configuration.
 *
 * @param[out] cfg The os configuration with defaults filled in.
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 */
int32_t os_get_def_cfg(struct os_cfg* cfg)
{
    return 0;
}

/*
 * @brief Initialize os instance.
 *
 * @param[in] cfg The os configuration. (FUTURE)
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * This function initializes the os singleton module. Generally, it should
 * not access other modules as they might not have been initialized yet. An
 * exception is the log module.
 */
int32_t os_init(struct os_cfg* cfg)
{
    NVIC_SetPriorityGrouping(3); // Make config
    return 0;
}

/*
 * @brief Start os instance.
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * This function starts the os singleton module, to enter normal operation.
 */
int32_t os_start(void)
{
    int32_t rc;

    rc = cmd_register(&cmd_info);
    if (rc < 0) {
        log_error("draw_start: cmd error %d\n", rc);
        return rc;
    }

    rc = tmr_inst_get_cb(1002, timer_callback, 0, TMR_CNTX_INTERRUPT);
    if (rc < 0) {
        log_error("os_start: tmr error %d\n", rc);
        return rc;
    }
    return 0;
}

/*
 * @brief Run os instance.
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * @note This function should not block.
 *
 * This function runs the os singleton module, during normal operation.
 */
int32_t os_run(void)
{
    return 0;
}

void os_dump(const char* tag)
{
    if (tag != NULL)
        printc("%s:\n", tag);
    printc(" BASEPRI=%lu PRIGROUP=%lu ICSR=0x%08lx IPSR=%lu\n",
           __get_BASEPRI(),
           NVIC_GetPriorityGrouping(),
           SCB->ICSR,
           __get_IPSR());
}

////////////////////////////////////////////////////////////////////////////////
// Private (static) functions
////////////////////////////////////////////////////////////////////////////////

/*
 * @brief Step timer callback.
 *
 * @param[in] tmr_id Timer ID.
 * @param[in] user_data User callback data (is instance ID)
 *
 * @return TMR_CB_NONE/TMR_CB_RESTART
 *
 * This function handles any current motion in progress, and also handles
 * starting new commands in the qeuue.
 */
static enum tmr_cb_action timer_callback(int32_t tmr_id, uint32_t user_data)
{
    if (get_systick_basepri) {
        get_systick_basepri = false;
        os_dump("\nsystick");
    }
    return TMR_CB_RESTART;
}

/*
 * @brief Console command function for "os status".
 *
 * @param[in] argc Number of arguments, including "os"
 * @param[in] argv Argument values, including "os"
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * Command usage: os status
 */
static int32_t cmd_os_status(int32_t argc, const char** argv)
{
    os_dump("cmd");
    return 0;
}

/*
 * @brief Console command function for "os test".
 *
 * @param[in] argc Number of arguments, including "os" (must be >= 2)
 * @param[in] argv Argument values, including "os"
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * Command usage: os test [<op> [<arg>]]
 */
static int32_t cmd_os_test(int32_t argc, const char** argv)
{
    int32_t rc = 0;
    int32_t idx;
    struct cmd_arg_val arg_vals[2];

    // Handle help case.
    if (argc == 2) {
        printc("Test operations and param(s) are as follows:\n"
               "  Set BASEPRI, usage: os test basepri <value>\n"
               "  Dump info from systick, usage: os test systick\n"
               "  Dump exception priorities, usage: os test excpri\n");
        printc("  Set priority, usage: os test setpri <irqn_type <priority>\n");
        return 0;
    }

    if (strcasecmp(argv[2], "basepri") == 0) {
        if (cmd_parse_args(argc-3, argv+3, "u", arg_vals) != 1) {
            return MOD_ERR_BAD_CMD;
        }
        __set_BASEPRI(arg_vals[0].val.u);
    } else if (strcasecmp(argv[2], "systick") == 0) {
        get_systick_basepri = true;
    } else if (strcasecmp(argv[2], "excpri") == 0) {
        printc("Exc IRQn\n"
               "Num Type Pri\n"
               "--- ---- ---\n");
        // We only print the priorities for exceptions that have non-NULL
        // vectors.  The range of IRQ numbers varies depending on the MCU. There
        // is an fixed offset between the IRQ number and the interrupt vector
        // table index.
        #define irqn idx
        for (irqn = CONFIG_OS_CFG_IRQN_TYPE_MIN;
             irqn <= CONFIG_OS_CFG_IRQN_TYPE_MAX;
             irqn++) {
            int32_t exc_num = irqn + CONFIG_OS_IRQN_TYPE_EXC_NUM_OFFSET;
            if (exc_num >= 1 && g_pfnVectors[exc_num] == NULL)
                continue;
            printf("%3ld %4ld %3lu\n", exc_num, irqn, __NVIC_GetPriority(irqn));
        }
    } else if (strcasecmp(argv[2], "setpri") == 0) {
        if (cmd_parse_args(argc-3, argv+3, "iu", arg_vals) != 2) {
            return MOD_ERR_BAD_CMD;
        }
        __NVIC_SetPriority(arg_vals[0].val.i, arg_vals[1].val.u);
    } else {
        printc("Invalid operation '%s'\n", argv[2]);
        return MOD_ERR_BAD_CMD;
    }
    printc("Result code =%ld\n", rc);
    return 0;
}
