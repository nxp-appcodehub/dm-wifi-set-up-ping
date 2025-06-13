/** @file main.c
 *
 *  @brief main file
 *
 *  Copyright 2020, 2025 NXP
 *
 *  SPDX-License-Identifier: BSD-3-Clause
 */

///////////////////////////////////////////////////////////////////////////////
//  Includes
///////////////////////////////////////////////////////////////////////////////

// SDK Included Files
#include "board.h"
#include "fsl_debug_console.h"
#include "wpl.h"
#include "wlan_bt_fw.h"
#include "wlan.h"
#include "wifi.h"
#include "wm_net.h"
#include <osa.h>
#if CONFIG_NXP_WIFI_SOFTAP_SUPPORT
#include "dhcp-server.h"
#endif
#include "cli.h"
#include "wifi_ping.h"
#include "iperf.h"
#include "app.h"
#ifndef RW610
#include "wifi_bt_config.h"
#else
#include "fsl_power.h"
#include "fsl_ocotp.h"
#endif
#include "cli_utils.h"
#if CONFIG_HOST_SLEEP
#include "host_sleep.h"
#endif

/*******************************************************************************
 * Variables
 ******************************************************************************/

 static char ssid[WPL_WIFI_SSID_LENGTH];
 static char password[WPL_WIFI_PASSWORD_LENGTH];
 static ip_addr_t ip;

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
static void LinkStatusChangeCallback(bool linkState);
static void promptJoinNetwork(void);
static void promptPingAddress(void);
#if CONFIG_HOST_SLEEP
int wlan_hs_cli_init(void);
#endif

/*******************************************************************************
 * Code
 ******************************************************************************/

#if CONFIG_WPS2
#define MAIN_TASK_STACK_SIZE 6000
#else
#define MAIN_TASK_STACK_SIZE 4096
#endif

static void main_task(osa_task_param_t arg);

static OSA_TASK_DEFINE(main_task, WLAN_TASK_PRI_LOW, 1, MAIN_TASK_STACK_SIZE, 0);

OSA_TASK_HANDLE_DEFINE(main_task_Handle);

static void printSeparator(void)
{
    PRINTF("========================================\r\n");
}

/* Link lost callback */
static void LinkStatusChangeCallback(bool linkState)
{
    if (linkState == false)
    {
        PRINTF("-------- LINK LOST --------\r\n");
    }
    else
    {
        PRINTF("-------- LINK REESTABLISHED --------\r\n");
    }
}

static void promptJoinNetwork(void)
{
    wpl_ret_t result;
    int i = 0;
    char ch;

    while (true)
    {
        PRINTF("\r\nPlease enter parameters of WLAN to connect\r\n");

        /* SSID prompt */
        PRINTF("\r\nSSID: ");
        i = 0;
        while ((ch = GETCHAR()) != '\r' && i < WPL_WIFI_SSID_LENGTH - 1)
        {
            ssid[i] = ch;
            PUTCHAR(ch);
            i++;
        }
        ssid[i] = '\0';

        /* Password prompt */
        PRINTF("\r\nPassword (for unsecured WLAN press Enter): ");
        i = 0;
        while ((ch = GETCHAR()) != '\r' && i < WPL_WIFI_PASSWORD_LENGTH - 1)
        {
            password[i] = ch;
            PUTCHAR('*');
            i++;
        }
        password[i] = '\0';
        PRINTF("\r\n");

        /* Add Wifi network as known network */
        result = WPL_AddNetwork(ssid, password, ssid);
        if (result != WPLRET_SUCCESS)
        {
            PRINTF("[!] WPL_AddNetwork: Failed to add network, error:  %d\r\n", (uint32_t)result);
            continue;
        }
        PRINTF("[i] WPL_AddNetwork: Success\r\n");

        /* Join the network using label */
        PRINTF("[i] Trying to join the network...\r\n");
        result = WPL_Join(ssid);
        if (result != WPLRET_SUCCESS)
        {
            PRINTF("[!] WPL_Join: Failed to join network, error: %d\r\n", (uint32_t)result);
            if (WPL_RemoveNetwork(ssid) != WPLRET_SUCCESS)
                __BKPT(0);
            continue;
        }
        PRINTF("[i] WPL_Join: Success\r\n");

        /* SSID and password was OK, exit the prompt */
        break;
    }
}

static void promptPingAddress(void)
{
    uint16_t size  = PING_DEFAULT_SIZE;
    uint16_t count = PING_DEFAULT_COUNT;
    uint32_t timeout = PING_DEFAULT_TIMEOUT_SEC;
    int i = 0;
    char ip_string[IP4ADDR_STRLEN_MAX];
    char ch;

    while (true)
    {
        PRINTF("\r\nPlease enter a valid IPv4 address to test the connection\r\n");

        /* Ping IP address prompt */
        PRINTF("\r\nIP address: ");
        i = 0;
        while ((ch = GETCHAR()) != '\r' && i < IP4ADDR_STRLEN_MAX - 1)
        {
            ip_string[i] = ch;
            PUTCHAR(ch);
            i++;
        }
        ip_string[i] = '\0';
        PRINTF("\r\n");

        if (ipaddr_aton(ip_string, &ip) == 0)
        {
            PRINTF("[!] %s is not a valid IPv4 address\r\n", ip_string);
        	continue;
        }
        	(void)ping(count,1, size, timeout, &ip);
    }
}

static void main_task(osa_task_param_t arg)
{
	int ret;
    wpl_ret_t result;
    char *scan_result;

#ifdef CONFIG_HOST_SLEEP
    hostsleep_init();
#endif

    PRINTF("\r\n Starting wifi_setup DEMO \r\n");
    printSeparator();

    result = WPL_Init();
    if (result != WPLRET_SUCCESS)
    {
        PRINTF("[!] WPL_Init: Failed, error: %d\r\n", (uint32_t)result);
        __BKPT(0);
    }

    PRINTF("[i] WPL_Init: Success\r\n");

    result = WPL_Start(LinkStatusChangeCallback);
    if (result != WPLRET_SUCCESS)
    {
        PRINTF("[!] WPL_Start: Failed, error: %d\r\n", (uint32_t)result);
        __BKPT(0);
    }
    PRINTF("[i] WPL_Start: Success\r\n");

    /* Scan the local area for available Wifi networks. The scan will print the results to the terminal
     * and return the results as JSON string */
    PRINTF("\r\nInitiating scan...\r\n\r\n");
    scan_result = WPL_Scan();
    if (scan_result == NULL)
    {
        PRINTF("[!] WPL_Scan: Failed to scan\r\n");
        __BKPT(0);
    }
    vPortFree(scan_result);

    promptJoinNetwork();
    promptPingAddress();
    vTaskDelete(NULL);
}

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
int main(void)
{
    OSA_Init();

    BOARD_InitHardware();
#ifdef RW610
    POWER_PowerOffBle();
#endif

    printSeparator();
    PRINTF("wifi setup demo\r\n");
    printSeparator();

    (void)OSA_TaskCreate((osa_task_handle_t)main_task_Handle, OSA_TASK(main_task), NULL);

    OSA_Start();

    return 0;
}
