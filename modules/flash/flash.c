/*
 * @brief Implementation of flash module.
 *
 * This module is for writing to flash memory. It only supports "panic" mode
 * operation, where interrupts are not used and blocking is allowed. In fact,
 * there are (unexpected) scenarios where a function could hang, and in this
 * case it is assumed a watchdog timer will force a reset.
 *
 * The following console commands are provided:
 * > flash e (to erase)
 * > flash w (to write)
 *
 * See code for details.
 *
 * Flash types examples:
 * 1: STM32L452xx 
 * 2: STM32F401xE
 * 3: STM32F103xB
 * 4: STM32U575xx
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

#include "config.h"
#include CONFIG_STM32_LL_CORTEX_HDR

#include "cmd.h"
#include "console.h"
#include "flash.h"
#include "log.h"
#include "module.h"

////////////////////////////////////////////////////////////////////////////////
// Common macros
////////////////////////////////////////////////////////////////////////////////

#if CONFIG_FLASH_TYPE == 1 // Example: STM32L452xx 

#define FLASH_ERR_MASK (FLASH_SR_OPTVERR_Msk |  \
                        FLASH_SR_RDERR_Msk |    \
                        FLASH_SR_FASTERR_Msk |  \
                        FLASH_SR_MISERR_Msk |   \
                        FLASH_SR_PGSERR_Msk |   \
                        FLASH_SR_SIZERR_Msk |  \
                        FLASH_SR_PGAERR_Msk |   \
                        FLASH_SR_WRPERR_Msk |   \
                        FLASH_SR_PROGERR_Msk |  \
                        FLASH_SR_OPERR_Msk)

#define FLASH_CR_CMD_MASK (FLASH_CR_RDERRIE_Msk |   \
                             FLASH_CR_ERRIE_Msk |   \
                             FLASH_CR_EOPIE_Msk |   \
                             FLASH_CR_FSTPG_Msk |     \
                             FLASH_CR_MER1_Msk |     \
                             FLASH_CR_SER_Msk |     \
                             FLASH_CR_PG_Msk)

#elif CONFIG_FLASH_TYPE == 2 // Example: STM32F401xE

#define FLASH_ERR_MASK (FLASH_SR_WRPERR_Msk |  \
                        FLASH_SR_PGAERR_Msk |    \
                        FLASH_SR_PGPERR_Msk |  \
                        FLASH_SR_PGSERR_Msk |   \
                        FLASH_SR_RDERR_Msk)

#define FLASH_CR_CMD_MASK (FLASH_CR_ERRIE_Msk |   \
                             FLASH_CR_EOPIE_Msk |   \
                             FLASH_CR_MER_Msk |     \
                             FLASH_CR_SER_Msk |     \
                             FLASH_CR_PG_Msk)

#elif CONFIG_FLASH_TYPE == 3 // Example: STM32F103xB

#define FLASH_ERR_MASK (FLASH_SR_PGERR_Msk |  \
                        FLASH_SR_WRPRTERR_Msk)

#define FLASH_CR_CMD_MASK (FLASH_CR_EOPIE_Msk |   \
                             FLASH_CR_ERRIE_Msk |   \
                             FLASH_CR_OPTER_Msk |   \
                             FLASH_CR_OPTPG_Msk |   \
                             FLASH_CR_MER_Msk |     \
                             FLASH_CR_PER_Msk |     \
                             FLASH_CR_PG_Msk)

#elif CONFIG_FLASH_TYPE == 4 // Example: STM32U575xx

#define FLASH_SR_BSY_Msk FLASH_NSSR_BSY_Msk

#define FLASH_ERR_MASK (FLASH_NSSR_OPERR_Msk |      \
                        FLASH_NSSR_PROGERR_Msk |    \
                        FLASH_NSSR_WRPERR_Msk |     \
                        FLASH_NSSR_PGAERR_Msk |     \
                        FLASH_NSSR_SIZERR_Msk |     \
                        FLASH_NSSR_PGSERR_Msk |     \
                        FLASH_NSSR_OPTWERR_Msk)

#define FLASH_CR_CMD_MASK (FLASH_NSCR_ERRIE_Msk | \
                             FLASH_NSCR_EOPIE_Msk | \
                             FLASH_NSCR_EOPIE_Msk | \
                             FLASH_NSCR_MER2_Msk |  \
                             FLASH_NSCR_BWR_Msk |   \
                             FLASH_NSCR_MER1_Msk |  \
                             FLASH_NSCR_PER_Msk |   \
                             FLASH_NSCR_PG_Msk)

#define FLASH_CR FLASH->NSCR
#define FLASH_CR_STRT_Msk FLASH_NSCR_STRT_Msk 
#define FLASH_CR_LOCK_Msk FLASH_NSCR_LOCK_Msk 
#define FLASH_CR_PG_Msk FLASH_NSCR_PG_Msk 
#define FLASH_CR_PNB_Msk FLASH_NSCR_PNB_Msk
#define FLASH_CR_PNB_Pos FLASH_NSCR_PNB_Pos
#define FLASH_CR_BKER_Msk FLASH_NSCR_BKER_Msk
#define FLASH_CR_BKER_Pos FLASH_NSCR_BKER_Pos
#define FLASH_CR_PER_Msk FLASH_NSCR_PER_Msk
#define FLASH_SR_WDW_Msk FLASH_NSSR_WDW_Msk

#define FLASH_SR FLASH->NSSR

#define KEYR NSKEYR

#endif

#define FLASH_WRITE_BYTES_MASK (CONFIG_FLASH_WRITE_BYTES - 1)

#ifndef FLASH_CR
    #define FLASH_CR FLASH->CR
#endif

#ifndef FLASH_SR
    #define FLASH_SR FLASH->SR
#endif

#ifndef FLASH_KEY1
    #define FLASH_KEY1 0x45670123U
#endif

#ifndef FLASH_KEY2
    #define FLASH_KEY2 0xCDEF89ABU                   
#endif

////////////////////////////////////////////////////////////////////////////////
// Type definitions
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Private (static) function declarations
////////////////////////////////////////////////////////////////////////////////

static void flash_unlock(void);
static void flash_panic_op_start(void);
static void flash_panic_op_complete(void);
static int32_t addr_to_page_num(uint32_t* addr);

#if CONFIG_FLASH_TYPE == 4
static int32_t addr_to_bank_num(uint32_t* addr);
#endif

static int32_t cmd_flash_erase(int32_t argc, const char** argv);
static int32_t cmd_flash_write(int32_t argc, const char** argv);

////////////////////////////////////////////////////////////////////////////////
// Private (static) variables
////////////////////////////////////////////////////////////////////////////////

#if CONFIG_FLASH_TYPE != 3
    static bool disabled_icache = false;
#endif

#if CONFIG_FLASH_TYPE != 3 && CONFIG_FLASH_TYPE != 4
    static bool disabled_dcache = false;
#endif

uint32_t last_op_error_mask = 0;

static int32_t log_level = LOG_DEFAULT;

static struct cmd_cmd_info cmds[] = {
    {
        .name = "e",
        .func = cmd_flash_erase,
        .help = "Erase flash: usage: flash e addr",
    },
    {
        .name = "w",
        .func = cmd_flash_write,
        .help = "Write flash: usage: flash w addr value(32) ...",
    },
};

static struct cmd_client_info cmd_info = {
    .name = "flash",
    .num_cmds = ARRAY_SIZE(cmds),
    .cmds = cmds,
    .log_level_ptr = &log_level,
};

////////////////////////////////////////////////////////////////////////////////
// Public (global) variables and externs
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Public (global) functions
////////////////////////////////////////////////////////////////////////////////

/*
 * @brief Start flash instance.
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * This function starts the flash singleton module, to enter normal operation.
 */
