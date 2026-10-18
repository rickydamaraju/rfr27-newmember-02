#include "monitor.h"
#include <string.h>

uint32_t adc_to_uv(uint16_t sum8, uint32_t reference_uv) {
    /* Eight 10-bit samples. 64 bits avoid overflow near full scale. */
    return (uint32_t)(((uint64_t)sum8 * reference_uv + 4096u) / 8192u);
}

int16_t temperature_centi_c(uint32_t uv, calibration_t c) {
    if (c.uv_per_c > -100 || c.uv_per_c < -10000) return INVALID_TEMP;
    int64_t t = 2500 + ((int64_t)uv - c.uv_at_25c) * 100 / c.uv_per_c;
    if (t < -4000 || t > 12500) return INVALID_TEMP;
    return (int16_t)t;
}

void process_frame(frame_t *f, const uint16_t sums[CHANNELS],
                   const calibration_t c[CHANNELS], uint32_t ref,
                   uint16_t calibrated_mask, uint8_t previous_hot) {
    f->valid_mask = 0;
    f->calibrated_mask = calibrated_mask;
    f->flags = calibrated_mask == UINT16_MAX ? 0 : FLAG_UNCALIBRATED;
    f->maximum_channel = UINT8_MAX;
    f->minimum = f->maximum = f->average = INVALID_TEMP;
    int32_t total = 0;
    uint8_t count = 0;
    for (uint8_t i = 0; i < CHANNELS; ++i) {
        uint32_t uv = adc_to_uv(sums[i], ref);
        int16_t t = (sums[i] > 5u * 8u && sums[i] < 1018u * 8u &&
                     uv >= 250000u && uv <= 950000u)
                    ? temperature_centi_c(uv, c[i]) : INVALID_TEMP;
        f->temperature[i] = t;
        if (t == INVALID_TEMP) continue;
        f->valid_mask |= (uint16_t)1u << i;
        if (count == 0 || t < f->minimum) f->minimum = t;
        if (count == 0 || t > f->maximum) {
            f->maximum = t;
            f->maximum_channel = i;
        }
        total += t;
        ++count;
    }
    if (count) f->average = (int16_t)(total / count);
    if (f->valid_mask != UINT16_MAX) f->flags |= FLAG_SENSOR_FAULT;
    /* Assert strictly above 60 C. Once hot, clear only at/below 58 C
       with all sensors valid, so a broken hot sensor cannot clear it. */
    if ((count && f->maximum > 6000) ||
        (previous_hot && (f->valid_mask != UINT16_MAX || f->maximum > 5800)))
        f->flags |= FLAG_HOT;
}

static void put16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
uint8_t crc8(const uint8_t *p, uint8_t n) {
    uint8_t crc = 0;
    while (n--) {
        crc ^= *p++;
        for (uint8_t b = 0; b < 8; ++b)
            crc = (crc & 0x80u) ? (uint8_t)((crc << 1) ^ 0x07u) : (uint8_t)(crc << 1);
    }
    return crc;
}
void encode_page(const frame_t *f, uint8_t page, uint8_t out[PAGE_BYTES]) {
    memset(out, 0, PAGE_BYTES);
    out[0] = 1; out[1] = page;
    put16(out + 2, f->sequence);
    out[4] = f->flags;
    if (page == 0) {
        out[5] = f->maximum_channel;
        put16(out + 6, f->valid_mask);
        put16(out + 8, f->calibrated_mask);
        put16(out + 10, (uint16_t)f->maximum);
        put16(out + 12, (uint16_t)f->minimum);
        put16(out + 14, (uint16_t)f->average);
        for (uint8_t b = 0; b < 4; ++b) out[16 + b] = (uint8_t)(f->uptime_ms >> (8u * b));
    } else if (page == 1 || page == 2) {
        out[5] = (uint8_t)((page - 1) * 8);
        for (uint8_t i = 0; i < 8; ++i)
            put16(out + 6 + 2 * i, (uint16_t)f->temperature[out[5] + i]);
    } else {
        out[1] = UINT8_MAX; /* Invalid command; no stale page returned. */
    }
    out[23] = crc8(out, 23);
}
