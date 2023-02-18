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

#include "board.h"
#include "filter.h"
#include "static.h"


void app_main(void)
{
    audio_pipeline_handle_t pipeline;

    audio_element_handle_t i2s_stream_writer;
    audio_element_handle_t i2s_stream_reader;
    audio_element_handle_t fir_filter_el;

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set(FIRTAG, ESP_LOG_DEBUG);

    ESP_LOGI(FIRTAG, "[ 1 ] Start codec chip");
    audio_board_handle_t board_handle = audio_board_init();

    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START); 
    es8388_config_adc_input(ADC_INPUT_LINPUT2_RINPUT2);
    es8388_write_reg(ES8388_ADCCONTROL10, 0x00); // turn off ALC

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

    ESP_LOGI(FIRTAG, "[ 4 ] Set up  event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);

    ESP_LOGI(FIRTAG, "[4.1] Listening event from all elements of pipeline");
    audio_pipeline_set_listener(pipeline, evt);

    ESP_LOGI(FIRTAG, "[ 5 ] Start audio_pipeline");
    audio_pipeline_run(pipeline);

    ESP_LOGI(FIRTAG, "[ 6 ] Listen for all pipeline events");
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