int32_t flash_start(void)
{
    int32_t rc;

    rc = cmd_register(&cmd_info);
    if (rc < 0) {
        log_error("flash_start: cmd error %d\n", rc);
        return rc;
    }

    return 0;
}

/*
 * @brief Panic erase of a single page of memory.
 *
 * @param[in] start_addr Starting address in flash (must be on page boundary).
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * @note This function is to be used in panic conditions - it will block until
 *       the operation is complete. The assumption is the hardware watchdog
 *       will trigger if this function gets stuck.
 */
int32_t flash_panic_erase_page(uint32_t* start_addr)
{
    int32_t page_num = addr_to_page_num(start_addr);
    if (page_num < 0)
        return page_num;

    log_debug("flash panic erase start_addr=0x%08x page_num=%ld\n",
              (unsigned)start_addr, page_num);

    // Check that no flash main memory operation is ongoing.
    if (FLASH_SR & FLASH_SR_BSY_Msk)
        return MOD_ERR_BUSY;

    flash_panic_op_start();

#if CONFIG_FLASH_TYPE == 1 // Example: STM32L452xx 

    // Select the page in FLASH->CR;
    FLASH_CR = (FLASH_CR & (~FLASH_CR_PNB_Msk)) |
        (page_num << FLASH_CR_PNB_Pos);

    // Set the PER bit in FLASH->CR.
    FLASH_CR |= FLASH_CR_PER_Msk;

#elif CONFIG_FLASH_TYPE == 2 // Example: STM32F401xE

    // Select the SER bit and sector in FLASH->CR;
    FLASH_CR = (FLASH_CR & (~FLASH_CR_SNB_Msk)) |
        ((page_num << FLASH_CR_SNB_Pos) | FLASH_CR_SER_Msk);

#elif CONFIG_FLASH_TYPE == 3 // Example: STM32F103xB

    #error TODO STM32F103xB

#elif CONFIG_FLASH_TYPE == 4 // Example: STM32U575xx

    {
        int32_t bank_num = addr_to_bank_num(start_addr);
        if (bank_num < 0)
            return bank_num;

        // Select the page and bank in FLASH->CR;
        FLASH_CR = (FLASH_CR & (~(FLASH_CR_PNB_Msk | FLASH_CR_BKER_Msk))) |
            (FLASH_CR_PER_Msk |
             (page_num << FLASH_CR_PNB_Pos) |
             (bank_num << FLASH_CR_BKER_Pos));
    }

#else
    #error Unknown procesor
#endif

    // Start the erase.
    FLASH_CR |= FLASH_CR_STRT_Msk;

    // Wait for BSY bit to be cleared in FLASH->SR.
    while (FLASH_SR & FLASH_SR_BSY_Msk) {}

    flash_panic_op_complete();

    if (last_op_error_mask != 0)
        return MOD_ERR_PERIPH;

    return 0;
}

