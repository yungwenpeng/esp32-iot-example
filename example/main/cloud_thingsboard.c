#include "app_main.h"
#include "cloud_thingsboard.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_event.h"
#include "stdio.h"
#include "cJSON.h"
#include "mqtt_client.h"
#include "dht.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_timer.h"

#include "nvs.h"
#include "nvs_flash.h"
#include "sys/param.h"

#define DHT11_PIN 4

static bool mqtt_connected = false;
esp_mqtt_client_handle_t client;
/*! Saves bit values used in application */
static EventGroupHandle_t event_group;

// Period of sending a temperature/humidity data.
static int send_delay = 5000;

// For HTTP Transport over SSL
extern const uint8_t server_cert_pem_start[] asm("_binary_ca_cert_pem_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_ca_cert_pem_end");
// For MQTT Transport over SSL
extern const uint8_t mqtt_server_cert_pem_start[] asm("_binary_tb_mqtt_server_cert_pem_start");  // Access Token Authentication for mqtts & X.509 Certificate Authentication
extern const uint8_t mqtt_server_cert_pem_end[] asm("_binary_tb_mqtt_server_cert_pem_end");
extern const uint8_t mqtt_client_cert_pem_start[] asm("_binary_tb_mqtt_client_cert_pem_start");  // X.509 Certificate Authentication
extern const uint8_t mqtt_client_cert_pem_end[] asm("_binary_tb_mqtt_client_cert_pem_end");
extern const uint8_t mqtt_client_key_pem_start[] asm("_binary_tb_mqtt_client_key_pem_start");    // X.509 Certificate Authentication
extern const uint8_t mqtt_client_key_pem_end[] asm("_binary_tb_mqtt_client_key_pem_end");

/*! Saves OTA config received from ThingsBoard*/
static struct shared_keys
{
    char fw_title[TB_SHARED_ATT_KEY_LENGTH];
    char fw_version[TB_SHARED_ATT_KEY_LENGTH];
    char targetFwServerUrl[TB_SHARED_ATT_KEY_LENGTH*2];
    char targetFwVer[TB_SHARED_ATT_KEY_LENGTH];
} shared_attributes;

/*! Buffer to save a received MQTT message */
static char mqtt_msg[TB_SHARED_ATT_KEY_LENGTH*4];


