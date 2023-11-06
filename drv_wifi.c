/* *****************************************************************************
 * File:   drv_wifi.c
 * Author: DL
 *
 * Created on 2023 11 05
 * 
 * Description: ...
 * 
 **************************************************************************** */

/* *****************************************************************************
 * Header Includes
 **************************************************************************** */
#include "drv_wifi.h"

#include <sdkconfig.h>

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/ip4_addr.h"
#include <netdb.h>

#include "drv_nvs.h"


/* *****************************************************************************
 * Configuration Definitions
 **************************************************************************** */
#define TAG "drv_wifi"

#define CONFIG_DRV_ESPTOUCH_USE                     0   /* to do: make component drv_esptouch */
#define CONFIG_APP_SOCKET_UDP_USE                   0   /* to do: make component app_socket_udp or relevant */
#define CONFIG_DRV_HTTP_USE                         0   /* to do: make components drv_http or remove */

#define USE_DRV_WIFI_DRV_ERT_COMBINED_SEMPHR_GOT_IP 0   /* to do: remove and combine got ip in parent module ex. named drv_network or drv_net */

/* *****************************************************************************
 * Constants and Macros Definitions
 **************************************************************************** */

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

#define MACSTR_U "%02X:%02X:%02X:%02X:%02X:%02X"



#if CONFIG_DRV_WIFI_AUTH_OPEN
#define WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_DRV_WIFI_AUTH_WEP
#define WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_DRV_WIFI_AUTH_WPA_PSK
#define WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_DRV_WIFI_AUTH_WPA2_PSK
#define WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_DRV_WIFI_AUTH_WPA_WPA2_PSK
#define WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_DRV_WIFI_AUTH_WPA3_PSK
#define WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_DRV_WIFI_AUTH_WPA2_WPA3_PSK
#define WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_DRV_WIFI_AUTH_WAPI_PSK
#define WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif



/* *****************************************************************************
 * Enumeration Definitions
 **************************************************************************** */

/* *****************************************************************************
 * Type Definitions
 **************************************************************************** */

/* *****************************************************************************
 * Function-Like Macros
 **************************************************************************** */

/* *****************************************************************************
 * Variables Definitions
 **************************************************************************** */
SemaphoreHandle_t flag_wifi_got_ip = NULL;

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

esp_netif_t* esp_netif_ap = NULL;
esp_netif_t* esp_netif_sta = NULL;

uint8_t connected_stations = 0;
bool bStationConnected = false;
bool bStationConnecting = false;
static int s_retry_num = 0;
static int s_reconnect_timeout = 0;


char ssid_wifi_ap_fix[32] = CONFIG_DRV_WIFI_SSID_AP;    /* to do: add as configuration whether to fix the name with ending of MAC address */
char ssid_wifi_ap[32] = CONFIG_DRV_WIFI_SSID_AP;
char pass_wifi_ap[32] = CONFIG_DRV_WIFI_PASS_AP;

char ssid_wifi_sta[32] = CONFIG_DRV_WIFI_SSID_STA;
char pass_wifi_sta[32] = CONFIG_DRV_WIFI_PASS_STA;


uint8_t mac_wifi_sta[6];
uint8_t mac_wifi_ap[6];



/* Station Configuration */
#if CONFIG_DRV_NVS_USE
bool bSkipStaConfigSave = false;
#endif

char ip_sta_def[16] = "192.168.0.234";
char mask_sta_def[16] = "255.255.255.0";
char gw_sta_def[16] = "192.168.0.1";

uint32_t dhcp_sta = 1;
uint32_t ip_sta = 0;
uint32_t mask_sta = 0;
uint32_t gw_sta = 0;


/* *****************************************************************************
 * Prototype of functions definitions
 **************************************************************************** */

/* *****************************************************************************
 * Functions
 **************************************************************************** */


