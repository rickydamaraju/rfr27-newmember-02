#include "monitor.h"
#include <string.h>

uint32_t adc_to_uv(uint16_t sum8, uint32_t reference_uv) {
    return (uint32_t)(((uint64_t)sum8 * reference_uv + 4096u) / 8192u);
}

int16_t temperature_centi_c(uint32_t uv, calibration_t sensor_calibration) {
    if (sensor_calibration.uv_per_c > -100 ||
        sensor_calibration.uv_per_c < -10000) {
        return INVALID_TEMP;
    }

    int64_t temperature = 2500 +
        ((int64_t)uv - sensor_calibration.uv_at_25c) * 100 /
        sensor_calibration.uv_per_c;

    if (temperature < -4000 || temperature > 12500) {
        return INVALID_TEMP;
    }
    return (int16_t)temperature;
}

void process_frame(frame_t *frame, const uint16_t sums[CHANNELS],
                   const calibration_t sensor_calibration[CHANNELS],
                   uint32_t reference_uv,
                   uint16_t calibrated_mask, uint8_t previous_hot) {
    frame->flags = calibrated_mask == UINT16_MAX ? 0 : FLAG_UNCALIBRATED;
    frame->maximum_channel = UINT8_MAX;
    frame->maximum = INVALID_TEMP;
    uint8_t valid_count = 0;

    for (uint8_t i = 0; i < CHANNELS; ++i) {
        uint32_t uv = adc_to_uv(sums[i], reference_uv);
        int16_t temperature = INVALID_TEMP;

        if (sums[i] > 5u * 8u && sums[i] < 1018u * 8u &&
            uv >= 250000u && uv <= 950000u) {
            temperature = temperature_centi_c(uv, sensor_calibration[i]);
        }

        frame->temperature[i] = temperature;
        if (temperature == INVALID_TEMP) {
            frame->flags |= FLAG_SENSOR_FAULT;
            continue;
        }

        if (valid_count == 0 || temperature > frame->maximum) {
            frame->maximum = temperature;
            frame->maximum_channel = i;
        }
        ++valid_count;
    }

    if ((valid_count && frame->maximum > 6000) ||
        (previous_hot && ((frame->flags & FLAG_SENSOR_FAULT) ||
                          frame->maximum > 5800))) {
        frame->flags |= FLAG_HOT;
    }
}

static void put16(uint8_t *output, uint16_t value) {
    output[0] = (uint8_t)value;
    output[1] = (uint8_t)(value >> 8);
}

void encode_page(const frame_t *frame, uint8_t page, uint8_t out[PAGE_BYTES]) {
    memset(out, 0, PAGE_BYTES);
    out[0] = 1;
    out[1] = page;
    put16(out + 2, frame->sequence);
    out[4] = frame->flags;
    out[5] = frame->maximum_channel;

    if (page == 0) {
        put16(out + 6, (uint16_t)frame->maximum);
    } else if (page == 1 || page == 2) {
        for (uint8_t i = 0; i < 8; ++i) {
            put16(out + 6 + 2 * i,
                  (uint16_t)frame->temperature[(page - 1) * 8 + i]);
        }
    } else {
        out[1] = UINT8_MAX;
    }
}
