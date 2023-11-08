/* *****************************************************************************
 * File:   cmd_wifi.c
 * Author: Dimitar Lilov
 *
 * Created on 2022 06 18
 * 
 * Description: ...
 * 
 **************************************************************************** */

/* *****************************************************************************
 * Header Includes
 **************************************************************************** */
#include "cmd_wifi.h"

#include "drv_wifi.h"

#include <string.h>

#include "esp_log.h"
#include "esp_console.h"
#include "esp_system.h"

#include "esp_wifi.h"
#include "esp_netif.h"

#if CONFIG_DRV_IPERF_USE
#include "iperf.h"  /* to do create component drv_iperf on base of iperf component in enpoint project */
#endif

#include "argtable3/argtable3.h"

//#include "iperf.h"

/* *****************************************************************************
 * Configuration Definitions
 **************************************************************************** */
#define TAG "cmd_wifi"

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
 * Function-Like Macros
 **************************************************************************** */

/* *****************************************************************************
 * Variables Definitions
 **************************************************************************** */

static struct {
    struct arg_str *device;
    struct arg_str *ip;
    struct arg_str *mask;
    struct arg_str *gw;
    struct arg_str *dhcp;
    struct arg_str *ssid;
    struct arg_str *pass;
    struct arg_end *end;
} wifi_args;

char null_string_wifi[] = "";





#if CONFIG_DRV_IPERF_USE
typedef struct {
    struct arg_str *ip;
    struct arg_lit *server;
    struct arg_lit *udp;
    struct arg_lit *version;
    struct arg_int *port;
    struct arg_int *length;
    struct arg_int *interval;
    struct arg_int *time;
    struct arg_int *bw_limit;
    struct arg_lit *abort;
    struct arg_end *end;
} wifi_iperf_t;
static wifi_iperf_t iperf_args;
#endif

typedef struct {
    struct arg_str *ssid;
    struct arg_str *password;
    struct arg_end *end;
} wifi_args_t;

typedef struct {
    struct arg_str *ssid;
    struct arg_end *end;
} wifi_scan_arg_t;

static wifi_args_t sta_args;
static wifi_scan_arg_t scan_args;
static wifi_args_t ap_args;
static bool reconnect = true;

//static esp_netif_t *netif_ap = NULL;
//static esp_netif_t *netif_sta = NULL;






/* *****************************************************************************
 * Prototype of functions definitions
 **************************************************************************** */

/* *****************************************************************************
 * Functions
 **************************************************************************** */