static void wifi_connect(void)
{
    esp_err_t err = esp_wifi_connect();
    if(err == ESP_OK)
    {
        ESP_LOGI(TAG,"Wifi Station Connected Success");
    }
    else
    if(err == ESP_ERR_WIFI_NOT_INIT)
    {
        ESP_LOGE(TAG,"WiFi is not initialized by esp_wifi_init");
    }
    else
    if(err == ESP_ERR_WIFI_NOT_STARTED)
    {
        ESP_LOGE(TAG,"WiFi is not started by esp_wifi_start");
    }
    else
    if(err == ESP_ERR_WIFI_CONN)
    {
        ESP_LOGE(TAG,"WiFi internal error, station or soft-AP control block wrong");
    }
    else
    if(err == ESP_ERR_WIFI_SSID)
    {
        ESP_LOGE(TAG,"SSID of AP which station connects is invalid");
    }
    else
    {
        ESP_LOGE(TAG,"Wifi Station Connected Failure Unknown error : %d", err);
    }

}


static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) 
    {
        connected_stations++;
        ESP_LOGI(TAG, "Station Connected to %s. Total %d Station(s)", ssid_wifi_ap_fix, connected_stations);
        if (connected_stations == 1)
        {
            #if CONFIG_DRV_HTTP_USE
            drv_http_start();
            #endif
             //drv_esptouch_start();
             //drv_esptouch_connected();
        }
        #if CONFIG_APP_SOCKET_UDP_USE
        app_socket_udp_wifi_ap_bootp_set_deny_connect(false);
        #endif
    } 
    else 
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) 
    {
        if (connected_stations)
        {
            connected_stations--;
            ESP_LOGI(TAG, "Station Disconnected from %s. Left %d Station(s)", ssid_wifi_ap_fix, connected_stations);
            if (connected_stations == 0)
            {
                #if CONFIG_APP_SOCKET_UDP_USE
                app_socket_udp_wifi_ap_bootp_set_deny_connect(true);
                #endif
                #if CONFIG_DRV_HTTP_USE
                drv_http_stop();
                #endif
                //drv_esptouch_disconnected();
            }
        }
        else
        {
            ESP_LOGE(TAG, "Disconnected More Stations than the ones that were connected to %s", ssid_wifi_ap_fix);
        }
    } 
    else 
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) 
    {
        s_retry_num = 0;
        wifi_connect();
    } 
    else 
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) 
    {
        #if CONFIG_APP_SOCKET_UDP_USE
        app_socket_udp_wifi_sta_bootp_set_deny_connect(true);
        #endif
        bStationConnected = false;
        bStationConnecting = true;
        ESP_LOGI(TAG,"connect to the AP success");
        s_retry_num = 0;
    } 
    else 
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) 
    {
        #if CONFIG_APP_SOCKET_UDP_USE
        app_socket_udp_wifi_sta_bootp_set_deny_connect(true);
        #endif
        #if CONFIG_DRV_ESPTOUCH_USE
        drv_esptouch_disconnected();
        #endif
        ESP_LOGI(TAG,"connect to the AP fail");
        bStationConnected = false;
        bStationConnecting = false;
        
        xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);

        if (s_retry_num < 10)
        {
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP %d times", s_retry_num);
            wifi_connect();
        }
        else
        {
            #if CONFIG_DRV_ESPTOUCH_USE
            drv_esptouch_start();
            #endif
        }
        s_reconnect_timeout = 0;

    } 
    else 
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) 
    {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGW(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;

        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        #if CONFIG_DRV_ESPTOUCH_USE
        drv_esptouch_connected();
        #endif

        bStationConnected = true;


        const esp_netif_ip_info_t *ip_info = &event->ip_info;

        esp_netif_t* esp_netif = event->esp_netif;

        
        if (esp_netif == esp_netif_sta)
        {
            ESP_LOGI(TAG, "Wifi STATION Got IP Address");
            ESP_LOGI(TAG, "~~~~~~~~~~~");
            ESP_LOGI(TAG, "STAIP:" IPSTR, IP2STR(&ip_info->ip));
            ESP_LOGI(TAG, "STAMASK:" IPSTR, IP2STR(&ip_info->netmask));
            ESP_LOGI(TAG, "STAGW:" IPSTR, IP2STR(&ip_info->gw));
            ESP_LOGI(TAG, "~~~~~~~~~~~");

            ESP_LOGI(TAG, "esp_netif:0x%08X", (int)esp_netif);
            ESP_LOGI(TAG, "~~~~~~~~~~~");
            if (esp_netif != NULL)
            {
                esp_netif_dns_info_t dns_info_sta;
                esp_netif_get_dns_info(esp_netif,ESP_NETIF_DNS_MAIN, &dns_info_sta);
                ESP_LOGI(TAG, "DNS PRIMER: " IPSTR, IP2STR(&dns_info_sta.ip.u_addr.ip4));
                if (dns_info_sta.ip.u_addr.ip4.addr == 0)
                {
                    dns_info_sta.ip.u_addr.ip4.addr = inet_addr("8.8.8.8");
                    esp_netif_set_dns_info(esp_netif,ESP_NETIF_DNS_MAIN, &dns_info_sta);
                    esp_netif_get_dns_info(esp_netif,ESP_NETIF_DNS_MAIN, &dns_info_sta);
                    ESP_LOGI(TAG, "DNS PRIFIX: " IPSTR, IP2STR(&dns_info_sta.ip.u_addr.ip4));
                }
                esp_netif_get_dns_info(esp_netif,ESP_NETIF_DNS_BACKUP, &dns_info_sta);
                ESP_LOGI(TAG, "DNS SECOND: " IPSTR, IP2STR(&dns_info_sta.ip.u_addr.ip4));
                if (dns_info_sta.ip.u_addr.ip4.addr == 0)
                {
                    dns_info_sta.ip.u_addr.ip4.addr = inet_addr("8.8.4.4");
                    esp_netif_set_dns_info(esp_netif,ESP_NETIF_DNS_BACKUP, &dns_info_sta);
                    esp_netif_get_dns_info(esp_netif,ESP_NETIF_DNS_BACKUP, &dns_info_sta);
                    ESP_LOGI(TAG, "DNS SECFIX: " IPSTR, IP2STR(&dns_info_sta.ip.u_addr.ip4));
                }
                esp_netif_get_dns_info(esp_netif,ESP_NETIF_DNS_FALLBACK, &dns_info_sta);
                ESP_LOGI(TAG, "DNS FALLEN: " IPSTR, IP2STR(&dns_info_sta.ip.u_addr.ip4));
                if (dns_info_sta.ip.u_addr.ip4.addr == 0)
                {
                    dns_info_sta.ip.u_addr.ip4.addr = ip_info->gw.addr;
                    esp_netif_set_dns_info(esp_netif,ESP_NETIF_DNS_FALLBACK, &dns_info_sta);
                    esp_netif_get_dns_info(esp_netif,ESP_NETIF_DNS_FALLBACK, &dns_info_sta);
                    ESP_LOGI(TAG, "DNS FALFIX: " IPSTR, IP2STR(&dns_info_sta.ip.u_addr.ip4));
                }
                ESP_LOGI(TAG, "~~~~~~~~~~~");
            }
            #if USE_DRV_WIFI_DRV_ERT_COMBINED_SEMPHR_GOT_IP
            drv_wifi_or_eth_give_semaphore();
            #endif
            xSemaphoreGive(flag_wifi_got_ip);

        }
        else
        if (esp_netif == esp_netif_ap)
        {
            ESP_LOGI(TAG, "Wifi Soft-AP Got IP Address");
        }
        else
        {
            ESP_LOGI(TAG, "Unknown Got IP Address esp_netif: 0x%08X", (int)esp_netif);
        }




    }
    else
    {
        ESP_LOGE(TAG, "Unknown Wifi Event: event_base: %d event_id:%d", (int)event_base, (int)event_id);
    }

}



