/*
 * @brief Implementation of wdg module.
 *
 * This module provides a watchdog service. It supports a configurable number
 * of software-based watchdogs. A hardware-based watchdog is used to verify the
 * software-based watchdogs are operating correctly.
 *
 * The following console commands are provided:
 * > wdg status
 * > wdg test
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
#include CONFIG_STM32_LL_IWDG_HDR

#include "cmd.h"
#include "console.h"
#include "fault.h"
#include "log.h"
#include "module.h"
#include "tmr.h"
#include "wdg.h"

////////////////////////////////////////////////////////////////////////////////
// Common macros
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Type definitions
////////////////////////////////////////////////////////////////////////////////

struct soft_wdg
{
    uint32_t period_ms;
    uint32_t last_feed_time_ms;
};

struct wdg_state
{
    struct soft_wdg soft_wdgs[CONFIG_WDG_NUM_WDGS];
    wdg_triggered_cb triggered_cb;
};

struct wdg_no_init_vars {
    uint32_t magic;
    uint32_t consec_failed_init_ctr;
    uint32_t check;
};

#define WDG_NO_INIT_VARS_MAGIC 0xdeaddead

////////////////////////////////////////////////////////////////////////////////
// Private (static) function declarations
////////////////////////////////////////////////////////////////////////////////

static enum tmr_cb_action wdg_tmr_cb(int32_t tmr_id, uint32_t user_data);
static void validate_no_init_vars(void);
static void update_no_init_vars(void);
static int32_t cmd_wdg_status(int32_t argc, const char** argv);
static int32_t cmd_wdg_test(int32_t argc, const char** argv);

////////////////////////////////////////////////////////////////////////////////
// Private (static) variables
////////////////////////////////////////////////////////////////////////////////

struct wdg_state state;

static int32_t log_level = LOG_DEFAULT;

// Data structure with console command info.
static struct cmd_cmd_info cmds[] = {
    {
        .name = "status",
        .func = cmd_wdg_status,
        .help = "Get module status, usage: wdg status",
    },
    {
        .name = "test",
        .func = cmd_wdg_test,
        .help = "Run test, usage: wdg test [<op> [<arg>]] (enter no op for help)",
    }
};

// Data structure passed to cmd module for console interaction.
static struct cmd_client_info cmd_info = {
    .name = "wdg",
    .num_cmds = ARRAY_SIZE(cmds),
    .cmds = cmds,
    .log_level_ptr = &log_level,
};

static bool test_cmd_fail_hard_wdg = false;
static bool test_cmd_disable_wdg = false;

struct wdg_no_init_vars no_init_vars __attribute__((section (".no.init.vars")));

////////////////////////////////////////////////////////////////////////////////
// Public (global) variables and externs
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Public (global) functions
////////////////////////////////////////////////////////////////////////////////

/*
 * @brief Initialize wdg instance.
 *
 * @param[in] cfg The wdg configuration. (FUTURE)
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * This function initializes the wdg singleton module. Generally, it should
 * not access other modules as they might not have been initialized yet. An
 * exception is the log module.
 */
int32_t wdg_init(struct wdg_cfg* cfg)
{
    memset(&state, 0, sizeof(state));
    return 0;
}

/*
 * @brief Start wdg instance.
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * This function starts the wdg singleton module, to enter normal operation.
 */
int32_t wdg_start(void)
{
    int32_t rc;

    rc = cmd_register(&cmd_info);
    if (rc < 0) {
        log_error("wdg_start: cmd error %d\n", rc);
        goto exit;
    }

    rc = tmr_inst_get_cb(CONFIG_WDG_RUN_CHECK_MS,
                         wdg_tmr_cb, 0, TMR_CNTX_BASE_LEVEL);
    if (rc < 0) {
        log_error("wdg_start: tmr error %d\n", rc);
        goto exit;
    }

exit:
    return rc;
}

