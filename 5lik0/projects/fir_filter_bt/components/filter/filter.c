
#include "filter.h"

#include "filtercoefficients.h"

#include <audio_error.h>
#include <audio_mem.h>
#include <esp_log.h>

static const char *TAG = "FILTER";

static const int BYTES_PER_RL_SAMPLE = 4; // 2 bytes per sample, 2 channels

typedef struct {
	bool filter_on;
	unsigned int fir_index; // Index for the circular buffer
	// two circular buffers (left/right) to hold the old samples for the FIR convolution 
	int16_t fir_circular_buffer_left[FIR_FILTER_LENGTH];
	int16_t fir_circular_buffer_right[FIR_FILTER_LENGTH];
	// variables to accumulate the output during the FIR computations 
} filter_t;


void toggle_filter(audio_element_handle_t self) {
	filter_t *filter = (filter_t *)audio_element_getdata(self);
	filter->filter_on = ! filter->filter_on;
}


static esp_err_t filter_open(audio_element_handle_t self) {
    ESP_LOGI(TAG, "The FIR filter is starting");
	filter_t *filter = (filter_t *)audio_element_getdata(self);

   	/* initialize the circular buffer */
   	for (int i = 0  ; i < FIR_FILTER_LENGTH  ; i++ )
   	{
   		filter->fir_circular_buffer_left[i] = 0;
   		filter->fir_circular_buffer_right[i] = 0;
   	}
   
    return ESP_OK;
}

static esp_err_t filter_close(audio_element_handle_t self) {
	ESP_LOGI(TAG, "The FIR filter is stopping");
	return ESP_OK;
}

static esp_err_t filter_destroy(audio_element_handle_t self) {
	filter_t *filter = (filter_t *)audio_element_getdata(self);
	audio_free(filter);
	return ESP_OK;
}


inline void filter_sample(filter_t *filter, int16_t left_input, int16_t right_input, int16_t *left_output, int16_t *right_output) {

	// update the buffer index
	filter->fir_index++;
	if(filter->fir_index >= FIR_FILTER_LENGTH) filter->fir_index = 0;

	// store the new samples in the circular buffers
	filter->fir_circular_buffer_left[filter->fir_index] = left_input;
	filter->fir_circular_buffer_right[filter->fir_index] = right_input;

	/* compute the FIR filters output */
	int32_t fir_accum_left = 0;
	int32_t fir_accum_right = 0;

	for (int j = 0  ; j < FIR_FILTER_LENGTH; j++ )
	{
		// do computations in 32 bit for accuracy, considering the fixed point representation of the
		// filter coefficients
		fir_accum_left += (int32_t)filter->fir_circular_buffer_left[filter->fir_index] * (int32_t)FIRFilterCoefficients[j];
		fir_accum_right += (int32_t)filter->fir_circular_buffer_right[filter->fir_index] * (int32_t)FIRFilterCoefficients[j];
		if(filter->fir_index != 0){
			filter->fir_index --;
		} else {
			filter->fir_index = FIR_FILTER_LENGTH-1;
		}
	}

	// divide the computed outputs according to the fixed point representation of the filter coefficients
	// if the filter coefficients have N fractional bits, we need to shit N bits
	*left_output = fir_accum_left >> FIR_FRACTIONAL_BITS;
	*right_output = fir_accum_right >> FIR_FRACTIONAL_BITS;
}

static esp_err_t filter_process(audio_element_handle_t self, char *in, int len)
{
	filter_t *filter = (filter_t *)audio_element_getdata(self);
	int diff = 0;
	if ((diff = len % BYTES_PER_RL_SAMPLE) != 0)
	{
		ESP_LOGD(TAG, "Need to adapt buffer length %d to %d", len, len - diff);
	}

	// Note: LR audio interleaves samples from left and right. So, we need to process 4 bytes at a time.
	// if we are not reading a multiple of 4 bytes, we only process until the multiple of 4.
	int r_size = audio_element_input(self, in, len - diff);
	if (r_size <= 0)
	{
		ESP_LOGE(TAG, "ALARM! %d", r_size);
		return r_size;
	}

	if (r_size % BYTES_PER_RL_SAMPLE != 0)
	{
		ESP_LOGW(TAG, "Could not get full samples");
	}

	int new_len = r_size - (r_size % 4);
	int num_samples = new_len / BYTES_PER_RL_SAMPLE;

	int16_t *buffer = (int16_t *)in;
    while (num_samples > 0)
	{
		if (filter->filter_on) {
			filter_sample(filter, buffer[0], buffer[1], &buffer[0], &buffer[1]);
		}
		buffer += 2;
		--num_samples;
	}

	int nrProd = audio_element_output(self, in, new_len);
    return nrProd;
}

audio_element_handle_t filter_init(filter_cfg_t *config) {
	if (config == NULL) {
		ESP_LOGE(TAG, "Filter config is NULL");
		return NULL;
	}

	filter_t *filter = audio_calloc(1, sizeof(filter_t));
	AUDIO_MEM_CHECK(TAG, filter, return NULL);
	
	if (filter == NULL) {
		ESP_LOGE(TAG, "audio_calloc failed for filter.");
		return NULL;
	}

	audio_element_cfg_t cfg = DEFAULT_AUDIO_ELEMENT_CONFIG();
    // Set filter callback functions
    cfg.open = filter_open;
    cfg.process = filter_process;
	cfg.close = filter_close;
	cfg.destroy = filter_destroy;
    cfg.tag = "fir_filter";
	cfg.task_stack = config->task_stack;
	cfg.task_core = config->task_core;
	cfg.task_prio = config->task_prio;
	cfg.out_rb_size = config->out_rb_size;
    audio_element_handle_t el = audio_element_init(&cfg);

	filter->filter_on = true;
	filter->fir_index = 0;
	audio_element_setdata(el, filter);
	return el;
}