char* drv_wifi_get_ssid_sta( size_t* pnMaxSize)
{
    *pnMaxSize = sizeof(ssid_wifi_sta);
    return ssid_wifi_sta;
}
char* drv_wifi_get_pass_sta(size_t* pnMaxSize)
{
    *pnMaxSize = sizeof(pass_wifi_sta);
    return pass_wifi_sta;
}



#if CONFIG_DRV_NVS_USE  
void drv_wifi_save_config(void)
{
    
    if (bSkipStaConfigSave == false)
    {
        int err_sta = 0;  
        if (drv_nvs_write_u32("app_cfg","dhcp_sta", dhcp_sta)) err_sta++;
        if (drv_nvs_write_u32("app_cfg","ip_sta", ip_sta)) err_sta++;
        if (drv_nvs_write_u32("app_cfg","mask_sta", mask_sta)) err_sta++;
        if (drv_nvs_write_u32("app_cfg","gw_sta", gw_sta)) err_sta++;

        if (err_sta > 0)
        {
            ESP_LOGE(TAG, "Error Data Wrte to NVS ip info of wifi station");
        }

    }
}


esp_err_t nvs_write_ssid_sta(char* pString)
{
    esp_err_t eError;
    eError = drv_nvs_write_string("app_cfg", "ssid_sta", pString);
    return eError;
}

