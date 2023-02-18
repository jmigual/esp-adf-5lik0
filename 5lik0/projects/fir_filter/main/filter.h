#pragma once

#include "esp_err.h"
#include "audio_element.h"

esp_err_t filter_open(audio_element_handle_t self);
int filter_process(audio_element_handle_t self, char *buf, int len);