#ifndef _CONFIG_H_
#define _CONFIG_H_

/* @brief Configuration.
 *
 * This file contains configuration #defines (sometimes referred to as
 * "settings") for the system and the various modules. It has several purposes:
 * - Allow the same software to be used on different MCUs/boards which have
 *   different resources (e.g. different number of I2C interfaces).
 * - Allow module instances to be included, or not included, based on which
 *   features are enabled.
 *
 * The #defines in this file are influenced by defines specified outside of
 * this file. This is normally defines specified in the makefiles (as arguments
 * for the -D compiler option). In the case of the STM32CubeIDE, the defines put
 * in the makefiles are based on project "properties" settings.
 *
 * The "input" defines in this file are:
 * - A #define for the CPU/MCU type, for example, STM32F103xB. In the case of
 *   STM32CubeIDE, such a define is automatically created and put into the
 *   makefiles.
 * - Defines for "features", such as CONFIG_FEAT_TMPHM for the temperature/
 *   humidity sensor. These are manually added to the project settings (and thus
 *   the makefiles).
 *
 * Naming convention:
 * - All names start with CONFIG_
 * - Names for modules start with CONFIG_<MOD-NAME>_, e.g. CONFIG_I2C_
 *
 * There are some #defines for modules and features that are independent of
 * the MPU/board and particular whether features are enabled. These are known
 * as "common" settings.
 *
 * Patterns of use:
 * - Often times, the default configuration parameters for modules are
 *   specified here, rather than hardcoded in the source code,
 * - Different MCUs might have different peripheral hardware implementations.
 *   For example, the peripherals might have different registers, different
 *   operations, etc.  These different implementations are termed "types" and
 *   are specified using a #define. For example, there is a CONFIG_UART_TYPE
 *   #define that can have values 1, 2, 3, etc. If an MPU does not have a
 *   particular peripheral, the #define for the type is set to -1.
 * - To indicate that a particular module instance is to be included in
 *   the build, a #define with a name of the format:
 *       CONFIG_<MODULE>_<INSTANCE_ID>_PRESENT
 *   is defined to 1. If the module is a singleton, the instance ID is
 *   not included. Examples:
 *       #define CONFIG_DRAW_PRESENT 1
 *       #define CONFIG_STEP_1_PRESENT 1
 *
 * Added notes:
 * - We always assume a console is present.
 * - The console is connected to UART 2 if possible.
 * - When using #defines in your code, keep in mind that an unknown symbol in a
 *   preprocessor statement is treated as zero, and is NOT an erorr. For example
 *   if there is no #define statement for CONFIG_X then the statement
 *       #if CONFIG_X == 0
 *   will be true.
 */

////////////////////////////////////////////////////////////////////////////////
// Odds and ends.
////////////////////////////////////////////////////////////////////////////////

// Used when a dummy value is needed.
#define CONFIG_DUMMY_0 0

////////////////////////////////////////////////////////////////////////////////
// Below are #defines based directly on "MCU type" #define from the IDE
// (makefile).
////////////////////////////////////////////////////////////////////////////////

#if defined STM32F103xB // STM32F103C8T6

    #define CONFIG_STM32_LL_BUS_HDR "stm32f1xx_ll_bus.h"
    #define CONFIG_STM32_LL_CORTEX_HDR "stm32f1xx_ll_cortex.h"
    #define CONFIG_STM32_LL_GPIO_HDR "stm32f1xx_ll_gpio.h"
    #define CONFIG_STM32_LL_I2C_HDR "stm32f1xx_ll_i2c.h"
    #define CONFIG_STM32_LL_RCC_HDR "stm32f1xx_ll_rcc.h"
    #define CONFIG_STM32_LL_USART_HDR "stm32f1xx_ll_usart.h"

    #define CONFIG_DIO_TYPE 3
    #define CONFIG_I2C_TYPE 1
    #define CONFIG_USART_TYPE 1
    #define CONFIG_MPU_TYPE -1

    #define CONFIG_OS_CFG_IRQN_TYPE_MIN MemoryManagement_IRQn 
    #define CONFIG_OS_CFG_IRQN_TYPE_MAX USBWakeUp_IRQn
    #define CONFIG_OS_IRQN_TYPE_EXC_NUM_OFFSET (4 - MemoryManagement_IRQn)

    #define CONFIG_FLASH_TYPE 3
    #define CONFIG_FLASH_BASE_ADDR 0x08000000
    #define CONFIG_FLASH_PAGE_SIZE 1024
    #define CONFIG_FLASH_NUM_PAGE 64
    #define CONFIG_FLASH_SIZE (64*1024)
    #define CONFIG_FLASH_WRITE_BYTES 8

