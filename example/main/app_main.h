/**
 * @file main.h
 */

#ifndef PRJ_MAIN_MODULE
#define PRJ_MAIN_MODULE

#include "esp_log.h"
#include "string.h"

/*! Firmware version used for comparison after OTA config was received from ThingsBoard */
#define FIRMWARE_VERSION "v1.0"

/*! Identifier of the log messages produced by the application */
#define APP_TAG "app_main"

/**
 * @brief Bit set for application events
 */
#define WIFI_CONNECTED_EVENT BIT0
#define WIFI_FAIL_BIT BIT1
#define WIFI_DISCONNECTED_EVENT BIT2
#define MQTT_CONNECTED_EVENT BIT3
#define MQTT_DISCONNECTED_EVENT BIT4
#define OTA_CONFIG_FETCHED_EVENT BIT5
#define OTA_CONFIG_UPDATED_EVENT BIT6
#define OTA_TASK_IN_NORMAL_STATE_EVENT BIT7

#endif /* PRJ_MAIN_MODULE */
