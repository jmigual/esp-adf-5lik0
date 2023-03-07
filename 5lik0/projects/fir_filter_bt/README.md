# FIR Filter

## Example Brief

This example demonstrates how to pass the audio received from the `Line In 2` port (aux in jack) through a FIR filter to the earphone or speaker ports.

The passthrough pipeline of this example is as follows:

```

[codec_chip] ---> i2s_stream_reader ---> fir_filter_el ---> i2s_stream_writer ---> [codec_chip]

```
## Environment Setup

### Hardware Required

The default board for this example is `ESP32-Lyrat V4.3`, which is the only supported board so far.

## Build and Flash

### Default IDF Branch

This example supports IDF release/v3.3 and later branches. By default, it runs on ADF's built-in branch `$ADF_PATH/esp-idf`.

### Configuration

Prepare an aux audio cable to connect the `AUX_IN` of the development board and the `Line-Out` of the audio output side.

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output (replace `PORT` with your board's serial port name):

```
idf.py -p PORT flash monitor
```

To exit the serial monitor, type ``Ctrl-]``.

See [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/release-v4.2/esp32/index.html) for full steps to configure and build an ESP-IDF project.

## How to Use the Example

## Troubleshooting

- 