#elif defined STM32F401xE

    #define CONFIG_STM32_LL_BUS_HDR "stm32f4xx_ll_bus.h"
    #define CONFIG_STM32_LL_CORTEX_HDR "stm32f4xx_ll_cortex.h"
    #define CONFIG_STM32_LL_GPIO_HDR "stm32f4xx_ll_gpio.h"
    #define CONFIG_STM32_LL_I2C_HDR "stm32f4xx_ll_i2c.h"
    #define CONFIG_STM32_LL_RCC_HDR "stm32f4xx_ll_rcc.h"
    #define CONFIG_STM32_LL_USART_HDR "stm32f4xx_ll_usart.h"
    #define CONFIG_STM32_LL_IWDG_HDR "stm32f4xx_ll_iwdg.h"

    #define CONFIG_DIO_TYPE 1
    #define CONFIG_I2C_TYPE 1
    #define CONFIG_USART_TYPE 1
    #define CONFIG_MPU_TYPE 1

    #define CONFIG_OS_CFG_IRQN_TYPE_MIN MemoryManagement_IRQn 
    #define CONFIG_OS_CFG_IRQN_TYPE_MAX SPI4_IRQn
    #define CONFIG_OS_IRQN_TYPE_EXC_NUM_OFFSET (4 - MemoryManagement_IRQn)

    #define CONFIG_FLASH_TYPE 2
    #define CONFIG_FLASH_BASE_ADDR 0x08000000
    #define CONFIG_FLASH_WRITE_BYTES 8

    #define CONFIG_FAULT_FLASH_PANIC_ADDR 0x08004000

#elif defined STM32L452xx

    #define CONFIG_STM32_LL_BUS_HDR "stm32l4xx_ll_bus.h"
    #define CONFIG_STM32_LL_CORTEX_HDR "stm32l4xx_ll_cortex.h"
    #define CONFIG_STM32_LL_GPIO_HDR "stm32l4xx_ll_gpio.h"
    #define CONFIG_STM32_LL_I2C_HDR "stm32l4xx_ll_i2c.h"
    #define CONFIG_STM32_LL_RCC_HDR "stm32l4xx_ll_rcc.h"
    #define CONFIG_STM32_LL_USART_HDR "stm32l4xx_ll_usart.h"
    #define CONFIG_STM32_LL_IWDG_HDR "stm32l4xx_ll_iwdg.h"

    #define CONFIG_DIO_TYPE 2
    #define CONFIG_I2C_TYPE 0
    #define CONFIG_USART_TYPE 2

    #define CONFIG_OS_CFG_IRQN_TYPE_MIN MemoryManagement_IRQn 
    #define CONFIG_OS_CFG_IRQN_TYPE_MAX I2C4_ER_IRQn
    #define CONFIG_OS_IRQN_TYPE_EXC_NUM_OFFSET (4 - MemoryManagement_IRQn)

    #define CONFIG_FLASH_TYPE 1
    #define CONFIG_FLASH_BASE_ADDR 0x08000000
    #define CONFIG_FLASH_SIZE (512*1024)
    #define CONFIG_FLASH_PAGE_SIZE 2048
    #define CONFIG_FLASH_NUM_PAGE 256
    #define CONFIG_FLASH_WRITE_BYTES 8