esp_err_t nvs_read_ssid_sta(char* pString, size_t nMaxSize)
{
    esp_err_t eError;
    eError = drv_nvs_read_string("app_cfg", "ssid_sta", pString, nMaxSize);
    return eError;
}

void drv_wifi_load_config(void)
{
    /* get ssid_sta and pass_sta */
    size_t ssid_size = 0;
    char* ssid_sta = drv_wifi_get_ssid_sta(&ssid_size);             /* here get the pointer to the ssid in drv_wifi module */
    nvs_read_ssid_sta(ssid_sta, ssid_size);                     /* nvs -> drv_wifi memory */
    size_t pass_size = 0;
    char* pass_sta = drv_wifi_get_pass_sta(&pass_size);             /* here get the pointer to the pass in drv_wifi module */
    drv_nvs_read_string("app_cfg","pass_sta", pass_sta, pass_size); /* nvs -> drv_wifi memory */

    /* get ip_info sta */
    dhcp_sta = 1;
    ip_sta = inet_addr(ip_sta_def);
    mask_sta = inet_addr(mask_sta_def);
    gw_sta = inet_addr(gw_sta_def);

    int err_sta = 0;    
    if (drv_nvs_read_u32("app_cfg","dhcp_sta", &dhcp_sta)) err_sta++;
    if (drv_nvs_read_u32("app_cfg","ip_sta", &ip_sta)) err_sta++;
    if (drv_nvs_read_u32("app_cfg","mask_sta", &mask_sta)) err_sta++;
    if (drv_nvs_read_u32("app_cfg","gw_sta", &gw_sta)) err_sta++;

    if ((err_sta != 0) && (err_sta != 4))
    {
        ESP_LOGE(TAG, "Error Partial Data Read from NVS");
        dhcp_sta = 1;
        ip_sta = inet_addr("192.168.0.234");
        mask_sta = inet_addr("255.255.255.0");
        gw_sta = inet_addr("192.168.0.1");
    }
}
#endif

void drv_wifi_set_static_ip(esp_netif_t *netif, const char *ip_address, const char *ip_netmask, const char *gw_address, bool bSkipSave)
{
    esp_netif_dhcp_status_t status;
    if (esp_netif_dhcpc_get_status(netif, &status) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read dhcp client status");
        return;
    }
    else if (status == ESP_NETIF_DHCP_STARTED)
    {
        if (esp_netif_dhcpc_stop(netif) != ESP_OK) 
        {
            ESP_LOGE(TAG, "Failed to stop dhcp client");
            return;
        }
    }

    esp_netif_ip_info_t ip_info;
    //memset(&ip, 0 , sizeof(esp_netif_ip_info_t));

    if(netif == esp_netif_ap)
    {
        ESP_LOGI(TAG, "Soft-AP IP Change Request");
    }
    else
    if(netif == esp_netif_sta)
    {
        ESP_LOGI(TAG, "Station IP Change Request");
    }
    else
    {
        ESP_LOGI(TAG, "IP Change Request unknown interface");
        return;
    }


    //ESP_ERROR_CHECK(esp_netif_get_ip_info(netif, &ip_info));    /* this function returns first netif->lwip data which is 0 */
    ESP_ERROR_CHECK(esp_netif_get_old_ip_info(netif, &ip_info));

    ESP_LOGI(TAG, "From:");
    
    ESP_LOGI(TAG, "  IP: " IPSTR, IP2STR(&ip_info.ip));
    ESP_LOGI(TAG, "MASK: " IPSTR, IP2STR(&ip_info.netmask));
    ESP_LOGI(TAG, "  GW: " IPSTR, IP2STR(&ip_info.gw));

    ESP_LOGI(TAG, "To  :");

    if((ip_address != NULL) && (strlen(ip_address) > 0))ip_info.ip.addr = ipaddr_addr(ip_address);
    if((ip_netmask != NULL) && (strlen(ip_netmask) > 0))ip_info.netmask.addr = ipaddr_addr(ip_netmask);
    if((gw_address != NULL) && (strlen(gw_address) > 0))ip_info.gw.addr = ipaddr_addr(gw_address);
    ESP_LOGI(TAG, "  IP: " IPSTR, IP2STR(&ip_info.ip));
    ESP_LOGI(TAG, "MASK: " IPSTR, IP2STR(&ip_info.netmask));
    ESP_LOGI(TAG, "  GW: " IPSTR, IP2STR(&ip_info.gw));

    if (esp_netif_set_ip_info(netif, &ip_info) != ESP_OK) 
    {
        ESP_LOGE(TAG, "Failed to set ip info 3");
        return;
    }

    if (bSkipSave == false)
    {
        if(netif == esp_netif_sta)
        {
            dhcp_sta = 0;
            ip_sta = ip_info.ip.addr;
            mask_sta = ip_info.netmask.addr;
            gw_sta = ip_info.gw.addr;
            #if CONFIG_DRV_NVS_USE 
            drv_wifi_save_config();
            #endif
        }
    }

}


