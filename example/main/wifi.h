/**
 * @file wifi.h
 */

#ifndef PRJ_WIFI_MODULE
#define PRJ_WIFI_MODULE

/*! Identifier of the log messages produced by the application */
#define WIFI_TAG "app_wifi"

#define SERV_NAME_PREFIX    "PROV_"

/**
 * @brief Bit set for application events
 */
#define WIFI_CONNECTED_EVENT BIT0
#define WIFI_FAIL_BIT BIT1

void initialise_wifi(void);

#endif /* PRJ_WIFI_MODULE */
