
#include "filter.h"
#include "esp_log.h"
#include "static.h"

esp_err_t filter_open(audio_element_handle_t self) {
    ESP_LOGI(FIRTAG, "The FIR filter is starting");
    return ESP_OK;
}

esp_err_t filter_process(audio_element_handle_t self, char *in, int len)
{
	// ESP_LOGI(FIRTAG, "%s", __func__);
	int diff = 0;
	if ((diff = len % 4) != 0)
	{
		ESP_LOGD(FIRTAG, "Need to adapt buffer length %d to %d", len, len - diff);
	}

	int r_size = audio_element_input(self, in, len - diff);
	if (r_size <= 0)
	{
		ESP_LOGE(FIRTAG, "ALARM! %d", r_size);
		return r_size;
	}

	if (r_size % 4 != 0)
	{
		ESP_LOGW(FIRTAG, "Could not get full samples");
	}

	int new_len = r_size - (r_size % 4);
	int num_samples = new_len / 4;

	int16_t *left = (int16_t *)in;
    while (num_samples > 0)
	{
		left[0] = left[0];
		left[1] = left[1];
		left += 2;
		--num_samples;
	}

	int nrProd = audio_element_output(self, in, new_len);
    return nrProd;
}