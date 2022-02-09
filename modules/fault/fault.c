/*
 * @brief Implementation of fault module.
 *
 * This module is the main one for detecting and handling serious faults in the
 * system. More specifically:
 * - It handles any unexpected exceptions (e.g. hard faults).
 * - It sets up a guard region to prevent the stack from overflowing into other
 *   areas of RAM.
 * - It handles watchdog module timeout notifications.
 * - On a fault (panic), it writes diagnositc data to the console and to flash
 *   (depending on configuration). This includes the light weight log (lwl)
 *   buffer.
 *
 * The following console commands are provided:
 * > fault data [erase]
 * > fault status
 * > fault test
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
#include "flash.h"
#include "log.h"
#include "lwl.h"
#include "module.h"
#include "tmr.h"
#include "wdg.h"

#include "fault.h"

////////////////////////////////////////////////////////////////////////////////
// Common macros
////////////////////////////////////////////////////////////////////////////////

#define STACK_INIT_PATTERN 0xcafebadd
#define STACK_GUARD_BLOCK_SIZE 32

// Address of last page of flash.
#if CONFIG_FLASH_TYPE == 1

#define FLASH_PANIC_DATA_ADDR ((uint8_t*)(CONFIG_FLASH_BASE_ADDR +  \
                                          CONFIG_FLASH_SIZE -       \
                                          CONFIG_FLASH_PAGE_SIZE))

#elif CONFIG_FLASH_TYPE == 2

#define FLASH_PANIC_DATA_ADDR ((uint8_t*)CONFIG_FAULT_FLASH_PANIC_ADDR)

#elif CONFIG_FLASH_TYPE == 3

#define FLASH_PANIC_DATA_ADDR ((uint8_t*)(CONFIG_FLASH_BASE_ADDR +  \
                                          CONFIG_FLASH_PAGE_SIZE))

#elif CONFIG_FLASH_TYPE == 4

#define FLASH_PANIC_DATA_ADDR ((uint8_t*)(CONFIG_FLASH_BASE_ADDR +  \
                                          CONFIG_FLASH_SIZE -       \
                                          CONFIG_FLASH_PAGE_SIZE))

#endif

#if CONFIG_MPU_TYPE == -1
    #define _s_stack_guard _sstack
    #define _e_stack_guard _sstack
#endif

////////////////////////////////////////////////////////////////////////////////
// Type definitions
////////////////////////////////////////////////////////////////////////////////

// Data collected after a fault. For writing to flash, this structure needs
// to be a multiple of the flash write size (e.g. 8 or 16 bytes).

struct fault_data
{
    uint32_t magic;                  //@fault_data,magic,4
    uint32_t num_section_bytes;      //@fault_data,num_section_bytes,4

    uint32_t fault_type;             //@fault_data,fault_type,4
    uint32_t fault_param;            //@fault_data,fault_param,4

    // The following fields must follow the exception stack format described
    // in the ARM v7-M Architecture Reference Manual.

    uint32_t excpt_stk_r0;           //@fault_data,excpt_stk_r0,4
    uint32_t excpt_stk_r1;           //@fault_data,excpt_stk_r1,4
    uint32_t excpt_stk_r2;           //@fault_data,excpt_stk_r2,4
    uint32_t excpt_stk_r3;           //@fault_data,excpt_stk_r3,4
    uint32_t excpt_stk_r12;          //@fault_data,excpt_stk_r12,4
    uint32_t excpt_stk_lr;           //@fault_data,excpt_stk_lr,4
    uint32_t excpt_stk_rtn_addr;     //@fault_data,excpt_stk_rtn_addr,4
    uint32_t excpt_stk_xpsr;         //@fault_data,excpt_stk_xpsr,4

    uint32_t sp;                     //@fault_data,sp,4
    uint32_t lr;                     //@fault_data,lr,4

    uint32_t ipsr;                   //@fault_data,ipsr,4
    uint32_t icsr;                   //@fault_data,icsr,4

    uint32_t shcsr;                  //@fault_data,shcsr,4
    uint32_t cfsr;                   //@fault_data,cfsr,4

    uint32_t hfsr;                   //@fault_data,hfsr,4
    uint32_t mmfar;                  //@fault_data,mmfar,4

    uint32_t bfar;                   //@fault_data,bfar,4
    uint32_t tick_ms;                //@fault_data,tick_ms,4

    #if CONFIG_FLASH_WRITE_BYTES == 16
    uint32_t pad1[2];                //@fault_data,pad1,8
    #endif
};

#define EXCPT_STK_BYTES (8*4)

_Static_assert((sizeof(struct fault_data) % CONFIG_FLASH_WRITE_BYTES) == 0,
               "Invalid struct fault_data");

struct end_marker {
    uint32_t magic;
    uint32_t num_section_bytes;

    #if CONFIG_FLASH_WRITE_BYTES == 16
        uint32_t pad1[2];
    #endif

};

_Static_assert((sizeof(struct end_marker) % CONFIG_FLASH_WRITE_BYTES) == 0,
               "Invalid struct end_marker");

////////////////////////////////////////////////////////////////////////////////
// Private (static) function declarations
////////////////////////////////////////////////////////////////////////////////

static void fault_common_handler(void);
static void record_fault_data(uint32_t data_offset, uint8_t* addr,
                              uint32_t num_bytes);
static void wdg_triggered_handler(uint32_t wdg_client_id);
static int32_t cmd_fault_data(int32_t argc, const char** argv);
static int32_t cmd_fault_status(int32_t argc, const char** argv);
static int32_t cmd_fault_test(int32_t argc, const char** argv);
static void test_overflow_stack(void);

////////////////////////////////////////////////////////////////////////////////
// Private (static) variables
////////////////////////////////////////////////////////////////////////////////

static struct fault_data fault_data_buf;

static int32_t log_level = LOG_DEFAULT;  

// Data structure with console command info.
static struct cmd_cmd_info cmds[] = {
    {
        .name = "data",
        .func = cmd_fault_data,
        .help = "Print/erase fault data, usage: fault data [erase]",
    },
    {
        .name = "status",
        .func = cmd_fault_status,
        .help = "Get module status, usage: fault status",
    },
    {
        .name = "test",
        .func = cmd_fault_test,
        .help = "Run test, usage: fault test [<op> [<arg>]] (enter no op for help)",
    }
};

// Data structure passed to cmd module for console interaction.
static struct cmd_client_info cmd_info = {
    .name = "fault",
    .num_cmds = ARRAY_SIZE(cmds),
    .cmds = cmds,
    .log_level_ptr = &log_level,
};

static uint32_t rcc_csr;
static bool got_rcc_csr = false;

////////////////////////////////////////////////////////////////////////////////
// Public (global) variables and externs
////////////////////////////////////////////////////////////////////////////////

// These symbols are defined in the linker script.
extern uint32_t _sdata;
extern uint32_t _estack;
extern uint32_t _s_stack_guard;
extern uint32_t _e_stack_guard;

////////////////////////////////////////////////////////////////////////////////
// Public (global) functions
////////////////////////////////////////////////////////////////////////////////

/*
 * @brief Initialize fault module instance.
 *
 * @param[in] cfg The fault configuration. (FUTURE)
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 */
int32_t fault_init(struct fault_cfg* cfg)
{
    fault_get_rcc_csr();

    // A future feature would be to detect a reset due to the hardware watchdog
    // and if true, save the LWL buffer at this time. This would require the LWL
    // buffer to be preserved during early initialization, which might be best
    // done by putting it in its own section (vs being in .data/.bss).

    return 0;
}

