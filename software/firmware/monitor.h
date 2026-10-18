#ifndef MONITOR_H
#define MONITOR_H
#include <stdint.h>
#define CHANNELS 16
#define PAGE_BYTES 22
#define INVALID_TEMP INT16_MIN
#define FLAG_HOT 1u
#define FLAG_SENSOR_FAULT 2u
#define FLAG_UNCALIBRATED 4u
#define FLAG_NOT_READY 8u

typedef struct {
    int32_t uv_at_25c;
    int32_t uv_per_c;
} calibration_t;
typedef struct {
    uint16_t sequence;
    int16_t temperature[CHANNELS], maximum;
    uint8_t maximum_channel, flags;
} frame_t;

uint32_t adc_to_uv(uint16_t sum8, uint32_t reference_uv);
int16_t temperature_centi_c(uint32_t uv, calibration_t calibration);
void process_frame(frame_t *frame, const uint16_t sums[CHANNELS],
                   const calibration_t calibration[CHANNELS],
                   uint32_t reference_uv, uint16_t calibrated_mask,
                   uint8_t previous_hot);
void encode_page(const frame_t *frame, uint8_t page, uint8_t out[PAGE_BYTES]);
#endif