static int update_wifi(int argc, char **argv)
{
    ESP_LOGI(__func__, "argc=%d", argc);
    for (int i = 0; i < argc; i++)
    {
        ESP_LOGI(__func__, "argv[%d]=%s", i, argv[i]);
    }

    int nerrors = arg_parse(argc, argv, (void **)&wifi_args);
    if (nerrors != ESP_OK)
    {
        arg_print_errors(stderr, wifi_args.end, argv[0]);
        return ESP_FAIL;
    }

    esp_netif_t *netif = NULL;

    const char* device_interface = wifi_args.device->sval[0];
    if (strlen(device_interface) > 0)
    {
        if (strcmp(device_interface,"ap") == 0)
        {
            ESP_LOGI(TAG, "Wifi Soft-AP Interface Selected");
            netif = drv_wifi_get_netif_ap();
        }
        else if (strcmp(device_interface,"sta") == 0)
        {
            ESP_LOGI(TAG, "Wifi Station Interface Selected");
            netif = drv_wifi_get_netif_sta();
        }
        else
        {
            ESP_LOGE(TAG, "Wifi Interface %s Not Implemented", device_interface);

            ESP_LOGW(TAG, "Wifi Station Interface Selected by Default Forced");
            netif = drv_wifi_get_netif_sta();
        }
    }
    else
    {
        ESP_LOGI(TAG, "Wifi Station Interface Selected by Default");
        netif = drv_wifi_get_netif_sta();
    }


    if (netif == NULL)
    {
        ESP_LOGE(TAG, "Wifi Interface Not Implemented");
        netif = drv_wifi_get_netif_sta();
    }
    else
    {
        const char* ip_address = NULL;
        const char* ip_netmask = NULL;
        const char* gw_address = NULL;

        if ((wifi_args.ip->sval[0] != NULL)
            || (wifi_args.mask->sval[0] != NULL)
            || (wifi_args.gw->sval[0] != NULL))
        {
            
            if (ip_netmask != NULL)
            {
                ESP_LOGI(TAG, "0 *ip_netmask %s", ip_netmask);
            } else {
                ESP_LOGI(TAG, "0 ip_netmask NULL");
            }

            //if (netif != NULL)
            {
                ip_address = wifi_args.ip->sval[0];
                ip_netmask = wifi_args.mask->sval[0];

                if (ip_netmask != NULL)
                {
                    ESP_LOGI(TAG, "1 *ip_netmask %s", ip_netmask);
                } else {
                    ESP_LOGI(TAG, "1 ip_netmask NULL");
                }

                gw_address = wifi_args.gw->sval[0];  
            }
            drv_wifi_set_static_ip(netif, ip_address, ip_netmask, gw_address, false);
        }
        if ((wifi_args.ssid->sval[0] != NULL)
            || (wifi_args.pass->sval[0] != NULL))
        {
            drv_wifi_sta_ssid_pass_set((char*)wifi_args.ssid->sval[0], (char*)wifi_args.pass->sval[0], false, NULL);
        }
        if (wifi_args.dhcp->sval[0] != NULL)
        {
            if (strcmp(wifi_args.dhcp->sval[0],"1") == 0)
            {
                drv_wifi_set_dynamic_ip(netif);
            }
        }


    }

    


    return 0;
}








static bool wifi_cmd_sta_join(const char *ssid, const char *pass)
{
    // int bits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, 0, 1, 0);

    wifi_config_t wifi_config = { 0 };

    strlcpy((char *) wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    if (pass) {
        strlcpy((char *) wifi_config.sta.password, pass, sizeof(wifi_config.sta.password));
    }

    drv_wifi_reconnect();


    // // if (bits & CONNECTED_BIT) {
    // //     reconnect = false;
    // //     xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
    //         esp_wifi_disconnect();
    // //     xEventGroupWaitBits(wifi_event_group, DISCONNECTED_BIT, 0, 1, portTICK_PERIOD_MS);
    // // }

    // reconnect = true;
    // ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    // ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    // esp_wifi_connect();

    // //xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, 0, 1, 5000 / portTICK_PERIOD_MS);

    return true;
}

static int wifi_cmd_sta(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &sta_args);

    if (nerrors != 0) {
        arg_print_errors(stderr, sta_args.end, argv[0]);
        return 1;
    }

    ESP_LOGI(TAG, "sta connecting to '%s'", sta_args.ssid->sval[0]);
    wifi_cmd_sta_join(sta_args.ssid->sval[0], sta_args.password->sval[0]);
    return 0;
}

static bool wifi_cmd_sta_scan(const char *ssid)
{
    wifi_scan_config_t scan_config = { 0 };
    scan_config.ssid = (uint8_t *) ssid;

    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    esp_wifi_scan_start(&scan_config, false);

    return true;
}

static int wifi_cmd_scan(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &scan_args);

    if (nerrors != 0) {
        arg_print_errors(stderr, scan_args.end, argv[0]);
        return 1;
    }

    ESP_LOGI(TAG, "sta start to scan");
    if ( scan_args.ssid->count == 1 ) {
        wifi_cmd_sta_scan(scan_args.ssid->sval[0]);
    } else {
        wifi_cmd_sta_scan(NULL);
    }
    return 0;
}