/*
 * @brief Start fault module instance.
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * This function starts the fault singleton module, to enter normal operation.
 */
int32_t fault_start(void)
{
    int32_t rc;
    uint32_t* sp;

    rc = cmd_register(&cmd_info);
    if (rc < 0) {
        log_error("fault_start: cmd_register error %d\n", rc);
        return rc;
    }

    rc = wdg_register_triggered_cb(wdg_triggered_handler);
    if (rc != 0) {
        log_error("fault_start: wdg_register_triggered_cb returns %ld\n", rc);
        return rc;
    }

    // Fill stack with a pattern so we can detect high water mark. We also fill
    // the stack guard region (which hasn't been set up yet) just for
    // completeness even though it isn't necessary.
    //
    // The stack pointer points to the last word written, so we first decrement
    // it to get to the next word to write.

    __ASM volatile("MOV  %0, sp" : "=r" (sp) : : "memory");
    sp--;
    while (sp >= &_s_stack_guard)
        *sp-- = STACK_INIT_PATTERN;

#if CONFIG_MPU_TYPE == 1

    // Set up stack guard region.
    //
    // - Region = 0, the region number we are using.
    // - SubRegionDisable = 0, meaning all subregions are enabled (really
    //   doesn't matter).
    // - Address = _s_stack_guard, the base address of the guard region.
    // - Attributes:
    //   + LL_MPU_REGION_SIZE_32B, meaning 32 byte region.
    //   + LL_MPU_REGION_PRIV_RO_URO, meaning read-only access (priv/unpriv).
    //   + LL_MPU_TEX_LEVEL0, meaning strongly ordered (really doesn't matter).
    //   + LL_MPU_INSTRUCTION_ACCESS_DISABLE, meaning an instruction fetch 
    //     never allowed.
    //   + LL_MPU_ACCESS_SHAREABLE, meaning sharable (but "don't care" since
    //     TEX is set to 000).
    //   + LL_MPU_ACCESS_NOT_CACHEABLE, meaning not cachable (really doesn't
    //     matter).
    //   + LL_MPU_ACCESS_NOT_BUFFERABLE, meaning not bufferable (really doesn't
    //     matter).

    LL_MPU_ConfigRegion(0, 0, (uint32_t)(&_s_stack_guard),
                        LL_MPU_REGION_SIZE_32B |
                        LL_MPU_REGION_PRIV_RO_URO |
                        LL_MPU_TEX_LEVEL0 |
                        LL_MPU_INSTRUCTION_ACCESS_DISABLE |
                        LL_MPU_ACCESS_SHAREABLE |
                        LL_MPU_ACCESS_NOT_CACHEABLE |
                        LL_MPU_ACCESS_NOT_BUFFERABLE);

    // Now enable the MPU.
    // - PRIVDEFENA = 1, meaning the default memory map is used if there is no
    //   MPU region.
    // - HFNMIENA = 1, meaning MPU is used for even high priority exception
    //   handlers.

    ARM_MPU_Enable(MPU_CTRL_PRIVDEFENA_Msk|MPU_CTRL_HFNMIENA_Msk);

#elif CONFIG_MPU_TYPE == 2

    // Set up stack guard region.
    //
    // ARM_MPU_SetMemAttr(uint8_t idx, uint8_t attr)
    // - param idx The attribute index to be set [0-7]
    // - param attr The attribute value to be set (MAIR):
    //   + Upper nibble = 0x4, meaning Normal memory, Outer non-cacheable.
    //   + Lower nibble = 0x4, meaning Normal memory, Inner non-cacheable.
    //
    // ARM_MPU_SetRegion(uint32_t rnr, uint32_t rbar, uint32_t rlar)
    // - param rnr Region number to be configured, 0.
    // - param rbar Value for RBAR register.
    // - param rlar Value for RLAR register.
    //
    // ARM_MPU_RBAR((uint32_t)(&_s_stack_guard)
    // - param BASE The base address bits [31:5] of a memory region. The value
    //              is zero extended. Effective address gets 32 byte aligned.
    // - param SH Defines the Shareability domain for this memory region.
    // - param RO Read-Only: Set to 1 for a read-only memory region.
    // - param NP Non-Privileged: Set to 1 for a non-privileged memory region.
    // - param XN eXecute Never: Set to 1 for a non-executable memory region.
    //
    // ARM_MPU_RLAR(LIMIT, IDX)
    // - param LIMIT The limit address bits [31:5] for this memory region. The
    //               value is one extended.
    // - param IDX The attribute index to be associated with this memory region.
    //             region.

    ARM_MPU_SetMemAttr(0, 0x44);
    ARM_MPU_SetRegion(0,
                      ARM_MPU_RBAR((uint32_t)(&_s_stack_guard),
                                   ARM_MPU_SH_OUTER,
                                   1,
                                   1,
                                   1),
                      ARM_MPU_RLAR(((uint32_t)(&_s_stack_guard) + 
                                    STACK_GUARD_BLOCK_SIZE - 1),
                                   0));

    // Now enable the MPU.
    // - PRIVDEFENA = 1, meaning the default memory map is used if there is no
    //   MPU region.
    // - HFNMIENA = 1, meaning MPU is used for even high priority exception
    //   handlers.

    ARM_MPU_Enable(MPU_CTRL_PRIVDEFENA_Msk|MPU_CTRL_HFNMIENA_Msk);

#endif

    return 0;
}

