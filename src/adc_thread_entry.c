/***********************************************************************************************************************
 * File Name    : hal_entry.c
 * Description  : Contains data structures and functions used in hal_entry.c.
 **********************************************************************************************************************/
/***********************************************************************************************************************
 * DISCLAIMER
 * This software is supplied by Renesas Electronics Corporation and is only intended for use with Renesas products. No
 * other uses are authorized. This software is owned by Renesas Electronics Corporation and is protected under all
 * applicable laws, including copyright laws.
 * THIS SOFTWARE IS PROVIDED "AS IS" AND RENESAS MAKES NO WARRANTIES REGARDING
 * THIS SOFTWARE, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. ALL SUCH WARRANTIES ARE EXPRESSLY DISCLAIMED. TO THE MAXIMUM
 * EXTENT PERMITTED NOT PROHIBITED BY LAW, NEITHER RENESAS ELECTRONICS CORPORATION NOR ANY OF ITS AFFILIATED COMPANIES
 * SHALL BE LIABLE FOR ANY DIRECT, INDIRECT, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES FOR ANY REASON RELATED TO THIS
 * SOFTWARE, EVEN IF RENESAS OR ITS AFFILIATES HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 * Renesas reserves the right, without notice, to make changes to this software and to discontinue the availability of
 * this software. By using this software, you agree to the additional terms and conditions found by accessing the
 * following link:
 * http://www.renesas.com/disclaimer
 *
 * Copyright (C) 2020 Renesas Electronics Corporation. All rights reserved.
 ***********************************************************************************************************************/

#include "adc_thread.h"
#include "common_utils.h"
#include "adc_periodic_scan.h"
#include "dtc_hal.h"
#include "adc_hal.h"
#include "gpt_hal.h"
#include "elc_hal.h"

/*******************************************************************************************************************//**
 * @addtogroup adc_gpt_periodic_sampling_ep
 * @{
 **********************************************************************************************************************/

FSP_CPP_HEADER
void R_BSP_WarmStart(bsp_warm_start_event_t event);
FSP_CPP_FOOTER

/* Private Function Declaration */
static void general_signal_acquisition_init (void);
static void handle_error(fsp_err_t err, char *err_str, module_name_t module);

/*extern variables */
extern uint16_t g_buffer_adc0_an0[NUM_SAMPLE_BUFFER][NUM_SAMPLES_PER_CHANNEL];

extern transfer_info_t g_transfer_adc0_an0[];
extern volatile bool g_adc0_flag;
extern volatile bool g_err_flag_adc0;

/* ADC Thread entry function */
void adc_thread_entry(void)
{
    fsp_pack_version_t version                 = {RESET_VALUE};
    fsp_err_t          err                     = FSP_SUCCESS;

    /* Version get API for FLEX pack information */
    R_FSP_VersionGet(&version);

    /* Example Project information printed on the RTT */
    APP_PRINT (BANNER_INFO, EP_VERSION, version.version_id_b.major, version.version_id_b.minor, version.version_id_b.patch);
    APP_PRINT (EP_INFO);

    /* Initialize the hal driver's for signal acquisition */
    general_signal_acquisition_init();

    APP_PRINT("\r\nADC0 AN0 periodic scan started...\r\n");

    err = scan_start_adc(&g_adc0_ctrl);
    handle_error(err,"\r\n** start_adc for Unit 0 FAILED ** \r\n", ALL);

    /*start gpt timer*/
    err = gpt_timer_start(&g_timer0_ctrl);
    handle_error(err,"\r\n** start gpt timer 0 FAILED ** \r\n", ALL);

    APP_PRINT("\r\nUser can now open waveform rendering of memory in e2studio to observe the output "
              "of adc gpt periodic sampling.\n\n");

    /*wait for callback event */
    while(true)
    {
        /* Check if adc 0 scan complete event */
        if(true == g_adc0_flag)
        {
            /*buffer select for ping pong buffer*/
            static uint8_t buffer_select = ZERO;
            buffer_select = (buffer_select == ZERO) ? ONE : ZERO;

            /*update destination address and length for transfer*/
            g_transfer_adc0_an0[ZERO].p_dest = (void*) &g_buffer_adc0_an0[buffer_select][ZERO];
            g_transfer_adc0_an0[ZERO].length = NUM_SAMPLES_PER_CHANNEL;

            /*Enable transfer*/
            err = dtc_enable(&g_transfer0_ctrl);
            handle_error(err,"\r\n** dtc enable failed for ADC0  ** \r\n", ALL);

            /*reset the variable */
            g_adc0_flag = false;
        }

        /* check if adc 0 scan complete event is not received */
        else if (true == g_err_flag_adc0)
        {
            handle_error(FSP_ERR_ABORTED,"\r\n** adc 0 scan complete event not received  ** \r\n", ALL);
        }
        else
        {
            /* do nothing */
        }

        tx_thread_sleep(10);
    }

#if BSP_TZ_SECURE_BUILD
    /* Enter non-secure code */
    R_BSP_NonSecureEnter();
#endif

}