static bool wifi_cmd_ap_set(const char *ssid, const char *pass)
{
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = "",
            .ssid_len = 0,
            .max_connection = 4,
            .password = "",
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };

    reconnect = false;
    strlcpy((char *) wifi_config.ap.ssid, ssid, sizeof(wifi_config.ap.ssid));
    if (pass) {
        if (strlen(pass) != 0 && strlen(pass) < 8) {
            reconnect = true;
            ESP_LOGE(TAG, "password less than 8");
            return false;
        }
        strlcpy((char *) wifi_config.ap.password, pass, sizeof(wifi_config.ap.password));
    }

    if (strlen(pass) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    return true;
}

static int wifi_cmd_ap(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &ap_args);

    if (nerrors != 0) {
        arg_print_errors(stderr, ap_args.end, argv[0]);
        return 1;
    }

    wifi_cmd_ap_set(ap_args.ssid->sval[0], ap_args.password->sval[0]);
    ESP_LOGI(TAG, "AP mode, %s %s", ap_args.ssid->sval[0], ap_args.password->sval[0]);
    return 0;
}

static int wifi_cmd_query(int argc, char **argv)
{
    wifi_config_t cfg;
    wifi_mode_t mode;

    esp_wifi_get_mode(&mode);
    if (WIFI_MODE_AP == mode) 
    {
        esp_wifi_get_config(WIFI_IF_AP, &cfg);
        ESP_LOGI(TAG, "AP mode, %s %s", cfg.ap.ssid, cfg.ap.password);
    } 
    else if (WIFI_MODE_STA == mode) 
    {
        
        if (drv_wifi_get_sta_connected()) 
        {
            esp_wifi_get_config(WIFI_IF_STA, &cfg);
            ESP_LOGI(TAG, "sta mode, connected %s", cfg.ap.ssid);
        } else {
            ESP_LOGI(TAG, "sta mode, disconnected");
        }
    } else {
        ESP_LOGI(TAG, "NULL mode");
        return 0;
    }

    return 0;
}

// static uint32_t wifi_get_local_ip(void)
// {
//     int bits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, 0, 1, 0);
//     esp_netif_t *netif = netif_ap;
//     esp_netif_ip_info_t ip_info;
//     wifi_mode_t mode;

//     esp_wifi_get_mode(&mode);
//     if (WIFI_MODE_STA == mode) {
//         bits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, 0, 1, 0);
//         if (bits & CONNECTED_BIT) {
//             netif = netif_sta;
//         } else {
//             ESP_LOGE(TAG, "sta has no IP");
//             return 0;
//         }
//     }

//     esp_netif_get_ip_info(netif, &ip_info);
//     return ip_info.ip.addr;
// }