/*
 * @brief Fault detected by software.
 *
 * @note This function will not return.
 *
 * This function is called by software that detects a fault, so if we get here,
 * the stack must be working to some extent. However, for safety we reset the
 * stack pointer (after saving the original value) before calling another
 * function.  We also save the current value of lr, which is often just after
 * the call to this function.
 */
void fault_detected(enum fault_type fault_type, uint32_t fault_param)
{
    // Panic mode.
    CRIT_START();
    wdg_feed_hdw();

    // Disable MPU to avoid another fault (shouldn't be necessary)
    ARM_MPU_Disable();

    // Start to collect fault data.
    fault_data_buf.fault_type = fault_type;
    fault_data_buf.fault_param = fault_param;
    memset(&fault_data_buf.excpt_stk_r0, 0, EXCPT_STK_BYTES);
    __ASM volatile("MOV  %0, lr" : "=r" (fault_data_buf.lr) : : "memory");
    __ASM volatile("MOV  %0, sp" : "=r" (fault_data_buf.sp) : : "memory");
    __ASM volatile("MOV  sp, %0" : : "r" (&_estack) : "memory");
    fault_common_handler();
}

/*
 * @brief Unexpected exception treated as fault.
 *
 * @param[in] orig_sp The original sp register before it was reset.
 *
 * @note This function will not return.
 *
 * This function is jumped to (not called) by the initial exception handler. The
 * initial handler will have reset the stack pointer to ensure it is usable, and
 * passes the original stack pointer value to this handler.
 *
 * The exception stack is described in the ARM v7-M Architecture Reference
 * Manual. The relavent part is shown below:
 *
 *               +------------------------+
 *      SP+28 -> |         xPSR           |
 *               +------------------------+
 *      SP+24 -> |    Return Address      |
 *               +------------------------+
 *      SP+20 -> |         LR (R14)       |
 *               +------------------------+
 *      SP+16 -> |           R12          |
 *               +------------------------+
 *      SP+12 -> |           R3           |
 *               +------------------------+
 *       SP+8 -> |           R2           |
 *               +------------------------+
 *       SP+4 -> |           R1           |
 *               +------------------------+
 * Current SP -> |           R0           |
 *               +------------------------+
 *
 * This is useful information to collect, but we must ensure that that SP value
 * is valid, and that the memory is points to is valid RAM.
 *
 * Code in default handler modified to this:
 *
 *     Default_Handler:
 *         mov  r0, sp
 *	       b    fault_exception_handler
 *
 * These lines can be commented out.
 *
 *     Infinite_Loop:
 *         b    Infinite_Loop
 */
