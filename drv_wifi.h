/* *****************************************************************************
 * File:   drv_wifi.h
 * Author: DL
 *
 * Created on 2023 11 05
 * 
 * Description: ...
 * 
 **************************************************************************** */
#pragma once

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


/* *****************************************************************************
 * Header Includes
 **************************************************************************** */
#include <stdbool.h>
#include <stdint.h>
#include "esp_netif.h"

/* *****************************************************************************
 * Configuration Definitions
 **************************************************************************** */

/* *****************************************************************************
 * Constants and Macros Definitions
 **************************************************************************** */

/* *****************************************************************************
 * Enumeration Definitions
 **************************************************************************** */

/* *****************************************************************************
 * Type Definitions
 **************************************************************************** */

/* *****************************************************************************
 * Function-Like Macro
 **************************************************************************** */

/* *****************************************************************************
 * Variables External Usage
 **************************************************************************** */ 

/* *****************************************************************************
 * Function Prototypes
 **************************************************************************** */
void drv_wifi_set_static_ip(esp_netif_t *netif, const char *ip_address, const char *ip_netmask, const char *gw_address, bool bSkipSave);
void drv_wifi_set_dynamic_ip(esp_netif_t *netif);
void drv_wifi_reconnect(void);
esp_netif_t* drv_wifi_get_netif_sta(void);
esp_netif_t* drv_wifi_get_netif_ap(void);
bool drv_wifi_get_ap_connected(void);
bool drv_wifi_get_sta_connected(void);
void drv_wifi_init(void);
void drv_wifi_print_rssi(void);
void drv_wifi_sta_ssid_pass_set(char* ssid, char* pass, bool bssid_set, uint8_t* bssid);




#ifdef __cplusplus
}
#endif /* __cplusplus */