/*******************************************************************************************************************//**
 * @brief       This functions initializes and enables adc, gpt, dtc and elc modules to be used as signal acquisition module.
 * @param[IN]   None
 * @retval      None
 * @retval      Any Other Error code apart from FSP_SUCCESS  Unsuccessful open.
 **********************************************************************************************************************/
static void general_signal_acquisition_init (void)
{
    fsp_err_t err = FSP_SUCCESS;

    /* Initialize all the links in the Event Link Controller */
    err = init_hal_elc(&g_elc_ctrl,&g_elc_cfg);
    if(FSP_SUCCESS != err)
    {
        APP_ERR_PRINT("\r\n** init_hal_elc FAILED ** \r\n");
        APP_ERR_TRAP(err);
    }

    /* Initialize DTC instance and reconfigure in chain mode for instance unit 0 */
    err = init_hal_dtc(&g_transfer0_ctrl, &g_transfer0_cfg);
    handle_error(err,"\r\n** dtc_init for unit 0, group a failed ** \r\n", ELC_MODULE);

    err = dtc_hal_reconfigure(&g_transfer0_ctrl,  &g_transfer_adc0_an0[ZERO]);
    handle_error(err,"\r\n** dtc reconfiguration for unit 0, group a failed ** \r\n", ELC_DTC1);


    /* Initialize ADC Unit 0 and configure channels for it*/
    err = init_hal_adc(&g_adc0_ctrl, &g_adc0_cfg);
    handle_error(err,"\r\n** adc_init for unit 0 failed ** \r\n", ELC_DTC_MODULE_ALL);

    err = adc_channel_config(&g_adc0_ctrl, &g_adc0_channel_cfg);
    handle_error(err,"\r\n** adc_channel_config for unit 0 failed ** \r\n", ELC_DTC_ADC0_MODULE);


    /* Initialize GPT timer 0*/
    err = init_hal_gpt(&g_timer0_ctrl, &g_timer0_cfg);
    handle_error(err,"\r\n** gpt_init for timer 0 failed ** \r\n", ELC_DTC_ADC_MODULE_ALL);

    /* Enable the operation of the Event Link Controller */
    err = elc_enable(&g_elc_ctrl);
    handle_error(err,"\r\n** R_ELC_Enable failed ** \r\n", ALL);


    /*Enable transfers for adc unit 0 group a*/
    err = dtc_enable(&g_transfer0_ctrl);
    handle_error(err,"\r\n** dtc_enable for ADC unit 0 group a failed ** \r\n", ALL);
}

/*******************************************************************************************************************//**
 *  @brief       Closes the ELC, DTC, GPT and ADC module, Print and traps error.
 *  @param[IN]   status    error status
 *  @param[IN]   err_str   error string
 *  @param[IN]   module    module to be closed
 *  @retval      None
 **********************************************************************************************************************/