#elif defined STM32U575xx

    #define CONFIG_STM32_LL_BUS_HDR "stm32u5xx_ll_bus.h"
    #define CONFIG_STM32_LL_CORTEX_HDR "stm32u5xx_ll_cortex.h"
    #define CONFIG_STM32_LL_GPIO_HDR "stm32u5xx_ll_gpio.h"
    #define CONFIG_STM32_LL_I2C_HDR "stm32u5xx_ll_i2c.h"
    #define CONFIG_STM32_LL_RCC_HDR "stm32u5xx_ll_rcc.h"
    #define CONFIG_STM32_LL_USART_HDR "stm32u5xx_ll_usart.h"
    #define CONFIG_STM32_LL_IWDG_HDR "stm32u5xx_ll_iwdg.h"

    #define CONFIG_DIO_TYPE 4
    #define CONFIG_I2C_TYPE 0
    #define CONFIG_USART_TYPE 3
    #define CONFIG_MPU_TYPE 2

    #define CONFIG_OS_CFG_IRQN_TYPE_MIN MemoryManagement_IRQn 
    #define CONFIG_OS_CFG_IRQN_TYPE_MAX FMAC_IRQn
    #define CONFIG_OS_IRQN_TYPE_EXC_NUM_OFFSET (4 - MemoryManagement_IRQn)

    #define CONFIG_FLASH_TYPE 4
    #define CONFIG_FLASH_BASE_ADDR 0x08000000
    #define CONFIG_FLASH_SIZE (256*8192)
    #define CONFIG_FLASH_PAGE_SIZE 8192
    #define CONFIG_FLASH_NUM_BANK 2
    #define CONFIG_FLASH_NUM_PAGE 256
    #define CONFIG_FLASH_WRITE_BYTES 16
    #define CONFIG_FAULT_FLASH_BANK_NUM 1

#else
    #error Unknown processor
#endif

////////////////////////////////////////////////////////////////////////////////
// Common settings.
////////////////////////////////////////////////////////////////////////////////

// Module cmd.
#define CONFIG_CMD_MAX_TOKENS 10
#define CONFIG_CMD_MAX_CLIENTS 12

// Modules conole and ttys.
#define CONFIG_CONSOLE_PRINT_BUF_SIZE 240
#if defined STM32U575xx
    #define CONFIG_TTYS_1_PRESENT 1
    #define CONFIG_CONSOLE_DFLT_TTYS_INSTANCE TTYS_INSTANCE_1
#else
    #define CONFIG_TTYS_2_PRESENT 1
    #define CONFIG_CONSOLE_DFLT_TTYS_INSTANCE TTYS_INSTANCE_2
#endif

// Module draw.
#define CONFIG_DRAW_DFLT_LINK_1_LEN_MM 149
#define CONFIG_DRAW_DFLT_LINK_2_LEN_MM 119

// Module float.
#define CONFIG_FLOAT_TYPE_FLOAT 1
#define CONFIG_FLOAT_TYPE_DOUBLE 0
#define CONFIG_FLOAT_TYPE_LONG_DOUBLE 0

// Module i2c.
#define CONFIG_I2C_DFLT_TRANS_GUARD_TIME_MS 100

// Module tmphm.
#define CONFIG_TMPHM_1_DFLT_I2C_ADDR 0x44
#define CONFIG_TMPHM_DFLT_SAMPLE_TIME_MS 1000
#define CONFIG_TMPHM_DFLT_MEAS_TIME_MS 17
#define CONFIG_TMPHM_WDG_MS 5000

// Module wdg.
#define CONFIG_WDG_RUN_CHECK_MS 10
#define CONFIG_WDG_HARD_TIMEOUT_MS 4000

////////////////////////////////////////////////////////////////////////////////
// Feature-dependent configuration.
////////////////////////////////////////////////////////////////////////////////

// GPS feature.
#if defined CONFIG_FEAT_GPS
    #if defined STM32F103xB
        #define CONFIG_GPS_DFLT_TTYS_INSTANCE TTYS_INSTANCE_3
    #elif defined STM32F401xE
        #define CONFIG_GPS_DFLT_TTYS_INSTANCE TTYS_INSTANCE_6
    #elif defined STM32L452xx
        #define CONFIG_GPS_DFLT_TTYS_INSTANCE TTYS_INSTANCE_3
    #endif
#else
    #define CONFIG_GPS_DFLT_TTYS_INSTANCE CONFIG_DUMMY_0
#endif

