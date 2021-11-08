#include "app_main.h"
#include "cloud_thingsboard.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_event.h"
#include "stdio.h"
#include "mqtt_client.h"
#include "dht.h"

#define DHT11_PIN 4

static bool mqtt_connected = false;
esp_mqtt_client_handle_t client;
static EventGroupHandle_t mqtt_event_group;

// Period of sending a temperature/humidity data.
static int send_delay = 2000;

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
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(CLOUD_TAG, "MQTT_EVENT_CONNECTED");
        xEventGroupSetBits(mqtt_event_group, MQTT_CONNECTED_BIT);
        mqtt_connected = true;
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(CLOUD_TAG, "MQTT_EVENT_DISCONNECTED");
        xEventGroupClearBits(mqtt_event_group, MQTT_CONNECTED_BIT);
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
            ESP_LOGI(CLOUD_TAG, "MQTT_EVENT_DATA");
            break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(CLOUD_TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(CLOUD_TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        xEventGroupClearBits(mqtt_event_group, MQTT_CONNECTED_BIT);
        break;
    default:
        ESP_LOGI(CLOUD_TAG, "Other event id:%d", event->event_id);
        break;
    }
}

void cloud_start(void)
{
    ESP_LOGE(CLOUD_TAG, "Starting cloud ...");

	mqtt_event_group = xEventGroupCreate();
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = CONFIG_MQTT_BROKER_URL,
		.port = CONFIG_MQTT_BROKER_PORT,
		.username = CONFIG_MQTT_ACCESS_TOKEN,
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
    xEventGroupWaitBits(mqtt_event_group, MQTT_CONNECTED_BIT, false, true, portMAX_DELAY);

    int16_t temperature;
    int16_t humidity;
	char post_data[80];

    while (1) {
    	if(mqtt_connected)
    	{
    		esp_err_t ret = dht_read_data(DHT_TYPE_DHT11, (gpio_num_t)DHT11_PIN, &humidity, &temperature);
    		if(mqtt_connected && ret == ESP_OK)
    		{
    			ESP_LOGI(CLOUD_TAG, "Humidity: %d%% Temperature: %dC\n", humidity / 10, temperature / 10);
    			sprintf(post_data, "{\"temperature\":%f,\"humidity\":%f}", temperature / 10.0, humidity / 10.0);
    			int msg_id = esp_mqtt_client_publish(client, "v1/devices/me/telemetry", post_data, 0, 1, 0);
    			ESP_LOGI(CLOUD_TAG, "sent publish successful, msg_id=%d", msg_id);
    		}
    		else
    		{
    			ESP_LOGE(CLOUD_TAG, "Could not read data from sensor");
    		}
    	}
        vTaskDelay(send_delay / portTICK_PERIOD_MS);
    }
}
