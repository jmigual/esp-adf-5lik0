
#include "filter.h"
#include "esp_log.h"
#include "static.h"

#include "filtercoefficients.h"

bool filter_on = true;

// two circular buffers (left/right) to hold the old samples for the FIR convolution 
int16_t fir_circular_buffer_left[FIR_FILTER_LENGTH];
int16_t fir_circular_buffer_right[FIR_FILTER_LENGTH];
// an index for the circular buffer
unsigned int fir_index = 0;

// variables to accumulate the output during the FIR computations 
int32_t fir_accum_left = 0;
int32_t fir_accum_right = 0;

void toggle_filter() {
    filter_on = ! filter_on;
}


esp_err_t filter_open(audio_element_handle_t self) {
    ESP_LOGI(FIRTAG, "The FIR filter is starting");

       /* initialize the circular buffer */
       for (int i = 0  ; i < FIR_FILTER_LENGTH  ; i++ )
       {
           fir_circular_buffer_left[i] = 0;
           fir_circular_buffer_right[i] = 0;
       }
   
    return ESP_OK;
}

inline void filter_sample(int16_t left_input, int16_t right_input, int16_t *left_output, int16_t *right_output) {

    // update the buffer index
    fir_index++;
    if(fir_index >= FIR_FILTER_LENGTH) fir_index = 0;

    // store the new samples in the circular buffers
    fir_circular_buffer_left[fir_index] = left_input;
    fir_circular_buffer_right[fir_index] = right_input;

    /* compute the FIR filters output */
    fir_accum_left = 0;
    fir_accum_right = 0;

    for (int j = 0  ; j < FIR_FILTER_LENGTH; j++ )
    {
        // do computations in 32 bit for accuracy, considering the fixed point representation of the
        // filter coefficients
        fir_accum_left += (int32_t)fir_circular_buffer_left[fir_index] * (int32_t)FIRFilterCoefficients[j];
        fir_accum_right += (int32_t)fir_circular_buffer_right[fir_index] * (int32_t)FIRFilterCoefficients[j];
        if(fir_index != 0){
            fir_index --;
        } else {
            fir_index = FIR_FILTER_LENGTH-1;
        }
    }

    // divide the computed outputs according to the fixed point representation of the filter coefficients
    // if the filter coefficients have N fractional bits, we need to shit N bits
    *left_output = fir_accum_left >> FIR_FRACTIONAL_BITS;
    *right_output = fir_accum_right >> FIR_FRACTIONAL_BITS;
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

    int16_t *buffer = (int16_t *)in;
    while (num_samples > 0)
    {
        if (filter_on) {
            filter_sample(buffer[0], buffer[1], &buffer[0], &buffer[1]);
        }
        buffer += 2;
        --num_samples;
    }

    int nrProd = audio_element_output(self, in, new_len);
    return nrProd;
}

audio_element_handle_t filter_init()
{
    
}