static void parse_ota_config(const cJSON *object)
{
    if (object != NULL)
    {
        cJSON *fw_title_response = cJSON_GetObjectItem(object, TB_SHARED_ATTR_FIELD_FW_TITLE);
        if (cJSON_IsString(fw_title_response) && (fw_title_response->valuestring != NULL) && strlen(fw_title_response->valuestring) < sizeof(shared_attributes.fw_title))
        {
            memcpy(shared_attributes.fw_title, fw_title_response->valuestring, strlen(fw_title_response->valuestring));
            shared_attributes.fw_title[sizeof(shared_attributes.fw_title) - 1] = 0;
            ESP_LOGI(TB_TAG, "Received shared attributes fw_title: %s", shared_attributes.fw_title);
        }

        cJSON *fw_version_response = cJSON_GetObjectItem(object, TB_SHARED_ATTR_FIELD_FW_VERSION);
        if (cJSON_IsString(fw_version_response) && (fw_version_response->valuestring != NULL) && strlen(fw_version_response->valuestring) < sizeof(shared_attributes.fw_version))
        {
            memcpy(shared_attributes.fw_version, fw_version_response->valuestring, strlen(fw_version_response->valuestring));
            shared_attributes.fw_version[sizeof(shared_attributes.fw_version) - 1] = 0;
            ESP_LOGI(TB_TAG, "Received shared attributes fw_version: %s", shared_attributes.fw_version);
        }
    }
}

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(CLOUD_TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(CLOUD_TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(CLOUD_TAG, "MQTT_EVENT_CONNECTED");
        xEventGroupSetBits(event_group, MQTT_CONNECTED_EVENT);
        mqtt_connected = true;
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(CLOUD_TAG, "MQTT_EVENT_DISCONNECTED");
        xEventGroupClearBits(event_group, MQTT_CONNECTED_EVENT);
        mqtt_connected = false;
        break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(CLOUD_TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(CLOUD_TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(CLOUD_TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(CLOUD_TAG, "MQTT_EVENT_DATA, msg_id=%d, event: %s", event->msg_id, event->topic);
            if (event->data_len >= (sizeof(mqtt_msg) - 1))
            {
                ESP_LOGE(TB_TAG, "Received MQTT message size [%d] more than expected [%d]", event->data_len, (sizeof(mqtt_msg) - 1));
            }

            if (strcmp(TB_ATTRIBUTES_RESPONSE_TOPIC, event->topic) == 0)
            {
                memcpy(mqtt_msg, event->data, event->data_len);
                mqtt_msg[event->data_len] = 0;
                cJSON *attributes = cJSON_Parse(mqtt_msg);
                if (attributes != NULL)
                {
                    cJSON *shared = cJSON_GetObjectItem(attributes, "shared");
                    parse_ota_config(shared);
                }

                char *attributes_string = cJSON_Print(attributes);
                cJSON_Delete(attributes);
                ESP_LOGD(TB_TAG, "Shared attributes response: %s", attributes_string);
                // Free is intentional, it's client responsibility to free the result of cJSON_Print
                free(attributes_string);
                xEventGroupSetBits(event_group, OTA_CONFIG_FETCHED_EVENT);
            }
            else if (strcmp(TB_ATTRIBUTES_TOPIC, event->topic) == 0)
            {
                memcpy(mqtt_msg, event->data, MIN(event->data_len, sizeof(mqtt_msg)));
                mqtt_msg[event->data_len] = 0;
                cJSON *attributes = cJSON_Parse(mqtt_msg);
                parse_ota_config(attributes);
                char *attributes_string = cJSON_Print(attributes);
                cJSON_Delete(attributes);
                ESP_LOGD(TB_TAG, "Shared attributes were updated on ThingsBoard: %s", attributes_string);
                // Free is intentional, it's client responsibility to free the result of cJSON_Print
                free(attributes_string);
                xEventGroupSetBits(event_group, OTA_CONFIG_UPDATED_EVENT);
            }
            break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(CLOUD_TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(CLOUD_TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        xEventGroupClearBits(event_group, MQTT_CONNECTED_EVENT);
        break;
    default:
        ESP_LOGI(CLOUD_TAG, "Other event id:%d", event->event_id);
        break;
    }
}


esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    assert(evt != NULL);

    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TB_TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TB_TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TB_TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TB_TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TB_TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        if (!esp_http_client_is_chunked_response(evt->client))
        {
            // Write out data
            ESP_LOGD(TB_TAG, "%.*s", evt->data_len, (char *)evt->data);
        }
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TB_TAG, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(TB_TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    }
    return ESP_OK;
}

/**
 * @brief Main application task, it sends counter value to ThingsBoard telemetry MQTT topic.
 *
 * @param pvParameters Pointer to the task arguments
 */
static void main_application_task(void *pvParameters)
{
    int16_t temperature;
    int16_t humidity;
	static char post_data[80];

    while (1) {
    	if(mqtt_connected)
    	{
    		esp_err_t ret = dht_read_data(DHT_TYPE_DHT11, (gpio_num_t)DHT11_PIN, &humidity, &temperature);
    		if(mqtt_connected && ret == ESP_OK)
    		{
    			ESP_LOGI(CLOUD_TAG, "Humidity: %d%% Temperature: %dC\n", humidity / 10, temperature / 10);
    			sprintf(post_data, "{\"temperature\":%f,\"humidity\":%f}", temperature / 10.0, humidity / 10.0);
    			esp_mqtt_client_publish(client, "v1/devices/me/telemetry", post_data, 0, 1, 0);
    		}
    		else
    		{
    			ESP_LOGE(CLOUD_TAG, "Could not read data from sensor");
    		}
    	}
        vTaskDelay(send_delay / portTICK_PERIOD_MS);
    }
}

/**
 * @brief Check is the current running partition label is factory
 *
 * @param running_partition_label Current running partition label
 * @param config_name Configuration name that specified in Kconfig, ThingsBoard OTA configuration submenu
 * @return true If current running partition label is 'factory'
 * @return false If current runnit partition label is not 'factory'
 */
static bool partition_is_factory(const char *running_partition_label, const char *config_name)
{
    if (strcmp(FACTORY_PARTITION_LABEL, running_partition_label) == 0)
    {
        ESP_LOGW(TB_TAG, "Factory partition is running. %s from config is saving to the flash memory", config_name);
        return true;
    }
    else
    {
        ESP_LOGE(TB_TAG, "%s wasn't found, running partition is not '%s'", config_name, FACTORY_PARTITION_LABEL);
        APP_ABORT_ON_ERROR(ESP_FAIL);
        return false;
    }
}

/**
 * @brief Get MQTT broker URL to use in MQTT client.
 *        If the flash memory is empty and running partition is 'factory'
 *        then MQTT broker URL specified in ThingsBoard OTA configuration submenu will be saved to NVS.
 *        If running partition is not 'factory' ('ota_0' or 'ota_1') then MQTT broker URL from NVS is used.
 *        The application stops if running partition is not 'factory' and MQTT broker URL was not found in NVS.
 *
 * @param running_partition_label Current running partition label
 * @return const char* MQTT broker URL
 */
static const char *get_mqtt_url(const char *running_partition_label)
{
    nvs_handle handle;
    APP_ABORT_ON_ERROR(nvs_open(NVS_KEY_MQTT_URL, NVS_READWRITE, &handle));

    static char mqtt_url[MAX_LENGTH_TB_URL + 1];
    size_t mqtt_url_len = sizeof(mqtt_url);

    esp_err_t result_code = nvs_get_str(handle, NVS_KEY_MQTT_URL, mqtt_url, &mqtt_url_len);
    if (result_code == ESP_OK)
    {
        ESP_LOGI(TB_TAG, "MQTT URL from flash memory: %s", mqtt_url);
    }
    else if (result_code == ESP_ERR_NVS_NOT_FOUND)
    {
        if (partition_is_factory(running_partition_label, "MQTT URL"))
        {
            APP_ABORT_ON_ERROR(nvs_set_str(handle, NVS_KEY_MQTT_URL, CONFIG_MQTT_BROKER_URL));
            APP_ABORT_ON_ERROR(nvs_commit(handle));
            APP_ABORT_ON_ERROR(nvs_get_str(handle, NVS_KEY_MQTT_URL, mqtt_url, &mqtt_url_len));
        }
    }
    else
    {
        ESP_LOGE(TB_TAG, "Unable to to get MQTT URL from NVS");
        APP_ABORT_ON_ERROR(ESP_FAIL);
    }

    nvs_close(handle);
    return mqtt_url;
}

/**
 * @brief Get MQTT broker port to use in MQTT client.
 *        If the flash memory is empty and running partition is 'factory'
 *        then MQTT broker port specified in ThingsBoard OTA configuration submenu will be saved to NVS.
 *        If running partition is not 'factory' ('ota_0' or 'ota_1') then MQTT broker port from NVS is used.
 *        The application stops if running partition is not 'factory' and MQTT broker port was not found in NVS.
 *
 * @param running_partition_label Current running partition label
 * @return const char* MQTT broker port
 */
static uint32_t get_mqtt_port(const char *running_partition_label)
{
    nvs_handle handle;
    APP_ABORT_ON_ERROR(nvs_open(NVS_KEY_MQTT_PORT, NVS_READWRITE, &handle));

    uint32_t mqtt_port;

    esp_err_t result_code = nvs_get_u32(handle, NVS_KEY_MQTT_PORT, &mqtt_port);
    if (result_code == ESP_OK)
    {
        ESP_LOGI(TB_TAG, "MQTT port from flash memory: %d", mqtt_port);
    }
    else if (result_code == ESP_ERR_NVS_NOT_FOUND)
    {
        if (partition_is_factory(running_partition_label, "MQTT port"))
        {
            APP_ABORT_ON_ERROR(nvs_set_u32(handle, NVS_KEY_MQTT_PORT, CONFIG_MQTT_BROKER_PORT));
            APP_ABORT_ON_ERROR(nvs_commit(handle));
            APP_ABORT_ON_ERROR(nvs_get_u32(handle, NVS_KEY_MQTT_PORT, &mqtt_port));
        }
    }
    else
    {
        ESP_LOGE(TB_TAG, "Unable to to get MQTT port from NVS");
        APP_ABORT_ON_ERROR(ESP_FAIL);
    }

    nvs_close(handle);
    return mqtt_port;
}

/**
 * @brief Get MQTT access token to use in MQTT client.
 *        If the flash memory is empty and running partition is 'factory'
 *        then MQTT broker access token specified in ThingsBoard OTA configuration submenu will be saved to NVS.
 *        If running partition is not 'factory' ('ota_0' or 'ota_1') then MQTT broker access token from NVS is used.
 *        The application stops if running partition is not 'factory' and MQTT broker access token was not found in NVS.
 *
 * @param running_partition_label Current running partition label
 * @return const char* MQTT broker access token
 */
static const char *get_mqtt_access_token(const char *running_partition_label)
{
    nvs_handle handle;
    APP_ABORT_ON_ERROR(nvs_open(NVS_KEY_MQTT_ACCESS_TOKEN, NVS_READWRITE, &handle));

    static char access_token[MAX_LENGTH_TB_ACCESS_TOKEN + 1];
    size_t access_token_len = sizeof(access_token);

    esp_err_t result_code = nvs_get_str(handle, NVS_KEY_MQTT_ACCESS_TOKEN, access_token, &access_token_len);
    if (result_code == ESP_OK)
    {
        ESP_LOGI(TB_TAG, "MQTT access token from flash memory: %s", access_token);
    }
    else if (result_code == ESP_ERR_NVS_NOT_FOUND)
    {
        if (partition_is_factory(running_partition_label, "MQTT access token"))
        {
            APP_ABORT_ON_ERROR(nvs_set_str(handle, NVS_KEY_MQTT_ACCESS_TOKEN, CONFIG_MQTT_ACCESS_TOKEN));
            APP_ABORT_ON_ERROR(nvs_commit(handle));
            APP_ABORT_ON_ERROR(nvs_get_str(handle, NVS_KEY_MQTT_ACCESS_TOKEN, access_token, &access_token_len));
        }
    }
    else
    {
        ESP_LOGE(TB_TAG, "Unable to to get MQTT access token from NVS");
        APP_ABORT_ON_ERROR(ESP_FAIL);
    }

    nvs_close(handle);
    return access_token;
}


static bool fw_versions_are_equal(const char *current_ver, const char *target_ver)
{
    assert(current_ver != NULL && target_ver != NULL);

    if (strcmp(current_ver, target_ver) == 0)
    {
        ESP_LOGW(TB_TAG, "Skipping OTA, firmware versions are equal - current: %s, target: %s", FIRMWARE_VERSION, shared_attributes.targetFwVer);
        return true;
    }
    return false;
}


static void start_ota(const char *current_ver, struct shared_keys ota_config)
{
    assert(current_ver != NULL);
	ESP_LOGW(TB_TAG, "start_ota, current_ver: %s, target: %s, fw_version:%s", current_ver, ota_config.targetFwVer, ota_config.fw_version);

	if (!fw_versions_are_equal(current_ver, ota_config.fw_version) && strlen(ota_config.fw_version) != 0)
    {
        ESP_LOGW(TB_TAG, "Starting OTA, firmware versions are different - current: %s, fw_version: %s", current_ver, ota_config.fw_version);
        ESP_LOGI(TB_TAG, "Shared attributes fw_title: %s, fw_version: %s", ota_config.fw_title, ota_config.fw_version);
        char sharedAttributesApiFwUrl[TB_SHARED_ATT_KEY_LENGTH*2] = { '\0' };
        memcpy(sharedAttributesApiFwUrl, CONFIG_HTTPS_BROKER_URL "/api/v1/" CONFIG_MQTT_ACCESS_TOKEN STATIC_PARAMETER_TITLE_STRING, strlen(CONFIG_HTTPS_BROKER_URL "/api/v1/" CONFIG_MQTT_ACCESS_TOKEN STATIC_PARAMETER_TITLE_STRING));
        strncat(sharedAttributesApiFwUrl, ota_config.fw_title, strlen(ota_config.fw_title));
        strcat(sharedAttributesApiFwUrl, STATIC_PARAMETER_VERSION_STRING);
        strncat(sharedAttributesApiFwUrl, ota_config.fw_version, strlen(ota_config.fw_version));
        ESP_LOGI(TB_TAG, "Firmware URL: %s", sharedAttributesApiFwUrl);
        esp_http_client_config_t config = {
            .url = sharedAttributesApiFwUrl,
            .cert_pem = (char *)server_cert_pem_start,
            .event_handler = _http_event_handler,
        };
        esp_err_t ret = esp_https_ota(&config);
        if (ret == ESP_OK)
        {
            esp_restart();
        }
        else
        {
            ESP_LOGE(TB_TAG, "Firmware Upgrades Failed");
        }
    }
}

static enum state connection_state(BaseType_t actual_event, const char *current_state_name)
{
    assert(current_state_name != NULL);

    if (actual_event & WIFI_DISCONNECTED_EVENT)
    {
        ESP_LOGE(TB_TAG, "%s state, Wi-Fi not connected, wait for the connect", current_state_name);
        return STATE_WAIT_WIFI;
    }

    if (actual_event & MQTT_DISCONNECTED_EVENT)
    {
        ESP_LOGW(TB_TAG, "%s state, MQTT not connected, wait for the connect", current_state_name);
        return STATE_WAIT_MQTT;
    }

    return STATE_CONNECTION_IS_OK;
}

/**
 * @brief OTA task, it handles the shared attributes updates and starts OTA if the config received from ThingsBoard is valid.
 *
 * @param pvParameters Pointer to the task arguments
 */
static void ota_task(void *pvParameters)
{
    enum state current_connection_state = STATE_CONNECTION_IS_OK;
    enum state state = STATE_INITIAL;
    BaseType_t ota_events;
    BaseType_t actual_event = 0x00;
    char running_partition_label[sizeof(((esp_partition_t *)0)->label)];

    while (1)
    {
        ESP_LOGI(TB_TAG, "ota_task -> state: %d", state);
        if (state != STATE_INITIAL && state != STATE_APP_LOOP)
        {
            if (state != STATE_APP_LOOP)
            {
                xEventGroupClearBits(event_group, OTA_TASK_IN_NORMAL_STATE_EVENT);
            }

            actual_event = xEventGroupWaitBits(event_group,
                                               WIFI_CONNECTED_EVENT | WIFI_DISCONNECTED_EVENT | MQTT_CONNECTED_EVENT | MQTT_DISCONNECTED_EVENT | OTA_CONFIG_FETCHED_EVENT,
                                               false, false, portMAX_DELAY);
        }

        switch (state)
        {
        case STATE_INITIAL:
        {
            // Initialize NVS.
            esp_err_t err = nvs_flash_init();
            if (err == ESP_ERR_NVS_NO_FREE_PAGES)
            {
                // OTA app partition table has a smaller NVS partition size than the non-OTA
                // partition table. This size mismatch may cause NVS initialization to fail.
                // If this happens, we erase NVS partition and initialize NVS again.
                APP_ABORT_ON_ERROR(nvs_flash_erase());
                err = nvs_flash_init();
            }
            APP_ABORT_ON_ERROR(err);

            const esp_partition_t *running_partition = esp_ota_get_running_partition();
            strncpy(running_partition_label, running_partition->label, sizeof(running_partition_label));
            ESP_LOGI(TB_TAG, "Running partition: %s", running_partition_label);

            state = STATE_WAIT_WIFI;
            break;
        }
        case STATE_WAIT_WIFI:
        {
            if (actual_event & WIFI_DISCONNECTED_EVENT)
            {
                ESP_LOGW(TB_TAG, "WAIT_WIFI state, Wi-Fi not connected, wait for the connect");
                state = STATE_WAIT_WIFI;
                break;
            }

            if (actual_event & WIFI_CONNECTED_EVENT)
            {
                //char *mqtt_url = get_mqtt_url(running_partition_label);
                //uint32_t mqtt_port = get_mqtt_port(running_partition_label);
                //char *mqtt_access_token = get_mqtt_access_token(running_partition_label);
                ESP_LOGD(TB_TAG, "Received MQTT broker"
                     "\n\tURL : %s\n\tPort : %d", get_mqtt_url(running_partition_label), get_mqtt_port(running_partition_label));
                state = STATE_WAIT_MQTT;
                break;
            }

            ESP_LOGW(TB_TAG, "WAIT_WIFI state, unexpected event received: %d", actual_event);
            state = STATE_INITIAL;
            break;
        }
        case STATE_WAIT_MQTT:
        {
            current_connection_state = connection_state(actual_event, "WAIT_MQTT");
            if (current_connection_state != STATE_CONNECTION_IS_OK)
            {
                state = current_connection_state;
                break;
            }

            if (actual_event & (WIFI_CONNECTED_EVENT | MQTT_CONNECTED_EVENT))
            {
                ESP_LOGI(TB_TAG, "Connected to MQTT broker %s, on port %d", CONFIG_MQTT_BROKER_URL, CONFIG_MQTT_BROKER_PORT);

                // Send the current firmware version to ThingsBoard
                cJSON *current_fw = cJSON_CreateObject();
                cJSON_AddStringToObject(current_fw, TB_CLIENT_ATTR_FIELD_CURRENT_FW, FIRMWARE_VERSION);
                char *current_fw_attribute = cJSON_PrintUnformatted(current_fw);
                cJSON_Delete(current_fw);
				ESP_LOGI(TB_TAG, "current_fw_attribute: %s", current_fw_attribute);
                esp_mqtt_client_publish(client, TB_ATTRIBUTES_TOPIC, current_fw_attribute, 0, 1, 0);
                // Free is intentional, it's client responsibility to free the result of cJSON_Print
                free(current_fw_attribute);

                // Send the shared attributes keys to receive their values
                esp_mqtt_client_subscribe(client, TB_ATTRIBUTES_SUBSCRIBE_TO_RESPONSE_TOPIC, 1);
                esp_mqtt_client_publish(client, TB_ATTRIBUTES_REQUEST_TOPIC, TB_SHARED_ATTR_KEYS_REQUEST, 0, 1, 0);
                ESP_LOGI(TB_TAG, "Waiting for shared attributes response");

                state = STATE_WAIT_OTA_CONFIG_FETCHED;
                break;
            }

            ESP_LOGW(TB_TAG, "WAIT_MQTT state, unexpected event received: %d", actual_event);
            state = STATE_INITIAL;
            break;
        }
        case STATE_WAIT_OTA_CONFIG_FETCHED:
        {
            current_connection_state = connection_state(actual_event, "WAIT_OTA_CONFIG_FETCHED");
            if (current_connection_state != STATE_CONNECTION_IS_OK)
            {
                state = current_connection_state;
                break;
            }

            if (actual_event & (WIFI_CONNECTED_EVENT | MQTT_CONNECTED_EVENT))
            {
                if (actual_event & OTA_CONFIG_FETCHED_EVENT)
                {
                    ESP_LOGI(TB_TAG, "Shared attributes were fetched from ThingsBoard");
                    xEventGroupClearBits(event_group, OTA_CONFIG_FETCHED_EVENT);
                    state = STATE_OTA_CONFIG_FETCHED;
                    break;
                }

                state = STATE_WAIT_OTA_CONFIG_FETCHED;
                break;
            }

            ESP_LOGW(TB_TAG, "WAIT_OTA_CONFIG_FETCHED state, unexpected event received: %d", actual_event);
            state = STATE_INITIAL;
            break;
        }
        case STATE_OTA_CONFIG_FETCHED:
        {
            current_connection_state = connection_state(actual_event, "OTA_CONFIG_FETCHED");
            if (current_connection_state != STATE_CONNECTION_IS_OK)
            {
                state = current_connection_state;
                break;
            }

            if (actual_event & (WIFI_CONNECTED_EVENT | MQTT_CONNECTED_EVENT))
            {

                start_ota(FIRMWARE_VERSION, shared_attributes);
                esp_mqtt_client_subscribe(client, TB_ATTRIBUTES_TOPIC, 1);
                ESP_LOGI(TB_TAG, "Subscribed to shared attributes updates");
                state = STATE_APP_LOOP;
                break;
            }
            ESP_LOGW(TB_TAG, "OTA_CONFIG_FETCHED state, unexpected event received: %d", actual_event);
            state = STATE_INITIAL;
            break;
        }
        case STATE_APP_LOOP:
        {
            current_connection_state = connection_state(actual_event, "APP_LOOP");
            if (current_connection_state != STATE_CONNECTION_IS_OK)
            {
                state = current_connection_state;
                break;
            }

            if (actual_event & (WIFI_CONNECTED_EVENT | MQTT_CONNECTED_EVENT))
            {
                ota_events = xEventGroupWaitBits(event_group, OTA_CONFIG_UPDATED_EVENT, false, true, 0);
                if ((ota_events & OTA_CONFIG_UPDATED_EVENT))
                {
                    start_ota(FIRMWARE_VERSION, shared_attributes);
                }
                xEventGroupClearBits(event_group, OTA_CONFIG_UPDATED_EVENT);
                xEventGroupSetBits(event_group, OTA_TASK_IN_NORMAL_STATE_EVENT);
                state = STATE_APP_LOOP;
                break;
            }

            ESP_LOGW(TB_TAG, "APP_LOOP state, unexpected event received: %d", actual_event);
            state = STATE_INITIAL;
            break;
        }
        default:
        {
            ESP_LOGE(TB_TAG, "Unexpected state");
            state = STATE_INITIAL;
            break;
        }
        }  /* switch end */

        vTaskDelay(1200 / portTICK_PERIOD_MS);
    }   /* while end */

}

void cloud_start(void)
{
    ESP_LOGE(CLOUD_TAG, "Starting cloud ...");

	event_group = xEventGroupCreate();
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = CONFIG_MQTT_BROKER_URL,
		.port = CONFIG_MQTT_BROKER_PORT,
		//.client_cert_pem = (char *)mqtt_client_cert_pem_start,  // X.509 Certificate Authentication
		//.client_key_pem = (char *)mqtt_client_key_pem_start,    // X.509 Certificate Authentication
		.cert_pem = (char *)mqtt_server_cert_pem_start,   // Access Token Authentication for mqtts & X.509 Certificate Authentication
		.username = CONFIG_MQTT_ACCESS_TOKEN,
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
    xEventGroupWaitBits(event_group, MQTT_CONNECTED_EVENT, false, true, portMAX_DELAY);
	
	xTaskCreate(&ota_task, "ota_task", 8192, NULL, 5, NULL);
    xTaskCreate(&main_application_task, "main_application_task", 8192, NULL, 5, NULL);
}

void notify_wifi_connected()
{
    xEventGroupClearBits(event_group, WIFI_DISCONNECTED_EVENT);
    xEventGroupSetBits(event_group, WIFI_CONNECTED_EVENT);
}

void notify_wifi_disconnected()
{
    xEventGroupClearBits(event_group, WIFI_CONNECTED_EVENT);
    xEventGroupSetBits(event_group, WIFI_DISCONNECTED_EVENT);
}
