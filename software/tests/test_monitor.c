#include "../firmware/monitor.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static calibration_t cal[CHANNELS];
static uint16_t sums[CHANNELS];
static uint16_t at_uv(uint32_t uv) { return (uint16_t)(((uint64_t)uv * 8192u + 550000u) / 1100000u); }
static void fill(uint32_t uv) { for (unsigned i=0;i<CHANNELS;++i) sums[i]=at_uv(uv); }
int main(void) {
    for (unsigned i=0;i<CHANNELS;++i) cal[i]=(calibration_t){650000,-2000};
    assert(adc_to_uv(0,1100000)==0);
    assert(adc_to_uv(8192,1100000)==1100000);
    assert(temperature_centi_c(650000,cal[0])==2500);
    assert(temperature_centi_c(580000,cal[0])==6000);
    assert(temperature_centi_c(580020,cal[0])==5999);
    assert(temperature_centi_c(579980,cal[0])==6001);
    assert(temperature_centi_c(700000,cal[0])==0);
    assert(temperature_centi_c(650000,(calibration_t){650000,0})==INVALID_TEMP);
    frame_t f={0};
    fill(650000); process_frame(&f,sums,cal,1100000,0,0);
    assert(f.valid_mask==UINT16_MAX && f.flags==FLAG_UNCALIBRATED);
    assert(f.maximum_channel==0 && f.maximum>=2495 && f.maximum<=2505);
    /* Highest temperature corresponds to the LOWEST forward voltage. */
    sums[15]=at_uv(570000); process_frame(&f,sums,cal,1100000,UINT16_MAX,0);
    assert(f.maximum_channel==15 && (f.flags&FLAG_HOT));
    fill(582000); process_frame(&f,sums,cal,1100000,UINT16_MAX,FLAG_HOT);
    assert(f.flags&FLAG_HOT);
    fill(586000); process_frame(&f,sums,cal,1100000,UINT16_MAX,FLAG_HOT);
    assert(!(f.flags&FLAG_HOT));
    sums[0]=0; sums[1]=8184; process_frame(&f,sums,cal,1100000,UINT16_MAX,0);
    assert(f.valid_mask==0xFFFCu && (f.flags&FLAG_SENSOR_FAULT));
    assert(f.temperature[0]==INVALID_TEMP && f.temperature[1]==INVALID_TEMP);
    memset(sums,0,sizeof sums); process_frame(&f,sums,cal,1100000,0,FLAG_HOT);
    assert(f.maximum==INVALID_TEMP && f.minimum==INVALID_TEMP && f.average==INVALID_TEMP);
    assert(f.maximum_channel==UINT8_MAX && (f.flags&FLAG_HOT));
    uint8_t page[PAGE_BYTES]; f.sequence=0x1234; f.uptime_ms=0x12345678;
    for (unsigned i=0;i<CHANNELS;++i) f.temperature[i]=(int16_t)(i*100-500);
    encode_page(&f,0,page);
    assert(page[2]==0x34 && page[3]==0x12 && page[16]==0x78 && page[19]==0x12);
    assert(page[23]==crc8(page,23));
    encode_page(&f,1,page); assert(page[5]==0 && page[6]==0x0C && page[7]==0xFE);
    encode_page(&f,2,page); assert(page[5]==8 && page[6]==0x2C && page[7]==0x01);
    encode_page(&f,3,page); assert(page[1]==255 && page[23]==crc8(page,23));
    assert(crc8((const uint8_t *)"123456789",9)==0xF4);
    puts("PASS: conversion, calibration, max/min/mean, hot hysteresis, faults, serialization, CRC");
}