void fault_exception_handler(uint32_t sp)
{
    // Panic mode.
    CRIT_START();
    wdg_feed_hdw();

    // Disable MPU to avoid another fault (shouldn't be necessary)
    ARM_MPU_Disable();

    // Start to collect fault data.
    fault_data_buf.fault_type = FAULT_TYPE_EXCEPTION;
    fault_data_buf.fault_param = __get_IPSR();
    __ASM volatile("MOV  %0, lr" : "=r" (fault_data_buf.lr) : : "memory");
    fault_data_buf.sp = sp;

    if (((sp & 0x7) == 0) &&
        (sp >= (uint32_t)&_sdata) &&
        ((sp + EXCPT_STK_BYTES + 4) <= (uint32_t)&_estack)) {
        memcpy(&fault_data_buf.excpt_stk_r0, (uint8_t*)sp, EXCPT_STK_BYTES);
    } else {
        memset(&fault_data_buf.excpt_stk_r0, 0, EXCPT_STK_BYTES);
    }
    fault_common_handler();
}

uint32_t fault_get_rcc_csr(void)
{
    if (!got_rcc_csr) {
        got_rcc_csr = true;
        rcc_csr = RCC->CSR;
        RCC->CSR |= RCC_CSR_RMVF_Msk;
    }
    return rcc_csr;
}

////////////////////////////////////////////////////////////////////////////////
// Private (static) functions
////////////////////////////////////////////////////////////////////////////////

/*
 * @brief Common fault handling.
 *
 * @note This function will not return.
 */