/*
 * @brief Client registration.
 *
 * @param[in] wdg_id The sofware-based watchdog ID.
 * @param[in] period_ms The watchdog timeout period.
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 */
int32_t wdg_register(uint32_t wdg_id, uint32_t period_ms)
{
    struct soft_wdg* soft_wdg;

    if (wdg_id >= CONFIG_WDG_NUM_WDGS)
        return MOD_ERR_ARG;

    soft_wdg = &state.soft_wdgs[wdg_id];
    soft_wdg->last_feed_time_ms = tmr_get_ms();
    soft_wdg->period_ms = period_ms;

    return 0;
}

/*
 * @brief Feed a software-based watchdog.
 *
 * @param[in] wdg_id The sofware-based watchdog ID.
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 */
int32_t wdg_feed(uint32_t wdg_id)
{
    if (wdg_id >= CONFIG_WDG_NUM_WDGS)
        return MOD_ERR_ARG;
    state.soft_wdgs[wdg_id].last_feed_time_ms = tmr_get_ms();
    return 0;
}

/*
 * @brief Register to receive a callback when any watchdog triggers.
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 */
int32_t wdg_register_triggered_cb(wdg_triggered_cb triggered_cb)
{
    state.triggered_cb = triggered_cb;
    return 0;
}

/*
 * @brief Start the hardware watchdog timer for initialization.
 *
 * The hardware watchdog timer is used to ensure that initialization processing
 * (i.e. work done before the super loop starts) does not get stuck. In other
 * words, if initialization takes too long, the MCU will be reset to try
 * again. However, if we get over N consecutive failed initializations, we do
 * not start the watchdog, to give the system a chance to initialize with as
 * much time as it needs.
 */
void wdg_start_init_hdw_wdg(void)
{
    validate_no_init_vars();

    if ((fault_get_rcc_csr() & RCC_CSR_IWDGRSTF_Msk) == 0) {
        // Reset was not due to IWDG, so we reset the counter.
        no_init_vars.consec_failed_init_ctr = 0;
    }
    if (no_init_vars.consec_failed_init_ctr < CONFIG_WDG_MAX_INIT_FAILS ||
        CONFIG_WDG_MAX_INIT_FAILS == 0) {
        wdg_start_hdw_wdg(CONFIG_WDG_INIT_TIMEOUT_MS);
    }
    no_init_vars.consec_failed_init_ctr++;
    update_no_init_vars();
}

/*
 * @brief Indicate a succesful initialization.
 *
 * This function is called when initialization has successfully completed. It
 * just resets the "consecutive failed initializations" counter.
 */
void wdg_init_successful(void)
{
    validate_no_init_vars();
    no_init_vars.consec_failed_init_ctr = 0;
    update_no_init_vars();
}

/*
 * @brief Initialise and start hardware-based watchdog.
 * 
 * @param[in] timeout_ms The hardware-based watchdog timeout period.
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * @note This code based on generated startup code.
 *
 * The input clock to the hardware-based watchdog depends on the MPU:
 *
 * - stm32u575zi: LSI: 32 kHz or 250 Hz, based on LSI divider setting.
 * - stm32f401re: LSI: 32 kHz.
 * - stm32l452re: LSI: 32 kHz.
 *
 * For now we assume LSI is 32 kHZ, but this could be generalized. For example,
 * if there is an LSI divider, we could read that setting.
 *
 * We choose a divider of 64 -- this gives 2 ms resolution. This gives a maximum
 * timeout of 8192 ms.
 */
