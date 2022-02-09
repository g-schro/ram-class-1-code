/*
 * @brief Main application file
 *
 * This file is the main application file that initializes and starts the various
 * modules and then runs the super loop.
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

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include CONFIG_STM32_LL_RCC_HDR // Needed for IDE bug (see below).

#include "blinky.h"
#include "cmd.h"
#include "console.h"
#include "dio.h"
#include "draw.h"
#include "fault.h"
#include "flash.h"
#include "float.h"
#include "gps_gtu7.h"
#include "i2c.h"
#include "log.h"
#include "lwl.h"
#include "mem.h"
#include "module.h"
#include "os.h"
#include "stat.h"
#include "step.h"
#include "tmphm.h"
#include "ttys.h"
#include "tmr.h"
#include "wdg.h"

////////////////////////////////////////////////////////////////////////////////
// Common macros
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Type definitions
////////////////////////////////////////////////////////////////////////////////

#define MOD_NO_INSTANCE -1

typedef int32_t (*mod_get_def_cfg)(void* cfg);
typedef int32_t (*mod_init)(void* cfg);
typedef int32_t (*mod_start)(void);
typedef int32_t (*mod_run)(void);

typedef int32_t (*mod_instance_get_def_cfg)(int instanced, void* cfg);
typedef int32_t (*mod_instance_init)(int instance, void* cfg);
typedef int32_t (*mod_instance_start)(int instance);
typedef int32_t (*mod_instance_run)(int instance);

struct mod_info {
    const char* name;
    int instance;
    union {
        struct {
            mod_get_def_cfg mod_get_def_cfg;
            mod_init mod_init;
            mod_start mod_start;
            mod_run mod_run;
        } singleton;
        struct {
            mod_instance_get_def_cfg mod_get_def_cfg;
            mod_instance_init mod_init;
            mod_instance_start mod_start;
            mod_instance_run mod_run;
        } multi_instance;
    } ops;
    void* cfg_obj;
};

enum main_u16_pms {
    CNT_INIT_ERR,
    CNT_START_ERR,
    CNT_RUN_ERR,

    NUM_U16_PMS
};

////////////////////////////////////////////////////////////////////////////////
// Private (static) function declarations
////////////////////////////////////////////////////////////////////////////////

static int32_t cmd_main_status();

////////////////////////////////////////////////////////////////////////////////
// Private (static) variables
////////////////////////////////////////////////////////////////////////////////

static int32_t log_level = LOG_DEFAULT;

static struct cmd_cmd_info cmds[] = {
    {
        .name = "status",
        .func = cmd_main_status,
        .help = "Get main status, usage: main status [clear]",
    },
};

static uint16_t cnts_u16[NUM_U16_PMS];

static const char* cnts_u16_names[NUM_U16_PMS] = {
    "init err",
    "start err",
    "run err",
};

static struct cmd_client_info cmd_info = {
    .name = "main",
    .num_cmds = ARRAY_SIZE(cmds),
    .cmds = cmds,
    .log_level_ptr = &log_level,
    .num_u16_pms = NUM_U16_PMS,
    .u16_pms = cnts_u16,
    .u16_pm_names = cnts_u16_names,
};

// Board-specific DIO information regarding buttons and LEDs.
//
// These variables must be static because the dio module, during initialization,
// keeps pointers to them, and continues to accesses them after initialization.

#if defined STM32U575xx

enum din_index {
    USER_BUTTON,

    DIN_NUM
};

static struct dio_in_info d_inputs[DIN_NUM] = {
    {
        // User button
        .name = "Button_1",
        .port = DIO_PORT_C,
        .pin = DIO_PIN_13,
        .pull = DIO_PULL_NO,
        .invert = 1,
    },
};

enum dout_index {
    DOUT_LED_GREEN,
    DOUT_LED_RED,
    DOUT_LED_BLUE,

    DOUT_NUM,

    DOUT_LED_BLINKY = DOUT_LED_BLUE,
};

static struct dio_out_info d_outputs[DOUT_NUM] = {
    {
        // LED GREEN
        .name = "LED_GREEN",
        .port = DIO_PORT_C,
        .pin = DIO_PIN_7,
        .pull = DIO_PULL_NO,
        .init_value = 0,
        .speed = DIO_SPEED_FREQ_LOW,
        .output_type = DIO_OUTPUT_PUSHPULL,
    },
    {
        // LED RED
        .name = "LED_RED",
        .port = DIO_PORT_G,
        .pin = DIO_PIN_2,
        .pull = DIO_PULL_NO,
        .init_value = 0,
        .speed = DIO_SPEED_FREQ_LOW,
        .output_type = DIO_OUTPUT_PUSHPULL,
    },
    {
        // LED BLUE
        .name = "LED_BLUE",
        .port = DIO_PORT_B,
        .pin = DIO_PIN_7,
        .pull = DIO_PULL_NO,
        .init_value = 0,
        .speed = DIO_SPEED_FREQ_LOW,
        .output_type = DIO_OUTPUT_PUSHPULL,
    },
};

#else

enum din_index {
    DIN_BUTTON_1,
    DIN_GPS_PPS,

    DIN_NUM,
};

static struct dio_in_info d_inputs[DIN_NUM] = {
    {
        // Button 1
        .name = "Button_1",
        .port = DIO_PORT_C,
        .pin = DIO_PIN_13,
        .pull = DIO_PULL_NO,
        .invert = 1,
    },
    {
        // GPS PPS, connected to PB2 (CN10, pin 22).
        .name = "PPS",
        .port = DIO_PORT_B,
        .pin = DIO_PIN_3,
        .pull = DIO_PULL_NO,
    }
};

enum dout_index {
    DOUT_LED_2,

    DOUT_NUM,


    DOUT_LED_BLINKY = DOUT_LED_2,
};

static struct dio_out_info d_outputs[DOUT_NUM] = {
    {
        // LED 2
        .name = "LED_2",
        .port = DIO_PORT_A,
        .pin = DIO_PIN_5,
        .pull = DIO_PULL_NO,
        .init_value = 0,
        .speed = DIO_SPEED_FREQ_LOW,
        .output_type = DIO_OUTPUT_PUSHPULL,
    }
};

#endif

static struct dio_cfg dio_cfg = {
    .num_inputs = ARRAY_SIZE(d_inputs),
    .inputs = d_inputs,
    .num_outputs = ARRAY_SIZE(d_outputs),
    .outputs = d_outputs,
};

static struct stat_dur stat_loop_dur;

    struct console_cfg console_cfg;

#if CONFIG_GPS_PRESENT
    static struct gps_cfg gps_cfg;
#endif

#if CONFIG_I2C_TYPE == 1 && CONFIG_I2C_3_PRESENT
    static struct i2c_cfg i2c_cfg;
#endif

#if CONFIG_TTYS_1_PRESENT
    static struct ttys_cfg ttys_cfg_1;
#endif

#if CONFIG_TTYS_2_PRESENT
    static struct ttys_cfg ttys_cfg_2;
#endif

#if CONFIG_TTYS_6_PRESENT
    static struct ttys_cfg ttys_cfg_6;
#endif

    static struct blinky_cfg blinky_cfg = {
        .dout_idx = DOUT_LED_BLINKY,
        .code_num_blinks = 5,
        .code_period_ms = 1000,
        .sep_num_blinks = 5,
        .sep_period_ms = 200,
    };

#if CONFIG_I2C_TYPE == 1 && CONFIG_I2C_3_PRESENT
    static struct tmphm_cfg tmphm_cfg;
#endif

#if CONFIG_STEP_1_PRESENT
    static struct step_cfg step_cfg_1;
#endif

#if CONFIG_STEP_2_PRESENT
    static struct step_cfg step_cfg_2;
#endif

#if CONFIG_DRAW_PRESENT
    static struct draw_cfg draw_cfg;
#endif

static struct mod_info mods[] = {

#if CONFIG_TTYS_1_PRESENT
    {
        .name = "ttys",
        .instance = TTYS_INSTANCE_1,
        .ops.multi_instance.mod_get_def_cfg =
            (mod_instance_get_def_cfg)ttys_get_def_cfg,
        .ops.multi_instance.mod_init = (mod_instance_init)ttys_init,
        .ops.multi_instance.mod_start = (mod_instance_start)ttys_start,
        .cfg_obj = &ttys_cfg_1,
    },
#endif

#if CONFIG_TTYS_2_PRESENT
    {
        .name = "ttys",
        .instance = TTYS_INSTANCE_2,
        .ops.multi_instance.mod_get_def_cfg =
            (mod_instance_get_def_cfg)ttys_get_def_cfg,
        .ops.multi_instance.mod_init = (mod_instance_init)ttys_init,
        .ops.multi_instance.mod_start = (mod_instance_start)ttys_start,
        .cfg_obj = &ttys_cfg_2,
    },
#endif

#if CONFIG_TTYS_6_PRESENT
    {
        .name = "ttys",
        .instance = TTYS_INSTANCE_6,
        .ops.multi_instance.mod_get_def_cfg =
            (mod_instance_get_def_cfg)ttys_get_def_cfg,
        .ops.multi_instance.mod_init = (mod_instance_init)ttys_init,
        .ops.multi_instance.mod_start = (mod_instance_start)ttys_start,
        .cfg_obj = &ttys_cfg_6,
    },
#endif

#if CONFIG_FAULT_PRESENT
    {
        .name = "fault",
        .instance = MOD_NO_INSTANCE,
        .ops.singleton.mod_init = (mod_init)fault_init,
        .ops.singleton.mod_start = (mod_start)fault_start,
    },
#endif

#if CONFIG_FLASH_PRESENT
    {
        .name = "flash",
        .instance = MOD_NO_INSTANCE,
        .ops.singleton.mod_start = (mod_start)flash_start,
    },
#endif

#if CONFIG_LWL_PRESENT
    {
        .name = "lwl",
        .instance = MOD_NO_INSTANCE,
        .ops.singleton.mod_start = (mod_start)lwl_start,
    },
#endif

#if CONFIG_WDG_PRESENT
    {
        .name = "wdg",
        .instance = MOD_NO_INSTANCE,
        .ops.singleton.mod_init = (mod_init)wdg_init,
        .ops.singleton.mod_start = (mod_start)wdg_start,
    },
#endif

    {
        .name = "cmd",
        .instance = MOD_NO_INSTANCE,
        .ops.singleton.mod_init = (mod_init)cmd_init,
    },
    {
        .name = "console",
        .instance = MOD_NO_INSTANCE,
        .ops.singleton.mod_get_def_cfg = (mod_get_def_cfg)console_get_def_cfg,
        .ops.singleton.mod_init = (mod_init)console_init,
        .ops.singleton.mod_run = (mod_run)console_run,
        .cfg_obj = &console_cfg,
    },
    {
        .name = "tmr",
        .instance = MOD_NO_INSTANCE,
        .ops.singleton.mod_init = (mod_init)tmr_init,
        .ops.singleton.mod_start = (mod_start)tmr_start,
        .ops.singleton.mod_run = (mod_run)tmr_run,
    },
    {
        .name = "blinky",
        .instance = MOD_NO_INSTANCE,
        .ops.singleton.mod_init = (mod_init)blinky_init,
        .ops.singleton.mod_start = (mod_start)blinky_start,
        .cfg_obj = &blinky_cfg,
    },
    {
        .name = "dio",
        .instance = MOD_NO_INSTANCE,
        .ops.singleton.mod_init = (mod_init)dio_init,
        .ops.singleton.mod_start = (mod_start)dio_start,
        .cfg_obj = &dio_cfg,
    },

#if CONFIG_GPS_PRESENT
    {
        .name = "gps",
        .instance = MOD_NO_INSTANCE,
        .ops.singleton.mod_get_def_cfg = (mod_get_def_cfg)gps_get_def_cfg,
        .ops.singleton.mod_init = (mod_init)gps_init,
        .ops.singleton.mod_start = (mod_start)gps_start,
        .ops.singleton.mod_run = (mod_run)gps_run,
        .cfg_obj = &gps_cfg,
    },
#endif

#if CONFIG_I2C_TYPE == 1 && CONFIG_I2C_3_PRESENT
    {
        .name = "i2c",
        .instance = I2C_INSTANCE_3,
        .ops.singleton.mod_get_def_cfg = (mod_get_def_cfg)i2c_get_def_cfg,
        .ops.singleton.mod_init = (mod_init)i2c_init,
        .ops.singleton.mod_start = (mod_start)i2c_start,
        .cfg_obj = &i2c_cfg,
    },
#endif

#if CONFIG_TMPHM_1_PRESENT
    {
        .name = "tmphm",
        .instance = TMPHM_INSTANCE_1,
        .ops.singleton.mod_get_def_cfg = (mod_get_def_cfg)tmphm_get_def_cfg,
        .ops.singleton.mod_init = (mod_init)tmphm_init,
        .ops.singleton.mod_start = (mod_start)tmphm_start,
        .ops.singleton.mod_run = (mod_run)tmphm_run,
        .cfg_obj = &tmphm_cfg,
    },
#endif

#if CONFIG_STEP_1_PRESENT
    {
        .name = "step",
        .instance = STEP_INSTANCE_1,
        .ops.multi_instance.mod_get_def_cfg =
            (mod_instance_get_def_cfg)step_get_def_cfg,
        .ops.multi_instance.mod_init = (mod_instance_init)step_init,
        .ops.multi_instance.mod_start = (mod_instance_start)step_start,
        .cfg_obj = &step_cfg_1,
    },
#endif


#if CONFIG_STEP_2_PRESENT
    {
        .name = "step",
        .instance = STEP_INSTANCE_2,
        .ops.multi_instance.mod_get_def_cfg =
            (mod_instance_get_def_cfg)step_get_def_cfg,
        .ops.multi_instance.mod_init = (mod_instance_init)step_init,
        .ops.multi_instance.mod_start = (mod_instance_start)step_start,
        .cfg_obj = &step_cfg_2,
    },
#endif

#if CONFIG_DRAW_PRESENT
    {
        .name = "draw",
        .instance = MOD_NO_INSTANCE,
        .ops.singleton.mod_get_def_cfg = (mod_get_def_cfg)draw_get_def_cfg,
        .ops.singleton.mod_init = (mod_init)draw_init,
        .ops.singleton.mod_start = (mod_start)draw_start,
        .ops.singleton.mod_run = (mod_run)draw_run,
        .cfg_obj = &draw_cfg,
    },
#endif

#if CONFIG_FLOAT_PRESENT
    {
        .name = "float",
        .instance = MOD_NO_INSTANCE,
        .ops.singleton.mod_start = (mod_start)float_start,
    },
#endif

    {
        .name = "mem",
        .instance = MOD_NO_INSTANCE,
        .ops.singleton.mod_start = (mod_start)mem_start,
        .ops.singleton.mod_run = (mod_run)mem_run,
    },

#if CONFIG_OS_PRESENT
    {
        .name = "os",
        .instance = MOD_NO_INSTANCE,
        .ops.singleton.mod_init = (mod_init)os_init,
        .ops.singleton.mod_start = (mod_start)os_start,
    },
#endif

};

////////////////////////////////////////////////////////////////////////////////
// Public (global) variables and externs
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Public (global) functions
////////////////////////////////////////////////////////////////////////////////

void app_main(void)
{
    // int32_t result;;
    int32_t rc;
    int32_t idx;
    struct mod_info* mod;

#if defined STM32L452xx
    // Fix for bug in IDE generated code.
    LL_RCC_HSI_SetCalibTrimming(64);
#endif

#if CONFIG_FAULT_PRESENT
    wdg_start_init_hdw_wdg();
#endif

#if CONFIG_LWL_PRESENT
    lwl_enable(true);
#endif

    //
    // Invoke the init API on modules the use it.
    //

    setvbuf(stdout, NULL, _IONBF, 0);
    printc("\nInit: Init modules\n");

    for (idx = 0, mod = mods;
         idx < ARRAY_SIZE(mods);
         idx++, mod++) {
        if (mod->ops.singleton.mod_get_def_cfg != NULL &&
            mod->cfg_obj != NULL) {
            if (mod->instance == MOD_NO_INSTANCE) {
                rc = mod->ops.singleton.mod_get_def_cfg(mod->cfg_obj);
            } else {
                rc = mod->ops.multi_instance.mod_get_def_cfg(mod->instance,
                                                             mod->cfg_obj);
            }
            if (rc < 0) {
                log_error("Default cfg error for %s[%d]: %d\n", mod->name,
                          mod->instance, rc);
                INC_SAT_U16(cnts_u16[CNT_INIT_ERR]);
            }
        }
    }

#if CONFIG_I2C_TYPE == 1 && CONFIG_I2C_3_PRESENT
    tmphm_cfg.i2c_instance_id = I2C_INSTANCE_3;
#endif

    for (idx = 0, mod = mods;
         idx < ARRAY_SIZE(mods);
         idx++, mod++) {
        if (mod->ops.singleton.mod_init != NULL) {
            if (mod->instance == MOD_NO_INSTANCE) {
                rc = mod->ops.singleton.mod_init(mod->cfg_obj);
            } else {
                rc = mod->ops.multi_instance.mod_init(mod->instance,
                                                      mod->cfg_obj);
            }
            if (rc < 0) {
                log_error("Init error for %s[%d]: %d\n", mod->name,
                          mod->instance, rc);
                INC_SAT_U16(cnts_u16[CNT_INIT_ERR]);
            }
        }
    }

    //
    // Invoke the start API on modules the use it.
    //

    printc("Init: Start modules\n");

    for (idx = 0, mod = mods;
         idx < ARRAY_SIZE(mods);
         idx++, mod++) {
        if (mod->ops.singleton.mod_start != NULL) {
            if (mod->instance == MOD_NO_INSTANCE) {
                rc = mod->ops.singleton.mod_start();
            } else {
                rc = mod->ops.multi_instance.mod_start(mod->instance);
            }
            if (rc < 0) {
                log_error("Start error for %s: %d\n", mods->name, rc);
                INC_SAT_U16(cnts_u16[CNT_START_ERR]);
            }
        }
                
    }

    rc = cmd_register(&cmd_info);
    if (rc < 0) {
        log_error("main: cmd_register error %d\n", rc);
        INC_SAT_U16(cnts_u16[CNT_START_ERR]);
    }

    stat_dur_init(&stat_loop_dur);

    //
    // In the super loop invoke the run API on modules the use it.
    //

#if CONFIG_FAULT_PRESENT
    wdg_init_successful();
    rc = wdg_start_hdw_wdg(CONFIG_WDG_HARD_TIMEOUT_MS);
    if (rc < 0) {
        log_error("main: wdg_start_hdw_wdg error %d\n", rc);
        INC_SAT_U16(cnts_u16[CNT_START_ERR]);
    }
#endif

    printc("Init: Enter super loop\n");
    while (1)
    {
        stat_dur_restart(&stat_loop_dur);

        for (idx = 0, mod = mods;
             idx < ARRAY_SIZE(mods);
             idx++, mod++) {
            if (mod->ops.singleton.mod_run != NULL) {
                if (mod->instance == MOD_NO_INSTANCE) {
                    rc = mod->ops.singleton.mod_run();
                } else {
                    rc = mod->ops.multi_instance.mod_run(mod->instance);
                }
                if (rc < 0) {
                    log_error("Run error for %s: %d\n", mods->name, rc);
                    INC_SAT_U16(cnts_u16[CNT_RUN_ERR]);
                }
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// Private (static) functions
////////////////////////////////////////////////////////////////////////////////

/*
 * @brief Console command function for "main status".
 *
 * @param[in] argc Number of arguments, including "main"
 * @param[in] argv Argument values, including "main"
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * Command usage: main status [clear]
 */
static int32_t cmd_main_status(int32_t argc, const char** argv)
{
    bool clear = false;
    bool bad_arg = false;

    if (argc == 3) {
        if (strcasecmp(argv[2], "clear") == 0)
            clear = true;
        else
            bad_arg = true;
    } else if (argc > 3) {
        bad_arg = true;
    }

    if (bad_arg) {
        printc("Invalid arguments\n");
        return MOD_ERR_ARG;
    }

    printc("Super loop samples=%lu min=%lu ms, max=%lu ms, avg=%lu us\n",
           stat_loop_dur.samples, stat_loop_dur.min, stat_loop_dur.max,
           stat_dur_avg_us(&stat_loop_dur));

    if (clear) {
        printc("Clearing loop stat\n");
        stat_dur_init(&stat_loop_dur);
    }
    return 0;
}