void drv_wifi_set_dynamic_ip(esp_netif_t *netif)
{
    esp_netif_dhcp_status_t status;
    if (esp_netif_dhcpc_get_status(netif, &status) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read dhcp client status");
        return;
    }
    else if (status == ESP_NETIF_DHCP_STOPPED)
    {
        if (esp_netif_dhcpc_start(netif) != ESP_OK) 
        {
            ESP_LOGE(TAG, "Failed to start dhcp client");
            return;
        }
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(netif, &ip_info) != ESP_OK) 
        {
            ESP_LOGE(TAG, "Failed to get ip info");

        }
        else
        {

        }
        dhcp_sta = 1;
        ip_sta = ip_info.ip.addr;
        mask_sta = ip_info.netmask.addr;
        gw_sta = ip_info.gw.addr;
        #if CONFIG_DRV_NVS_USE
        drv_wifi_save_config();
        #endif
    }
}

esp_netif_t* drv_wifi_get_netif_sta(void)
{
    return esp_netif_sta;
}



void drv_wifi_init(void)
{
    #if CONFIG_DRV_NVS_USE
    drv_wifi_load_config();
    #endif


    flag_wifi_got_ip = xSemaphoreCreateBinary(); 

    #if USE_DRV_WIFI_DRV_ERT_COMBINED_SEMPHR_GOT_IP
    drv_wifi_or_eth_create_semaphore();
    #endif

    #if CONFIG_DRV_ESPTOUCH_USE
    drv_esptouch_init();
    #endif

    //if (wifi_netif_init_passed == false)
    //{
        //wifi_netif_init_passed = true;
        //drv_eth_netif_init_passed();

        // Initialize TCP/IP network interface (should be called only once in application)
        //ESP_ERROR_CHECK(esp_netif_init());

        // Create default event loop that running in background
        //ESP_ERROR_CHECK(esp_event_loop_create_default());
    //}

    s_wifi_event_group = xEventGroupCreate(); 
    

#if 0   /* should be same as below - consider testing */
    esp_netif_inherent_config_t esp_netif_config_sta = ESP_NETIF_INHERENT_DEFAULT_WIFI_STA();
    esp_netif_inherent_config_t esp_netif_config_ap = ESP_NETIF_INHERENT_DEFAULT_WIFI_AP();
    esp_netif_sta = esp_netif_create_wifi(WIFI_IF_STA, esp_netif_config_sta);
    esp_netif_ap = esp_netif_create_wifi(WIFI_IF_AP, esp_netif_config_ap);
#else
    esp_netif_sta = esp_netif_create_default_wifi_sta();
    esp_netif_ap = esp_netif_create_default_wifi_ap();
#endif

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    /* Get and Print wifi MACs */
    esp_wifi_get_mac(ESP_IF_WIFI_STA, mac_wifi_sta);
    ESP_LOGI(TAG, "Wifi Station MAC "MACSTR_U, MAC2STR(mac_wifi_sta));
    esp_wifi_get_mac(ESP_IF_WIFI_AP, mac_wifi_ap);

    ESP_LOGI(TAG, "Access Point MAC "MACSTR_U, MAC2STR(mac_wifi_ap));

    /* Generate Ending of AP ssid with station MAC ending */
    char* p_ssid_ap;
    asprintf(&p_ssid_ap, "%s-%02X%02X", ssid_wifi_ap, mac_wifi_sta[4], mac_wifi_sta[5]);
    strcpy(ssid_wifi_ap_fix, p_ssid_ap);
    free(p_ssid_ap);

    /* Wifi Configuration AP */
    wifi_config_t wifi_config_ap = {
        .ap = {
            .max_connection = CONFIG_DRV_WIFI_AP_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
        },
    };
    memcpy(wifi_config_ap.ap.ssid, ssid_wifi_ap_fix, strlen(ssid_wifi_ap_fix));
    memcpy(wifi_config_ap.ap.password, pass_wifi_ap, strlen(pass_wifi_ap));
    if (strlen(pass_wifi_ap) == 0) wifi_config_ap.ap.authmode = WIFI_AUTH_OPEN;

    /* Wifi Configuration STA */
    wifi_config_t wifi_config_sta = {
        .sta = {
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
	     .threshold.authmode = WIFI_SCAN_AUTH_MODE_THRESHOLD,
        },
    };
    memcpy(wifi_config_sta.sta.ssid, ssid_wifi_sta, strlen(ssid_wifi_sta));
    memcpy(wifi_config_sta.sta.password, pass_wifi_sta, strlen(pass_wifi_sta));
    if (strlen(pass_wifi_sta) == 0) wifi_config_sta.sta.threshold.authmode = WIFI_AUTH_OPEN;

    ESP_LOGI(TAG, "Wifi Station SSID: %s", ssid_wifi_sta);
    ESP_LOGI(TAG, "Wifi Station PASS: %s", pass_wifi_sta);
    ESP_LOGI(TAG, "Wifi Soft-AP SSID: %s", ssid_wifi_ap_fix);
    ESP_LOGI(TAG, "Wifi Soft-AP PASS: %s", pass_wifi_ap);


    wifi_mode_t wifi_mode = WIFI_MODE_APSTA;  /* WIFI_MODE_STA, WIFI_MODE_AP, WIFI_MODE_APSTA */
    ESP_ERROR_CHECK(esp_wifi_set_mode(wifi_mode) );

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config_ap) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config_sta) );

    esp_netif_ip_info_t ip_info_ap;
    esp_netif_get_ip_info(esp_netif_ap, &ip_info_ap);
    ESP_LOGI(TAG, "Soft-AP IP Soft-AP: " IPSTR, IP2STR(&ip_info_ap.ip));
    ESP_LOGI(TAG, "      MASK Soft-AP: " IPSTR, IP2STR(&ip_info_ap.netmask));
    ESP_LOGI(TAG, "   GATEWAY Soft-AP: " IPSTR, IP2STR(&ip_info_ap.gw));

    esp_netif_dns_info_t dns_info_ap;
    esp_netif_get_dns_info(esp_netif_ap, ESP_NETIF_DNS_MAIN, &dns_info_ap);
    ESP_LOGI(TAG, "DNS PRIMER Soft-AP: " IPSTR, IP2STR(&dns_info_ap.ip.u_addr.ip4));
    esp_netif_get_dns_info(esp_netif_ap,ESP_NETIF_DNS_BACKUP, &dns_info_ap);
    ESP_LOGI(TAG, "DNS SECOND Soft-AP: " IPSTR, IP2STR(&dns_info_ap.ip.u_addr.ip4));
    esp_netif_get_dns_info(esp_netif_ap,ESP_NETIF_DNS_FALLBACK, &dns_info_ap);
    ESP_LOGI(TAG, "DNS FALLEN Soft-AP: " IPSTR, IP2STR(&dns_info_ap.ip.u_addr.ip4));

    #if CONFIG_DRV_NVS_USE
    bSkipStaConfigSave = true;
    #endif
    /* update load configuration */
    if (dhcp_sta)
    {
        drv_wifi_set_dynamic_ip(drv_wifi_get_netif_sta());
        ESP_LOGI(TAG, "Wifi Station Dynamic IP Used");
    }
    else
    {
        char ip_address_buffer[16] = {0};
        char *ip_address = ip_address_buffer;
        ip4addr_ntoa_r((ip4_addr_t*)&ip_sta, ip_address, sizeof(ip_address_buffer));
        char ip_netmask_buffer[16] = {0};
        char *ip_netmask = ip_netmask_buffer;
        ip4addr_ntoa_r((ip4_addr_t*)&mask_sta, ip_netmask, sizeof(ip_netmask_buffer));
        char ip_gateway_buffer[16] = {0};
        char *ip_gateway = ip_gateway_buffer;
        ip4addr_ntoa_r((ip4_addr_t*)&gw_sta, ip_gateway, sizeof(ip_gateway_buffer));
        drv_wifi_set_static_ip(drv_wifi_get_netif_sta(), ip_address, ip_netmask, ip_gateway, true);
        ESP_LOGI(TAG, "Wifi Station Static IP Used");
    }
    #if CONFIG_DRV_NVS_USE
    bSkipStaConfigSave = false;
    #endif

    ESP_ERROR_CHECK(esp_wifi_start() );

    int8_t j;
    esp_wifi_get_max_tx_power(&j);
    esp_wifi_set_ps(WIFI_PS_NONE);//may be not needed
    

    ESP_LOGI(TAG, "esp_wifi_get_max_tx_power %d", j);

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) 
    {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s", ssid_wifi_sta, pass_wifi_sta);
    } 
    else 
    if (bits & WIFI_FAIL_BIT) 
    {
        ESP_LOGE(TAG, "Failed to connect to SSID:%s, password:%s", ssid_wifi_sta, pass_wifi_sta);        
        //drv_esptouch_start();
    } 
    else 
    {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }


}