/*
 * @brief Panic data write.
 *
 * @param[in] flash_addr Starting address in flash (must be on N-byte boundary).
 * @param[in] data Pointer to data to write (must be on 4-byte boundary).
 * @param[in] data_len Number of bytes of data (must be multiple of N).
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * @note This function is to be used in panic conditions - it will block until
 *       the operation is complete. The assumption is the hardware watchdog
 *       will trigger if this function gets stuck.
 */
int32_t flash_panic_write(uint32_t* flash_addr, uint32_t* data,
                          uint32_t data_len)
{
    if ((((uint32_t)flash_addr) & FLASH_WRITE_BYTES_MASK) ||
        (((uint32_t)data) & 0x3) ||
        (data_len & FLASH_WRITE_BYTES_MASK))
        return MOD_ERR_ARG;

    // Check that no flash main memory operation is ongoing.
    if (FLASH_SR & FLASH_SR_BSY_Msk)
        return MOD_ERR_BUSY;

    #if CONFIG_FLASH_TYPE == 4
        // A write is in progress - not expected.
        if (FLASH_SR & FLASH_SR_WDW_Msk)
            return MOD_ERR_PERIPH;
    #endif

    flash_panic_op_start();

    // Set the program bit.
    FLASH_CR |= FLASH_CR_PG_Msk;

    for (; data_len > 0; data_len -= CONFIG_FLASH_WRITE_BYTES) {
        // Write the data to flash.
        *flash_addr++ = *data++;
        *flash_addr++ = *data++;

        #if CONFIG_FLASH_WRITE_BYTES == 16
            *flash_addr++ = *data++;
            *flash_addr++ = *data++;
        #endif

        // Wait until busy is cleared.
        while (FLASH_SR & FLASH_SR_BSY_Msk) {}

        #if CONFIG_FLASH_TYPE == 4
            if (FLASH_SR & FLASH_SR_WDW_Msk)
                return MOD_ERR_PERIPH;
        #endif

        // Since EOP interrupts are not enabled, we don't check/clear it.
    }

    flash_panic_op_complete();

    if (last_op_error_mask != 0)
        return MOD_ERR_PERIPH;

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Private (static) functions
////////////////////////////////////////////////////////////////////////////////

/*
 * @brief Unlock flash so it can be erased or written.
 */
static void flash_unlock(void)
{
    if (FLASH_CR & FLASH_CR_LOCK_Msk) {
        FLASH->KEYR = FLASH_KEY1;
        FLASH->KEYR = FLASH_KEY2;
    }
}

/*
 * @brief Panic operations start.
 *
 * Prepares flash for erase/write operations.
 */
static void flash_panic_op_start(void)
{
    flash_unlock();

    // Clear all error flags from a previous operation.
    FLASH_SR |= FLASH_SR & FLASH_ERR_MASK;
    last_op_error_mask = 0;

    // Clear all commands bits from a previous operation.
    FLASH_CR &= ~FLASH_CR_CMD_MASK;

#if CONFIG_FLASH_TYPE == 4

    if ((ICACHE->CR & ICACHE_CR_EN_Msk) != 0) {
        ICACHE->CR &= ~ICACHE_CR_EN_Msk;
        disabled_icache = true;
    }

#elif CONFIG_FLASH_TYPE != 3

    // Disable instruction and/or data caching.
    if ((FLASH->ACR & FLASH_ACR_ICEN_Msk) != 0) {
        FLASH->ACR &= ~FLASH_ACR_ICEN_Msk;
        disabled_icache = true;
    }
    if ((FLASH->ACR & FLASH_ACR_DCEN_Msk) != 0) {
        FLASH->ACR &= ~FLASH_ACR_DCEN_Msk;
        disabled_dcache = true;
    }

#endif

#if CONFIG_FLASH_TYPE == 2

    // Set up flash for 32-bit programming.
    FLASH->CR = (FLASH->CR & ~FLASH_CR_PSIZE_Msk) |
        (2 << FLASH_CR_PSIZE_Pos);

#endif
}

/*
 * @brief Panic operations complete.
 *
 * Restores flash after erase/write operations (should only be called after
 * calling flash_panic_op_start()).
 */
static void flash_panic_op_complete(void)
{
    // Save the error flags, and then clear them.
    last_op_error_mask = FLASH_SR & FLASH_ERR_MASK;
    FLASH_SR |= last_op_error_mask;

    // Clear all commands bits from the operation.
    FLASH_CR &= ~FLASH_CR_CMD_MASK;

#if CONFIG_FLASH_TYPE == 4

    // Invalidate the cache and wait for it to finish.
    ICACHE->CR |= ICACHE_CR_CACHEINV;
    while ((ICACHE->SR & ICACHE_SR_BUSYF) != 0) {}
    
    if (disabled_icache)
        ICACHE->CR |= ICACHE_CR_EN_Msk;

#elif CONFIG_FLASH_TYPE != 3

    // Flush instruction cache and re-enable if needed.
    FLASH->ACR |= FLASH_ACR_ICRST_Msk;
    FLASH->ACR &= ~FLASH_ACR_ICRST_Msk;

    if (disabled_icache)
        FLASH->ACR |= FLASH_ACR_ICEN_Msk;

    // Flush data cache and re-enable if needed.
    FLASH->ACR |= FLASH_ACR_DCRST_Msk;
    FLASH->ACR &= ~FLASH_ACR_DCRST_Msk;

    if (disabled_dcache)
        FLASH->ACR |= FLASH_ACR_DCEN_Msk;

#endif
}

/*
 * @brief Convert flash addrss to page number. The term "sector" might be used
 *        instead of "page".
 *
 * @param[in] addr Flash address of page.
 *
 * @return Page number (>=0) if a valid address, else a "MOD_ERR" value (<0).
 */
static int32_t addr_to_page_num(uint32_t* addr)
{
#if CONFIG_FLASH_TYPE == 1 || CONFIG_FLASH_TYPE == 3 || CONFIG_FLASH_TYPE == 4

    int32_t page_num = ((uint32_t)addr - CONFIG_FLASH_BASE_ADDR) /
        CONFIG_FLASH_PAGE_SIZE;

    if (((uint32_t)addr) % CONFIG_FLASH_PAGE_SIZE != 0)
        return MOD_ERR_ARG;

    if (page_num >= CONFIG_FLASH_NUM_PAGE)
        return MOD_ERR_ARG;

#if CONFIG_FLASH_NUM_BANK > 1
    page_num = page_num % (CONFIG_FLASH_NUM_PAGE / CONFIG_FLASH_NUM_BANK);
#endif

    return page_num;

#elif CONFIG_FLASH_TYPE == 2

    int32_t page_num;
    const static uint32_t sector_addr[] = {
        0x08000000,
        0x08004000,
        0x08008000,
        0x0800C000,
        0x08010000,
        0x08020000,
        0x08040000,
        0x08060000,
    };
    for (page_num = 0; page_num <= ARRAY_SIZE(sector_addr); page_num++) {
        if ((uint32_t)addr == sector_addr[page_num])
            return page_num;
    }
    return MOD_ERR_ARG;

#endif
}

#if CONFIG_FLASH_TYPE == 4

/*
 * @brief Convert flash addrss to bank number (0-based).
 *
 * @param[in] addr Flash address inside bank.
 *
 * @return Bank number (>=0) if a valid address, else a "MOD_ERR" value (<0).
 */
static int32_t addr_to_bank_num(uint32_t* addr)
{
    int32_t bank_num = ((uint32_t)addr - CONFIG_FLASH_BASE_ADDR) /
        ((CONFIG_FLASH_NUM_PAGE / CONFIG_FLASH_NUM_BANK) *
         CONFIG_FLASH_PAGE_SIZE);

    if (bank_num >= CONFIG_FLASH_NUM_BANK)
        return MOD_ERR_ARG;

    return bank_num;
}

#endif


/*
 * @brief Console command function for "flash e".
 *
 * @param[in] argc Number of arguments, including "flash"
 * @param[in] argv Argument values, including "flash"
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * Command usage: flash e addr
 */
static int32_t cmd_flash_erase(int32_t argc, const char** argv)
{
    int32_t rc;
    struct cmd_arg_val arg_vals[1];

    rc = cmd_parse_args(argc-2, argv+2, "p", arg_vals);
    if (rc != 1)
        return rc;

    rc = flash_panic_erase_page(arg_vals[0].val.p);
    printc("rc=%ld\n", rc);
    return rc;
}

/*
 * @brief Console command function for "flash w".
 *
 * @param[in] argc Number of arguments, including "flash"
 * @param[in] argv Argument values, including "flash"
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * Command usage: flash w addr value(32) ...
 */
static int32_t cmd_flash_write(int32_t argc, const char** argv)
{

    const int NUM_WORDS = CONFIG_FLASH_WRITE_BYTES / 4;
    int32_t num_args;
    struct cmd_arg_val arg_vals[NUM_WORDS + 1];
    uint32_t data[NUM_WORDS];
    int idx;
    int32_t rc;

    num_args = cmd_parse_args(argc-2, argv+2, "puu[uu]", arg_vals);
    if (num_args != (NUM_WORDS + 1)) {
        printc("Must specify %d data words\n", NUM_WORDS);
        return num_args;
    }
    num_args--;
    for (idx = 0; idx < num_args; idx++)
        data[idx] = arg_vals[idx+1].val.u;
    rc = flash_panic_write(arg_vals[0].val.p, data,
                           num_args * sizeof(uint32_t));
    printc("rc=%ld\n", rc);
    return rc;
}