static void fault_common_handler()
{
    uint8_t* lwl_data;
    uint32_t lwl_num_bytes;
    struct end_marker end;

    lwl_enable(false);
    printc_panic("\nFault type=%lu param=%lu\n", fault_data_buf.fault_type,
                 fault_data_buf.fault_param);

    // Populate data buffer and then record it.
    fault_data_buf.magic = MOD_MAGIC_FAULT;
    fault_data_buf.num_section_bytes = sizeof(fault_data_buf);
    fault_data_buf.ipsr = __get_IPSR();
    fault_data_buf.icsr = SCB->ICSR;
    fault_data_buf.shcsr =  SCB->SHCSR;
    fault_data_buf.cfsr =  SCB->CFSR;
    fault_data_buf.hfsr =  SCB->HFSR;
    fault_data_buf.mmfar =  SCB->MMFAR;
    fault_data_buf.bfar =  SCB->BFAR;
    fault_data_buf.tick_ms = tmr_get_ms();

    // Record the MCU data.
    record_fault_data(0, (uint8_t*)&fault_data_buf, sizeof(fault_data_buf));

    // Record the LWL buffer.
    lwl_data = lwl_get_buffer(&lwl_num_bytes);
    record_fault_data(sizeof(fault_data_buf), lwl_data, lwl_num_bytes);

    // Record end marker.
    memset(&end, 0, sizeof(end));
    end.magic = MOD_MAGIC_END;
    end.num_section_bytes = sizeof(end);

    record_fault_data(sizeof(fault_data_buf) + lwl_num_bytes, (uint8_t*)&end,
                      sizeof(end));

    // Reset system - this function will not return.
    NVIC_SystemReset();
}

/*
 * @brief Record fault data.
 *
 * @param[in] data_offset The logical offset of this chunk of data.
 * @param[in] data_addr The location of the data. Must be on 8 byte boundary
 *                      if writing to flash.
 * @param[in] num_bytes The number of bytes of data. Must ba multiple of 8
 *                      if writing to flash.
 *
 * @note As we are in a panic, we tend to just ignore return codes and keep
 *       going.
 */
static void record_fault_data(uint32_t data_offset, uint8_t* data_addr,
                              uint32_t num_bytes)
{
#if CONFIG_FAULT_PANIC_TO_FLASH
    {
        static bool do_flash;
        int32_t rc;
        
        if (data_offset == 0) {
            do_flash = ((struct fault_data*)FLASH_PANIC_DATA_ADDR)->magic !=
                MOD_MAGIC_FAULT;
        }
        if (do_flash) {
            if (data_offset == 0) {
                rc = flash_panic_erase_page((uint32_t*)FLASH_PANIC_DATA_ADDR);
                if (rc != 0)
                    printc_panic("flash_panic_erase_page returns %ld\n", rc);
            }
            rc = flash_panic_write((uint32_t*)(FLASH_PANIC_DATA_ADDR +
                                               data_offset),
                                   (uint32_t*)data_addr, num_bytes);
            if (rc != 0)
                printc_panic("flash_panic_write returns %ld\n", rc);
        }
    }
#endif

#if CONFIG_FAULT_PANIC_TO_CONSOLE
    {
        const int bytes_per_line = 32;
        uint32_t line_byte_ctr = 0;
        uint32_t idx;

        for (idx = 0; idx < num_bytes; idx++) {
            if (line_byte_ctr == 0)
                printc_panic("%08x: ", (unsigned int)data_offset);
            printc_panic("%02x", (unsigned)*data_addr++);
            data_offset++;
            if (++line_byte_ctr >= bytes_per_line) {
                printc_panic("\n");
                line_byte_ctr = 0;
            }
        }
        if (line_byte_ctr != 0)
            printc_panic("\n");
    }
#endif

}

/*
 * @brief Callback from watchdog module in case of a trigger.
 *
 * @param[in] wdg_client_id The watchdog client that triggered.
 *
 * @note This function will not return.
 */
static void wdg_triggered_handler(uint32_t wdg_client_id)
{
    fault_detected(FAULT_TYPE_WDG, wdg_client_id);
}

/*
 * @brief Console command function for "fault data".
 *
 * @param[in] argc Number of arguments, including "fault"
 * @param[in] argv Argument values, including "fault"
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * Command usage: fault data [erase]
 */