int32_t wdg_start_hdw_wdg(uint32_t timeout_ms)
{
    int32_t ctr;

    #define SANITY_CTR_LIMIT 1000000
    #define LSI_FREQ_HZ 32000
    #define WDG_PRESCALE 64
    #define WDG_PRESCALE_SETTING LL_IWDG_PRESCALER_64
    #define WDG_CLK_FREQ_HZ (LSI_FREQ_HZ/WDG_PRESCALE)
    #define WDG_MAX_RL 0xfff
    #define MS_PER_SEC 1000
    #define WDG_MS_TO_RL(ms) \
        (((ms) * WDG_CLK_FREQ_HZ + MS_PER_SEC/2)/MS_PER_SEC - 1)

    _Static_assert(CONFIG_WDG_HARD_TIMEOUT_MS <=
                   ((WDG_MAX_RL + 1) * 1000) / WDG_CLK_FREQ_HZ,
                   "Watchdog timeout too large");

    ctr = WDG_MS_TO_RL(timeout_ms);
    if (ctr < 0)
        ctr = 0;
    else if (ctr > WDG_MAX_RL)
        return MOD_ERR_ARG;

    LL_IWDG_Enable(IWDG);
    LL_IWDG_EnableWriteAccess(IWDG);
    LL_IWDG_SetPrescaler(IWDG, WDG_PRESCALE_SETTING); 
    LL_IWDG_SetReloadCounter(IWDG, ctr);
    for (ctr = 0; ctr < SANITY_CTR_LIMIT; ctr++) {
        if (LL_IWDG_IsReady(IWDG))
            break;
    }
    if (ctr >= SANITY_CTR_LIMIT)
        return MOD_ERR_PERIPH;

    // Stop the watchdog counter when the debugger stops the MCU.
    #ifdef DBGMCU_APB1FZR1_DBG_IWDG_STOP_Msk
        DBGMCU->APB1FZR1 |= DBGMCU_APB1FZR1_DBG_IWDG_STOP_Msk;
    #elif defined DBGMCU_APB1_FZ_DBG_IWDG_STOP_Msk
        DBGMCU->APB1FZ |= DBGMCU_APB1_FZ_DBG_IWDG_STOP_Msk;
    #endif

    return 0;
}

/*
 * @brief Feed the hardware-based watchdog.
 */
void wdg_feed_hdw(void)
{
    LL_IWDG_ReloadCounter(IWDG);
}

////////////////////////////////////////////////////////////////////////////////
// Private (static) functions
////////////////////////////////////////////////////////////////////////////////

/*
 * @brief Timer callback used to check software-based watchdogs.
 *
 * @param[in] tmr_id The timer ID (not used).
 * @param[in] user_data  User data for this timer (not used).
 *
 * @return TMR_CB_RESTART so that it is periodically called.
 *
 * This callback checks the software-based watchdogs. It is also the one place
 * where the hardware watchdog is fed. So if this callback does not run, the
 * system will be reset by the hardware-based watchdog. In this way the
 * following functions are monitored:
 *
 * - The timer service (at least BASE_LEVEL timers).
 * - The superloop, since BASE_LEVEL timers are called from the super loop.
 */

static enum tmr_cb_action wdg_tmr_cb(int32_t tmr_id,
                                     uint32_t user_data)
{
    int32_t idx;
    struct soft_wdg* soft_wdg;
    bool wdg_triggered = false;

    if (test_cmd_disable_wdg) {
        wdg_feed_hdw();
        goto exit;
    }

    for (idx = 0, soft_wdg = &state.soft_wdgs[0];
         idx < CONFIG_WDG_NUM_WDGS;
         idx++, soft_wdg++)
    {
        if (soft_wdg->period_ms != 0) {

            // We have to careful with race conditions, especially for
            // watchdogs fed from interrupt handlers.

            uint32_t last_feed_time_ms = soft_wdg->last_feed_time_ms;
            if (tmr_get_ms() - last_feed_time_ms > soft_wdg->period_ms) {
                wdg_triggered = true;
                if (state.triggered_cb != NULL) {
                    // This function will normally not return.
                    state.triggered_cb(idx);
                }
            }
        }
    }

    if (!wdg_triggered) {
        if (!test_cmd_fail_hard_wdg)
            wdg_feed_hdw();
    }

exit:
    return TMR_CB_RESTART;
}

/*
 * @brief Validate the "no-init-vars" block and initialize if needed.
 */
