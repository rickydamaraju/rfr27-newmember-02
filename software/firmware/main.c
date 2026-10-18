#ifndef F_CPU
#define F_CPU 16000000UL
#endif
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <util/delay.h>
#include <util/twi.h>
#include <string.h>
#include "monitor.h"
#include "calibration.h"

#define I2C_ADDRESS 0x32
#define MUX_ENABLE PD6
#define FAULT_LED PD7
static volatile uint32_t milliseconds;
static volatile uint8_t published_bank;
static volatile uint8_t selected_page;
static volatile uint8_t receiving_count;
static volatile uint8_t receiving_value;
static uint8_t pages[2][3][PAGE_BYTES];
static uint8_t transmit[PAGE_BYTES];
static uint8_t transmit_index;

ISR(TIMER0_COMPA_vect) {
    ++milliseconds;
}

static uint32_t now_ms(void) {
    uint32_t result;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        result = milliseconds;
    }
    return result;
}

ISR(TWI_vect) {
    switch (TW_STATUS) {
    case TW_SR_SLA_ACK:
    case TW_SR_ARB_LOST_SLA_ACK:
        receiving_count = 0;
        break;
    case TW_SR_DATA_ACK:
        if (receiving_count == 0) {
            receiving_value = TWDR;
        }
        if (receiving_count < UINT8_MAX) {
            ++receiving_count;
        }
        break;
    case TW_SR_STOP:
        if (receiving_count == 1 && receiving_value <= 2) {
            selected_page = receiving_value;
        } else {
            selected_page = UINT8_MAX;
        }
        break;
    case TW_ST_SLA_ACK:
    case TW_ST_ARB_LOST_SLA_ACK:
        if (selected_page <= 2) {
            memcpy(transmit, pages[published_bank][selected_page], PAGE_BYTES);
        } else {
            memset(transmit, 0, PAGE_BYTES);
            transmit[0] = 1;
            transmit[1] = UINT8_MAX;
        }
        transmit_index = 0;
        TWDR = transmit[transmit_index++];
        break;
    case TW_ST_DATA_ACK:
        TWDR = transmit_index < PAGE_BYTES ? transmit[transmit_index++] : UINT8_MAX;
        break;
    case TW_BUS_ERROR:
        TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWEA) | _BV(TWSTO);
        return;
    default:
        break;
    }
    TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWEA);
}

static uint16_t read_adc(void) {
    ADCSRA |= _BV(ADSC);
    while (ADCSRA & _BV(ADSC)) {
    }
    return ADC;
}

static void select_channel(uint8_t channel) {
    PORTD |= _BV(MUX_ENABLE);
    PORTD = (PORTD & (uint8_t)~0x3Cu) | (uint8_t)((channel & 15u) << 2);
    PORTD &= (uint8_t)~_BV(MUX_ENABLE);
    _delay_us(100);
}

static void update_led(uint8_t flags) {
    uint8_t fault = flags & (FLAG_HOT | FLAG_SENSOR_FAULT | FLAG_NOT_READY);
    if (fault && ((now_ms() / 250u) & 1u) == 0) {
        PORTD |= _BV(FAULT_LED);
    } else {
        PORTD &= (uint8_t)~_BV(FAULT_LED);
    }
}

static void publish(const frame_t *frame) {
    uint8_t next = published_bank ^ 1u;
    for (uint8_t page = 0; page < 3; ++page) {
        encode_page(frame, page, pages[next][page]);
    }
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        published_bank = next;
    }
}

int main(void) {
    PORTD = _BV(MUX_ENABLE);
    DDRD |= 0xFCu;
    DDRC &= (uint8_t)~(_BV(PC0) | _BV(PC4) | _BV(PC5));
    PORTC &= (uint8_t)~(_BV(PC0) | _BV(PC4) | _BV(PC5));
    DIDR0 = _BV(ADC0D);
    ADMUX = _BV(REFS1) | _BV(REFS0);
    ADCSRA = _BV(ADEN) | _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0);
    _delay_ms(10);
    (void)read_adc();
    TCCR0A = _BV(WGM01);
    TCCR0B = _BV(CS01) | _BV(CS00);
    OCR0A = 249;
    TIMSK0 = _BV(OCIE0A);
    frame_t frame;
    memset(&frame, 0, sizeof frame);
    frame.flags = FLAG_NOT_READY | FLAG_SENSOR_FAULT | FLAG_UNCALIBRATED;
    frame.maximum = INVALID_TEMP;
    frame.maximum_channel = UINT8_MAX;
    for (uint8_t i = 0; i < CHANNELS; ++i) {
        frame.temperature[i] = INVALID_TEMP;
    }
    publish(&frame);
    TWAR = I2C_ADDRESS << 1;
    TWCR = _BV(TWEN) | _BV(TWIE) | _BV(TWEA);
    sei();
    for (;;) {
        uint32_t start = now_ms();
        uint16_t sums[CHANNELS];
        for (uint8_t i = 0; i < CHANNELS; ++i) {
            select_channel(i);
            (void)read_adc();
            sums[i] = 0;
            for (uint8_t j = 0; j < 8; ++j) {
                sums[i] += read_adc();
            }
            update_led(frame.flags);
        }
        uint8_t previous_hot = frame.flags & FLAG_HOT;
        process_frame(&frame, sums, calibration, ADC_REFERENCE_UV,
                      CALIBRATED_MASK, previous_hot);
        ++frame.sequence;
        publish(&frame);
        do {
            update_led(frame.flags);
        } while ((uint32_t)(now_ms() - start) < 50u);
    }
}
