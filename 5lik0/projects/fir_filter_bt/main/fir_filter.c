/* FIR Filter

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "audio_pipeline.h"
#include "i2s_stream.h"
#include "es8388.h"
#include "esp_peripherals.h"
#include "periph_button.h"
#include "bluetooth_service.h"

#include "board.h"
#include "filter.h"
#include "static.h"

static const char *TAG = "ESP_BOARD";

static void bt_app_avrc_ct_cb(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *p_param)
{
    esp_avrc_ct_cb_param_t *rc = p_param;
    switch (event) {
        case ESP_AVRC_CT_METADATA_RSP_EVT: {
            uint8_t *tmp = audio_calloc(1, rc->meta_rsp.attr_length + 1);
            memcpy(tmp, rc->meta_rsp.attr_text, rc->meta_rsp.attr_length);
            ESP_LOGI(TAG, "AVRC metadata rsp: attribute id 0x%x, %s", rc->meta_rsp.attr_id, tmp);
            audio_free(tmp);
            break;
        }
        default:
            break;
    }
}

void app_main(void)
{
    audio_pipeline_handle_t pipeline;

    audio_element_handle_t bt_stream_reader;
    audio_element_handle_t i2s_stream_reader;
    audio_element_handle_t fir_filter_el;
    int player_volume;

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set(FIRTAG, ESP_LOG_DEBUG);

    ESP_LOGI(TAG, "[ 1 ] Create Bluetooth service");
    bluetooth_service_cfg_t bt_cfg = {
        .device_name = "ESP-ADF-SPEAKER",
        .mode = BLUETOOTH_A2DP_SINK,
        .user_callback.user_avrc_ct_cb = bt_app_avrc_ct_cb,
    };
    bluetooth_service_start(&bt_cfg);

    ESP_LOGI(FIRTAG, "[ 2 ] Start codec chip");
    audio_board_handle_t board_handle = audio_board_init();

    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_DECODE, AUDIO_HAL_CTRL_START); 
    // es8388_config_adc_input(ADC_INPUT_LINPUT2_RINPUT2);
    // es8388_write_reg(ES8388_ADCCONTROL10, 0x00); // turn off ALC
    // es8388_write_reg(ES8388_ADCCONTROL14, 0b11111011); // noise gate

    audio_hal_get_volume(board_handle->audio_hal, &player_volume);


    ESP_LOGI(FIRTAG, "[ 2 ] Create audio pipeline for playback");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);

    ESP_LOGI(FIRTAG, "[3.1] Create i2s stream to read data from codec chip");
    i2s_stream_cfg_t i2s_cfg_read = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg_read.type = AUDIO_STREAM_READER;
    i2s_stream_reader = i2s_stream_init_driver(&i2s_cfg_read, false);

    ESP_LOGI(FIRTAG, "[3.2] Create i2s stream to write data to codec chip");
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    // Create the writer stream element reusing the i2s driver of the reader
    i2s_stream_writer = i2s_stream_init_driver(&i2s_cfg, true);

    ESP_LOGI(FIRTAG, "[3.3] Create FIR Filter Element");
    audio_element_cfg_t fir_cfg = DEFAULT_AUDIO_ELEMENT_CONFIG();
    // Set filter callback functions
    fir_cfg.open = filter_open;
    fir_cfg.process = filter_process;
    fir_cfg.tag = "fir_filter";
    fir_filter_el = audio_element_init(&fir_cfg);

    ESP_LOGI(FIRTAG, "[3.4] Register all elements to audio pipeline");
    audio_pipeline_register(pipeline, i2s_stream_reader, "i2s_read");
    audio_pipeline_register(pipeline, fir_filter_el, "fir_filter");
    audio_pipeline_register(pipeline, i2s_stream_writer, "i2s_write");

    ESP_LOGI(FIRTAG, "[3.5] Link it together [codec_chip]-->i2s_stream_reader-->fir_filter_el-->i2s_stream_writer-->[codec_chip]");
    const char *link_tag[3] = {"i2s_read", "fir_filter", "i2s_write"};
    esp_err_t res = audio_pipeline_link(pipeline, &link_tag[0], 3);
    if (res == ESP_FAIL) {
        ESP_LOGE(FIRTAG, "Pipeline Link Failed.");
        return;
    } else {
        ESP_LOGW(FIRTAG, "Pipeline Link Succeeded.");
    }

    ESP_LOGI(FIRTAG, "[ 4 ] Initialize peripherals");
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    // TODO: the following gives an error in the log, although it seems to work
    // gpio_install_isr_service(449): GPIO isr service already installed
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    ESP_LOGI(FIRTAG, "[4.1] Initialize keys on board");
    audio_board_key_init(set);

    ESP_LOGI(FIRTAG, "[ 5 ] Set up  event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);

    ESP_LOGI(FIRTAG, "[5.1] Listening event from all elements of pipeline");
    audio_pipeline_set_listener(pipeline, evt);

    ESP_LOGI(FIRTAG, "[5.2] Listening event from peripherals");
    audio_event_iface_set_listener(esp_periph_set_get_event_iface(set), evt);

    ESP_LOGI(FIRTAG, "[ 6 ] Start audio_pipeline");
    audio_pipeline_run(pipeline);


    ESP_LOGI(FIRTAG, "[ 7 ] Listen for all pipeline events");
    while (1) {
        audio_event_iface_msg_t msg;
        esp_err_t ret = audio_event_iface_listen(evt, &msg, portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(FIRTAG, "[ * ] Event interface error : %d", ret);
            continue;
        }

        /* Stop when the last pipeline element (i2s_stream_writer in this case) receives stop event */
        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *) i2s_stream_writer
            && msg.cmd == AEL_MSG_CMD_REPORT_STATUS
            && (((int)msg.data == AEL_STATUS_STATE_STOPPED) || ((int)msg.data == AEL_STATUS_STATE_FINISHED))) {
            ESP_LOGW(FIRTAG, "[ * ] Stop event received");
            break;
        }

        if ((msg.source_type == PERIPH_ID_TOUCH || msg.source_type == PERIPH_ID_BUTTON || msg.source_type == PERIPH_ID_ADC_BTN)
            && (msg.cmd == PERIPH_BUTTON_PRESSED)) {

            if ((int) msg.data == get_input_mode_id()) {
                ESP_LOGI(FIRTAG, "[ * ] [mode] tap event");
                toggle_filter();
            } else if ((int) msg.data == get_input_volup_id()) {
                ESP_LOGI(FIRTAG, "[ * ] [Vol+] touch tap event");
                player_volume += 10;
                if (player_volume > 100) {
                    player_volume = 100;
                }
                audio_hal_set_volume(board_handle->audio_hal, player_volume);
                ESP_LOGI(FIRTAG, "[ * ] Volume set to %d %%", player_volume);
            } else if ((int) msg.data == get_input_voldown_id()) {
                ESP_LOGI(FIRTAG, "[ * ] [Vol-] touch tap event");
                player_volume -= 10;
                if (player_volume < 0) {
                    player_volume = 0;
                }
                audio_hal_set_volume(board_handle->audio_hal, player_volume);
                ESP_LOGI(FIRTAG, "[ * ] Volume set to %d %%", player_volume);
            }
        }


    }

    ESP_LOGI(FIRTAG, "[ 7 ] Stop audio_pipeline");
    audio_pipeline_stop(pipeline);
    audio_pipeline_wait_for_stop(pipeline);
    audio_pipeline_terminate(pipeline);

    audio_pipeline_unregister(pipeline, i2s_stream_reader);
    audio_pipeline_unregister(pipeline, i2s_stream_writer);
    audio_pipeline_unregister(pipeline, fir_filter_el);

    /* Terminate the pipeline before removing the listener */
    audio_pipeline_remove_listener(pipeline);

    /* Make sure audio_pipeline_remove_listener & audio_event_iface_remove_listener are called before destroying event_iface */
    audio_event_iface_destroy(evt);

    /* Release all resources */
    audio_pipeline_deinit(pipeline);
    audio_element_deinit(i2s_stream_reader);
    audio_element_deinit(fir_filter_el);
    audio_element_deinit(i2s_stream_writer);
}