void drv_wifi_print_rssi(void)
{
    wifi_ap_record_t ap_info;
    if (bStationConnecting == true)
    {
        esp_wifi_sta_get_ap_info(&ap_info);
        ESP_LOGI(TAG, "Wifi rssi: %d", ap_info.rssi);
    }
}

void drv_wifi_sta_ssid_pass_set(char* ssid, char* pass, bool bssid_set, uint8_t* bssid)
{
    wifi_config_t wifi_config;
    bzero(&wifi_config, sizeof(wifi_config_t));
    if (ssid != NULL)
    {
        memcpy(wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    }
    if (pass != NULL)
    {
        memcpy(wifi_config.sta.password, pass, sizeof(wifi_config.sta.password));
    }
    wifi_config.sta.bssid_set = bssid_set;
    if (wifi_config.sta.bssid_set == true) 
    {
        memcpy(wifi_config.sta.bssid, bssid, sizeof(wifi_config.sta.bssid));
    }

    ESP_ERROR_CHECK( esp_wifi_disconnect() );
    #if CONFIG_APP_SOCKET_UDP_USE
    app_socket_udp_wifi_sta_bootp_set_deny_connect(true);
    #endif
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );

    ESP_LOGI(TAG, "try connect on drv_wifi_sta_ssid_pass_set");
    ESP_LOGI(TAG, "stopping smart config (esptouch)");
    #if CONFIG_DRV_ESPTOUCH_USE
    drv_esptouch_done();
    #endif
    ESP_LOGI(TAG, "try reconnect to AP now");

    wifi_connect();

    /* set ssid_sta and pass_sta */
    size_t ssid_size = 0;
    char* ssid_sta = drv_wifi_get_ssid_sta(&ssid_size);
    if (ssid != NULL) strcpy(ssid_sta, ssid); else ssid_sta[0] = '\0';
    nvs_write_ssid_sta(ssid_sta);
    size_t pass_size = 0;
    char* pass_sta = drv_wifi_get_pass_sta(&pass_size);
    if (pass != NULL) strcpy(pass_sta, pass); else pass_sta[0] = '\0';
    drv_nvs_write_string("app_cfg","pass_sta", pass_sta);
}





