#ifndef CALIBRATION_H
#define CALIBRATION_H
/* PROVISIONAL demonstration model, NOT measured diode specifications.
   Replace each row after two-point calibration of the installed circuit.
   Set each corresponding mask bit only after independent validation. */
#define ADC_REFERENCE_UV 1100000UL
#define CALIBRATED_MASK 0x0000u
static const calibration_t calibration[CHANNELS] = {
    {650000,-2000}, {650000,-2000}, {650000,-2000}, {650000,-2000},
    {650000,-2000}, {650000,-2000}, {650000,-2000}, {650000,-2000},
    {650000,-2000}, {650000,-2000}, {650000,-2000}, {650000,-2000},
    {650000,-2000}, {650000,-2000}, {650000,-2000}, {650000,-2000}
};
#endif