#if CONFIG_DRV_IPERF_USE
static int wifi_cmd_iperf(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &iperf_args);
    iperf_cfg_t cfg;

    if (nerrors != 0) {
        arg_print_errors(stderr, iperf_args.end, argv[0]);
        return 0;
    }

    memset(&cfg, 0, sizeof(cfg));

    // now wifi iperf only support IPV4 address
    cfg.type = IPERF_IP_TYPE_IPV4;

    if ( iperf_args.abort->count != 0) {
        iperf_stop();
        return 0;
    }

    if ( ((iperf_args.ip->count == 0) && (iperf_args.server->count == 0)) ||
            ((iperf_args.ip->count != 0) && (iperf_args.server->count != 0)) ) {
        ESP_LOGE(TAG, "should specific client/server mode");
        return 0;
    }

    if (iperf_args.ip->count == 0) {
        cfg.flag |= IPERF_FLAG_SERVER;
    } else {
        cfg.destination_ip4 = esp_ip4addr_aton(iperf_args.ip->sval[0]);
        cfg.flag |= IPERF_FLAG_CLIENT;
    }

    cfg.source_ip4 = drv_wifi_get_ip_sta();
    if (cfg.source_ip4 == 0) {
        return 0;
    }

    if (iperf_args.udp->count == 0) {
        cfg.flag |= IPERF_FLAG_TCP;
    } else {
        cfg.flag |= IPERF_FLAG_UDP;
    }

    if (iperf_args.length->count == 0) {
        cfg.len_send_buf = 0;
    } else {
        cfg.len_send_buf = iperf_args.length->ival[0];
    }

    if (iperf_args.port->count == 0) {
        cfg.sport = IPERF_DEFAULT_PORT;
        cfg.dport = IPERF_DEFAULT_PORT;
    } else {
        if (cfg.flag & IPERF_FLAG_SERVER) {
            cfg.sport = iperf_args.port->ival[0];
            cfg.dport = IPERF_DEFAULT_PORT;
        } else {
            cfg.sport = IPERF_DEFAULT_PORT;
            cfg.dport = iperf_args.port->ival[0];
        }
    }

    if (iperf_args.interval->count == 0) {
        cfg.interval = IPERF_DEFAULT_INTERVAL;
    } else {
        cfg.interval = iperf_args.interval->ival[0];
        if (cfg.interval <= 0) {
            cfg.interval = IPERF_DEFAULT_INTERVAL;
        }
    }

    if (iperf_args.time->count == 0) {
        cfg.time = IPERF_DEFAULT_TIME;
    } else {
        cfg.time = iperf_args.time->ival[0];
        if (cfg.time <= cfg.interval) {
            cfg.time = cfg.interval;
        }
    }
#ifdef IPERF_DEFAULT_NO_BW_LIMIT
    /* iperf -b */
    if (iperf_args.bw_limit->count == 0) {
        cfg.bw_lim = IPERF_DEFAULT_NO_BW_LIMIT;
    } else {
        cfg.bw_lim = iperf_args.bw_limit->ival[0];
        if (cfg.bw_lim <= 0) {
            cfg.bw_lim = IPERF_DEFAULT_NO_BW_LIMIT;
        }
    }
#endif

    ESP_LOGI(TAG, "mode=%s-%s sip=%d.%d.%d.%d:%d, dip=%d.%d.%d.%d:%d, interval=%d, time=%d",
             cfg.flag & IPERF_FLAG_TCP ? "tcp" : "udp",
             cfg.flag & IPERF_FLAG_SERVER ? "server" : "client",
             (int)cfg.source_ip4 & 0xFF, (int)(cfg.source_ip4 >> 8) & 0xFF, (int)(cfg.source_ip4 >> 16) & 0xFF,
             (int)(cfg.source_ip4 >> 24) & 0xFF, (int)cfg.sport,
             (int)cfg.destination_ip4 & 0xFF, (int)(cfg.destination_ip4 >> 8) & 0xFF,
             (int)(cfg.destination_ip4 >> 16) & 0xFF, (int)(cfg.destination_ip4 >> 24) & 0xFF, (int)cfg.dport,
             (int)cfg.interval, (int)cfg.time);

    iperf_start(&cfg);

    return 0;
}
#endif