static void validate_no_init_vars(void)
{
    static const uint32_t num_u32_to_check = 
        sizeof(struct wdg_no_init_vars)/sizeof(uint32_t) - 1;
    uint32_t idx;
    uint32_t new_check = 0xBAADCEED; // Seed.

    for (idx = 0; idx < num_u32_to_check; idx++) {
        new_check = ((new_check << 1) | (new_check >> 31)) ^
            ((uint32_t*)&no_init_vars)[idx];
    }

    if (no_init_vars.magic != WDG_NO_INIT_VARS_MAGIC ||
        no_init_vars.check != new_check)
    {
        memset(&no_init_vars, 0, sizeof(no_init_vars));
        no_init_vars.magic = WDG_NO_INIT_VARS_MAGIC;
        no_init_vars.check = new_check;
    }
}

/*
 * @brief Recompute the check value on the "no-init-vars" block.
 */
static void update_no_init_vars(void)
{
    static const uint32_t num_u32_to_check = 
        sizeof(struct wdg_no_init_vars)/sizeof(uint32_t) - 1;
    uint32_t idx;
    uint32_t new_check = 0xBAADCEED; // Seed.

    for (idx = 0; idx < num_u32_to_check; idx++) {
        new_check = ((new_check << 1) | (new_check >> 31)) ^
            ((uint32_t*)&no_init_vars)[idx];
    }
    no_init_vars.check = new_check;
}


/*
 * @brief Console command function for "wdg status".
 *
 * @param[in] argc Number of arguments, including "wdg"
 * @param[in] argv Argument values, including "wdg"
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * Command usage: wdg test [<op> [<arg>]]
 */
static int32_t cmd_wdg_status(int32_t argc, const char** argv)
{
    uint32_t id;

    printc("Current time: %10lu\nWatchdog %s.\n",
           tmr_get_ms(),
           test_cmd_disable_wdg ? "disabled" : "enabled");
    printc("consec_failed_init_ctr=%lu\n", no_init_vars.consec_failed_init_ctr);

    printc("\nID  PERIOD LAST_FEED  ELAPSED\n"
             "--- ------ ---------- -------\n");
    for (id = 0; id < ARRAY_SIZE(state.soft_wdgs); id++) {
        struct soft_wdg* c = &state.soft_wdgs[id];
        printc("%3lu %6lu %10lu %7ld\n", id, c->period_ms, c->last_feed_time_ms,
               tmr_get_ms() - c->last_feed_time_ms);
    }
     return 0;
}

/*
 * @brief Console command function for "wdg test".
 *
 * @param[in] argc Number of arguments, including "wdg"
 * @param[in] argv Argument values, including "wdg"
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * Command usage: wdg test [<op> [<arg>]]
 */
static int32_t cmd_wdg_test(int32_t argc, const char** argv)
{
    int32_t num_args;
    struct cmd_arg_val arg_vals[1];

    // Handle help case.
    if (argc == 2) {
        printc("Test operations and param(s) are as follows:\n"
               "  Fail hardware wdg: usage: wdg test fail-hdw\n"
               "  Disable wdg: usage: wdg test disable\n"
               "  Enable wdg: usage: wdg test enable\n"
               "  Set init fails: usage: wdg test init-fails N\n"
            );
        return 0;
    }

    if (strcasecmp(argv[2], "fail-hdw") == 0) {
        test_cmd_fail_hard_wdg = true;
    } else if (strcasecmp(argv[2], "disable") == 0) {
        test_cmd_disable_wdg = true;
    } else if (strcasecmp(argv[2], "enable") == 0) {
        test_cmd_disable_wdg = false;
    } else if (strcasecmp(argv[2], "init-fails") == 0) {
        num_args = cmd_parse_args(argc-3, argv+3, "u", arg_vals);
        if (num_args == 1) {
            no_init_vars.consec_failed_init_ctr = arg_vals[0].val.u;
            update_no_init_vars();
        }
    } else {
        printc("Invalid test '%s'\n", argv[2]);
        return MOD_ERR_BAD_CMD;
    }
    return 0;
}