static int32_t cmd_fault_data(int32_t argc, const char** argv)
{
    int32_t rc = 0;

    if (argc > 3 || (argc == 3 && strcasecmp(argv[2], "erase") != 0)) {
        printc("Invalid command arguments\n");
        return MOD_ERR_BAD_CMD;
    }

    if (argc == 3) {
        rc = flash_panic_erase_page((uint32_t*)FLASH_PANIC_DATA_ADDR);
        if (rc != 0)
            printc("Flash erase fails\n");
    } else {
        uint32_t num_bytes;
        lwl_get_buffer(&num_bytes);
        num_bytes += sizeof(fault_data_buf);
        num_bytes += sizeof(struct end_marker);
        console_data_print((uint8_t*)FLASH_PANIC_DATA_ADDR, num_bytes);
    }
    return rc;
}

/*
 * @brief Console command function for "fault status".
 *
 * @param[in] argc Number of arguments, including "fault"
 * @param[in] argv Argument values, including "fault"
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * Command usage: fault status
 */
static int32_t cmd_fault_status(int32_t argc, const char** argv)
{
    const static struct
    {
        const char* bit_name;
        uint32_t csr_bit_mask;
    } reset_info[] = 
      {
          { "LPWR", RCC_CSR_LPWRRSTF_Msk},
          { "WWDG", RCC_CSR_WWDGRSTF_Msk},
          { "IWDG", RCC_CSR_IWDGRSTF_Msk},
          { "SFT", RCC_CSR_SFTRSTF_Msk},
          { "POR", RCC_CSR_PORRSTF_Msk},
          { "PIN", RCC_CSR_PINRSTF_Msk},
          { "BOR", RCC_CSR_BORRSTF_Msk},
      };
    uint32_t* sp;
    uint32_t idx;

    printc("Stack: 0x%08lx -> 0x%08lx (%lu bytes)\n",
           (uint32_t)&_estack,
           (uint32_t)&_e_stack_guard,
           (uint32_t)((&_estack - &_e_stack_guard) * sizeof(uint32_t)));
    sp = &_e_stack_guard;
    while (sp < &_estack && *sp == STACK_INIT_PATTERN)
        sp++;
    printc("Stack usage: 0x%08lx -> 0x%08lx (%lu bytes)\n",
           (uint32_t)&_estack, (uint32_t)sp,
           (uint32_t)((&_estack - sp) * sizeof(uint32_t)));
    printc("CSR: Poweron=0x%08lx Current=0x%08lx\n", rcc_csr,
           RCC->CSR);
    for (idx = 0; idx < ARRAY_SIZE(reset_info); idx++) {
        if (rcc_csr & reset_info[idx].csr_bit_mask) {
            printc("     %s reset bit set in CSR at power on.\n",
                   reset_info[idx].bit_name);
        }
    }

    return 0;
}

/*
 * @brief Console command function for "fault test".
 *
 * @param[in] argc Number of arguments, including "fault"
 * @param[in] argv Argument values, including "fault"
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * Command usage: fault test [<op> [<arg>]]
 */
static int32_t cmd_fault_test(int32_t argc, const char** argv)
{
    int32_t num_args;
    struct cmd_arg_val arg_vals[2];

    // Handle help case.
    if (argc == 2) {
        printc("Test operations and param(s) are as follows:\n"
               "  Report fault: usage: fault test report <type> <param>\n"
               "  Stack overflow: usage: fault test stack\n"
               "  Bad pointer: usage: fault test ptr\n"
            );
        return 0;
    }

    if (strcasecmp(argv[2], "report") == 0) {
        num_args = cmd_parse_args(argc-3, argv+3, "ui", arg_vals);
        if (num_args != 2) {
            return MOD_ERR_BAD_CMD;
        }
        fault_detected(arg_vals[0].val.u, arg_vals[1].val.i);
    } else if (strcasecmp(argv[2], "stack") == 0) {
        test_overflow_stack();
    } else if (strcasecmp(argv[2], "ptr") == 0) {
        *((uint32_t*)0xffffffff) = 0xbad;
    } else {
        printc("Invalid test '%s'\n", argv[2]);
        return MOD_ERR_BAD_CMD;
    }
    return 0;
}

static void test_overflow_stack(void)
{
    test_overflow_stack();
}