static void register_wifi(void)
{
    wifi_args.device    = arg_strn("i", "interface","<adapter interface>",  0, 1, "Command can be : wifi [-i {ap|sta}]");
    wifi_args.ip        = arg_strn("a", "address",  "<ip address>",         0, 1, "Command can be : wifi [-a 192.168.0.10]");
    wifi_args.mask      = arg_strn("m", "mask",     "<netmask>",            0, 1, "Command can be : wifi [-m 255.255.255.0]");
    wifi_args.gw        = arg_strn("g", "gateway",  "<gateway>",            0, 1, "Command can be : wifi [-g 192.168.0.1]");
    wifi_args.dhcp      = arg_strn("d", "dhcp",     "<dhcp use>",           0, 1, "Command can be : wifi [-d 1]");
    wifi_args.ssid      = arg_strn("s", "ssid",     "<ssid name>",          0, 1, "Command can be : wifi [-s dlink-4584]");
    wifi_args.pass      = arg_strn("p", "pass",     "<password>",           0, 1, "Command can be : wifi [-p megadeth440426]");
    wifi_args.end       = arg_end(7);

    const esp_console_cmd_t cmd_wifi = {
        .command = "wifi",
        .help = "Wifi Settings Update Request",
        .hint = NULL,
        .func = &update_wifi,
        .argtable = &wifi_args,
    };

    wifi_args.device->sval[0] = null_string_wifi;
    wifi_args.ip->sval[0] = null_string_wifi;
    wifi_args.mask->sval[0] = null_string_wifi;
    wifi_args.gw->sval[0] = null_string_wifi;

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_wifi));




    sta_args.ssid = arg_str1(NULL, NULL, "<ssid>", "SSID of AP");
    sta_args.password = arg_str0(NULL, NULL, "<pass>", "password of AP");
    sta_args.end = arg_end(2);

    const esp_console_cmd_t sta_cmd = {
        .command = "sta",
        .help = "WiFi is station mode, join specified soft-AP",
        .hint = NULL,
        .func = &wifi_cmd_sta,
        .argtable = &sta_args
    };

    ESP_ERROR_CHECK( esp_console_cmd_register(&sta_cmd) );

    scan_args.ssid = arg_str0(NULL, NULL, "<ssid>", "SSID of AP want to be scanned");
    scan_args.end = arg_end(1);

    const esp_console_cmd_t scan_cmd = {
        .command = "scan",
        .help = "WiFi is station mode, start scan ap",
        .hint = NULL,
        .func = &wifi_cmd_scan,
        .argtable = &scan_args
    };

    ap_args.ssid = arg_str1(NULL, NULL, "<ssid>", "SSID of AP");
    ap_args.password = arg_str0(NULL, NULL, "<pass>", "password of AP");
    ap_args.end = arg_end(2);


    ESP_ERROR_CHECK( esp_console_cmd_register(&scan_cmd) );

    const esp_console_cmd_t ap_cmd = {
        .command = "ap",
        .help = "AP mode, configure ssid and password",
        .hint = NULL,
        .func = &wifi_cmd_ap,
        .argtable = &ap_args
    };

    ESP_ERROR_CHECK( esp_console_cmd_register(&ap_cmd) );

    const esp_console_cmd_t query_cmd = {
        .command = "query",
        .help = "query WiFi info",
        .hint = NULL,
        .func = &wifi_cmd_query,
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&query_cmd) );

#if CONFIG_DRV_IPERF_USE
    iperf_args.ip = arg_str0("c", "client", "<ip>", "run in client mode, connecting to <host>");
    iperf_args.server = arg_lit0("s", "server", "run in server mode");
    iperf_args.udp = arg_lit0("u", "udp", "use UDP rather than TCP");
    iperf_args.version = arg_lit0("V", "ipv6_domain", "use IPV6 address rather than IPV4");
    iperf_args.port = arg_int0("p", "port", "<port>", "server port to listen on/connect to");
    iperf_args.length = arg_int0("l", "len", "<length>", "Set read/write buffer size");
    iperf_args.interval = arg_int0("i", "interval", "<interval>", "seconds between periodic bandwidth reports");
    iperf_args.time = arg_int0("t", "time", "<time>", "time in seconds to transmit for (default 10 secs)");
    iperf_args.bw_limit = arg_int0("b", "bandwidth", "<bandwidth>", "bandwidth to send at in Mbits/sec");
    iperf_args.abort = arg_lit0("a", "abort", "abort running iperf");
    iperf_args.end = arg_end(1);
    const esp_console_cmd_t iperf_cmd = {
        .command = "iperf",
        .help = "iperf command",
        .hint = NULL,
        .func = &wifi_cmd_iperf,
        .argtable = &iperf_args
    };

    ESP_ERROR_CHECK( esp_console_cmd_register(&iperf_cmd) );
#endif


}


void cmd_wifi_register(void)
{
    register_wifi();
}
