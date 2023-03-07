#pragma once

#include "esp_err.h"
#include "audio_element.h"

/**
 * @brief      Custom filter Configuration
 */
typedef struct {
    int samplerate;  /*!< Audio sample rate (in Hz)*/
    int channel;     /*!< Number of audio channels (Mono=1, Dual=2) */
    int out_rb_size; /*!< Size of output ring buffer */
    int task_stack;  /*!< Task stack size */
    int task_core;   /*!< Task running in core...*/
    int task_prio;   /*!< Task priority*/
} filter_cfg_t;

#define DEFAULT_FILTER_CONFIG() {                    \
        .samplerate     = 48000,                     \
        .channel        = 1,                         \
        .out_rb_size    = 8 * 1024,                  \
        .task_stack     = 4 * 1024,                  \
        .task_core      = 1,                         \
        .task_prio      = 5,                         \
    }

void toggle_filter(audio_element_handle_t self);

audio_element_handle_t filter_init(filter_cfg_t *config);