static void handle_error( fsp_err_t err, char *err_str, module_name_t module)
{
    if(FSP_SUCCESS != err)
    {
        switch (module)
        {
            case ELC_MODULE:
            {
                /* close elc instance */
                deinit_hal_elc(&g_elc_ctrl);
            }
            break;
            case ELC_DTC1:
            {
                /* close elc instance */
                deinit_hal_elc(&g_elc_ctrl);

                /* close dtc instance */
                deinit_hal_dtc(&g_transfer0_ctrl);
            }
            break;
            case ELC_DTC12:
            {
                /* close elc instance */
                deinit_hal_elc(&g_elc_ctrl);

                /* close dtc instances */
                deinit_hal_dtc(&g_transfer0_ctrl);
            }
            break;

            case ELC_DTC_ADC0_MODULE:
            {
                /* close elc instance */
                deinit_hal_elc(&g_elc_ctrl);

                /* close dtc instances */
                deinit_hal_dtc(&g_transfer0_ctrl);

                /* close adc instance */
                deinit_hal_adc(&g_adc0_ctrl);
            }
            break;

            case ELC_DTC_ADC_MODULE_ALL:
            {
                /* close elc instance */
                deinit_hal_elc(&g_elc_ctrl);

                /* close DTC opened instance */
                deinit_hal_dtc(&g_transfer0_ctrl);

                /* close adc instance */
                deinit_hal_adc(&g_adc0_ctrl);

            }
            break;

            case ELC_DTC_ADC_GPT0_MODULE:
            {
                /* close elc instance */
                deinit_hal_elc(&g_elc_ctrl);

                /* close DTC instances */
                deinit_hal_dtc(&g_transfer0_ctrl);

                /*close adc instances*/
                deinit_hal_adc(&g_adc0_ctrl);


                /* close gpt instance */
                deinit_hal_gpt(&g_timer0_ctrl);
            }
            break;

            case ALL:
            {
                /* close elc instance */
                deinit_hal_elc(&g_elc_ctrl);

                /*close adc instances*/
                deinit_hal_adc(&g_adc0_ctrl);

                /* close GPT 0 and 1 instance */
                deinit_hal_gpt(&g_timer0_ctrl);

                /* close DTC opened instance */
                deinit_hal_dtc(&g_transfer0_ctrl);

            }
            break;

            default:
            {
                /*do nothing */
            }
        }
        APP_PRINT(err_str);
        APP_ERR_TRAP(err);
    }
}

/*******************************************************************************************************************//**
 * This function is called at various points during the startup process.  This implementation uses the event that is
 * called right before main() to set up the pins.
 *
 * @param[in]  event    Where at in the start up process the code is currently at
 **********************************************************************************************************************/
void R_BSP_WarmStart(bsp_warm_start_event_t event)
{
    if (BSP_WARM_START_RESET == event)
    {
#if BSP_FEATURE_FLASH_LP_VERSION != 0

        /* Enable reading from data flash. */
        R_FACI_LP->DFLCTL = 1U;

        /* Would normally have to wait tDSTOP(6us) for data flash recovery. Placing the enable here, before clock and
         * C runtime initialization, should negate the need for a delay since the initialization will typically take more than 6us. */
#endif
    }

    if (BSP_WARM_START_POST_C == event)
    {
        /* C runtime environment and system clocks are setup. */

        /* Configure pins. */
        R_IOPORT_Open( &g_ioport_ctrl, &g_bsp_pin_cfg);
    }
}

#if BSP_TZ_SECURE_BUILD

BSP_CMSE_NONSECURE_ENTRY void template_nonsecure_callable ();

/* Trustzone Secure Projects require at least one nonsecure callable function in order to build (Remove this if it is not required to build). */
BSP_CMSE_NONSECURE_ENTRY void template_nonsecure_callable ()
{

}
#endif

/*******************************************************************************************************************//**
 * @} (end addtogroup adc_gpt_periodic_sampling_ep)
 **********************************************************************************************************************/