// DRAW feature.
#if defined CONFIG_FEAT_DRAW

    #define CONFIG_DRAW_PRESENT 1
    #define CONFIG_STEP_1_PRESENT 1
    #define CONFIG_STEP_2_PRESENT 1

    #define CONFIG_DRAW_DFLT_STEP_INSTANCE_1 STEP_INSTANCE_1
    #define CONFIG_DRAW_DFLT_STEP_INSTANCE_2 STEP_INSTANCE_2

    #if defined STM32F401xE

        #define CONFIG_STEP_1_DFLT_GPIO_PORT DIO_PORT_A
        #define CONFIG_STEP_1_DFLT_DIO_PIN_A DIO_PIN_10
        #define CONFIG_STEP_1_DFLT_DIO_PIN_NOT_A DIO_PIN_12
        #define CONFIG_STEP_1_DFLT_DIO_PIN_B DIO_PIN_11
        #define CONFIG_STEP_1_DFLT_DIO_PIN_NOT_B DIO_PIN_9
        #define CONFIG_STEP_1_DFLT_IDLE_TIMER_MS 2000
        #define CONFIG_STEP_1_DFLT_REV_DIRECTION false
        #define CONFIG_STEP_1_DFLT_DRIVE_MODE STEP_DRIVE_MODE_FULL

        #define CONFIG_STEP_2_DFLT_GPIO_PORT DIO_PORT_C
        #define CONFIG_STEP_2_DFLT_DIO_PIN_A DIO_PIN_1
        #define CONFIG_STEP_2_DFLT_DIO_PIN_NOT_A DIO_PIN_3
        #define CONFIG_STEP_2_DFLT_DIO_PIN_B DIO_PIN_2
        #define CONFIG_STEP_2_DFLT_DIO_PIN_NOT_B DIO_PIN_0
        #define CONFIG_STEP_2_DFLT_IDLE_TIMER_MS 2000
        #define CONFIG_STEP_2_DFLT_REV_DIRECTION false
        #define CONFIG_STEP_2_DFLT_DRIVE_MODE STEP_DRIVE_MODE_FULL
    #else
        #error DRAW not supported
    #endif
#else
    #define CONFIG_DRAW_DFLT_STEP_INSTANCE_1 CONFIG_DUMMY_0
    #define CONFIG_DRAW_DFLT_STEP_INSTANCE_2 CONFIG_DUMMY_0
#endif

// TMPHM features.
#if defined CONFIG_FEAT_TMPHM
    #if defined STM32F401xE
        #define CONFIG_I2C_3_PRESENT 1
        #define CONFIG_TMPHM_1_DFLT_I2C_INSTANCE I2C_INSTANCE_3
        #define CONFIG_TMPHM_1_PRESENT 1
        #define CONFIG_TTYS_3_PRESENT 1
        #define CONFIG_I2C_1_PRESENT 1
    #else
        #error TMPHM not supported
    #endif
#else
    #define CONFIG_TMPHM_1_DFLT_I2C_INSTANCE CONFIG_DUMMY_0
#endif

// FLOAT feature.
#if defined CONFIG_FEAT_FLOAT
    #define CONFIG_FLOAT_PRESENT 1
#endif

// OS feature.
#if defined CONFIG_FEAT_OS
    #define CONFIG_OS_PRESENT 1
#endif

// CAN feature.
#if defined CONFIG_FEAT_CAN
    #define CONFIG_CAN_1_PRESENT 1
#endif

// FAULT feature.
#if defined CONFIG_FEAT_FAULT
    #define CONFIG_FAULT_PRESENT 1
    #define CONFIG_LWL_PRESENT 1
    #define CONFIG_WDG_PRESENT 1
    #define CONFIG_FLASH_PRESENT 1

    #define CONFIG_WDG_MAX_INIT_FAILS 3
    #define CONFIG_WDG_INIT_TIMEOUT_MS 8000

    #define CONFIG_FAULT_PANIC_TO_CONSOLE 1
    #define CONFIG_FAULT_PANIC_TO_FLASH 1

    #define CONFIG_TMPHM_WDG_ID 0
    #define CONFIG_WDG_NUM_WDGS 1
#endif

#endif // _CONFIG_H_
