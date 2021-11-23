/**
 * @file cloud_thingsboard.h
 */

#ifndef PRJ_CLOUD_MODULE
#define PRJ_CLOUD_MODULE

/*! Identifier of the log messages produced by the application */
#define CLOUD_TAG "app_cloud"
#define TB_TAG "tb_ota"

/*! Factory partiton label */
#define FACTORY_PARTITION_LABEL "factory"

#define STATIC_PARAMETER_TITLE_STRING "/firmware?title="
#define STATIC_PARAMETER_VERSION_STRING "&version="

/*! MQTT topic to send a telemetry to ThingsBoard */
#define TB_TELEMETRY_TOPIC "v1/devices/me/telemetry"

/*! MQTT topic to send the shared attributes to ThingsBoard or to receive the shared attributes if they were updated on ThingsBoard */
#define TB_ATTRIBUTES_TOPIC "v1/devices/me/attributes"

/*! MQTT topic to subscribe for the receiving of the specified shared attribute after the request to ThingsBoard */
#define TB_ATTRIBUTES_SUBSCRIBE_TO_RESPONSE_TOPIC "v1/devices/me/attributes/response/+"

/**
 * @brief  MQTT topic to request the specified shared attributes from ThingsBoard.
 *         44332 is a request id, any integer number can be used.
 */
#define TB_ATTRIBUTES_REQUEST_TOPIC "v1/devices/me/attributes/request/44332"

/**
 * @brief  MQTT topic to receive the requested specified shared attributes from ThingsBoard.
 *         44332 is a request id, have to be the same as used for the request.
 */
#define TB_ATTRIBUTES_RESPONSE_TOPIC "v1/devices/me/attributes/response/44332"

/*! Client attribute key to send the firmware version value to ThingsBoard */
#define TB_CLIENT_ATTR_FIELD_CURRENT_FW "currentFwVer"

/*! Shared attribute keys on ThingsBoard */
#define TB_SHARED_ATT_KEY_LENGTH 128
#define TB_SHARED_ATTR_FIELD_FW_TITLE "fw_title"
#define TB_SHARED_ATTR_FIELD_FW_VERSION "fw_version"
#define TB_SHARED_ATTR_FIELD_TARGET_FW_URL "targetFwUrl"
#define TB_SHARED_ATTR_FIELD_TARGET_FW_VER "targetFwVer"

/*! Body of the request of specified shared attributes */
#define TB_SHARED_ATTR_KEYS_REQUEST "{\"sharedKeys\":\"fw_title,fw_version,targetFwUrl,targetFwVer\"}"

/*! Max length of access token */
#define MAX_LENGTH_TB_ACCESS_TOKEN 20
#define MAX_LENGTH_TB_URL 256

/*! NVS storage key where the MQTT broker URL is saved */
#define NVS_KEY_MQTT_URL "mqtt_url"

/*! NVS storage key where the MQTT broker port is saved */
#define NVS_KEY_MQTT_PORT "mqtt_port"

/*! NVS storage key where the MQTT access token is saved */
#define NVS_KEY_MQTT_ACCESS_TOKEN "access_token"

/**
 * @brief Set of states for @ref ota_task(void)
 */
enum state
{
    STATE_INITIAL,
    STATE_WAIT_WIFI,
    STATE_WIFI_CONNECTED,
    STATE_WAIT_MQTT,
    STATE_MQTT_CONNECTED,
    STATE_WAIT_OTA_CONFIG_FETCHED,
    STATE_OTA_CONFIG_FETCHED,
    STATE_APP_LOOP,

    STATE_CONNECTION_IS_OK
};

void cloud_start(void);

/*! Updates application event bits on changing Wi-Fi state */
void notify_wifi_connected();
void notify_wifi_disconnected();

/**
 * Macro to check the error code.
 * Prints the error code, error location, and the failed statement to serial output.
 * Unlike to ESP_ERROR_CHECK() method this macros abort the application's execution if it was built as 'release'.
 */
#define APP_ABORT_ON_ERROR(x)                                     \
    do                                                            \
    {                                                             \
        esp_err_t __err_rc = (x);                                 \
        if (__err_rc != ESP_OK)                                   \
        {                                                         \
            _esp_error_check_failed(__err_rc, __FILE__, __LINE__, \
                                    __ASSERT_FUNC, #x);           \
        }                                                         \
    } while (0);

#endif /* PRJ_CLOUD_MODULE */
