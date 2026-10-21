#include <Arduino.h>
#include <Wire.h>
#include <string.h>

const int PIN_A = 2;
const int PIN_B = 3;
const int PIN_C = 4;
const int PIN_MUX_ENABLE = 6;
const int PIN_ADC = A0;
const int PIN_LED = 7;

const int NUM_CHANNELS = 8;
const int NUM_SAMPLES = 8;
const int PAGE_BYTES = 22;
const int I2C_ADDRESS = 0x32;

const float ADC_REFERENCE = 1.1f;
const float FAULT_TEMP = 60.0f;
const float CLEAR_TEMP = 58.0f;
const float INVALID_TEMP = -1000.0f;

const unsigned long SCAN_MS = 50;
const unsigned long BLINK_MS = 250;
const bool CALIBRATED = false;

const float V_CAL[NUM_CHANNELS] = {
  0.65f, 0.65f, 0.65f, 0.65f, 0.65f, 0.65f, 0.65f, 0.65f
};
const float TEMP_CAL[NUM_CHANNELS] = {
  25.0f, 25.0f, 25.0f, 25.0f, 25.0f, 25.0f, 25.0f, 25.0f
};
const float SLOPE[NUM_CHANNELS] = {
  -0.002f, -0.002f, -0.002f, -0.002f,
  -0.002f, -0.002f, -0.002f, -0.002f
};

float temps[NUM_CHANNELS];
float maxTemp = INVALID_TEMP;
int maxChannel = 255;
bool sensorFault = true;
bool hotAlert = false;
bool ledOn = false;
bool hasData = false;
unsigned long lastToggle = 0;
uint16_t scanNumber = 0;

uint8_t pages[2][PAGE_BYTES];
const uint8_t badPage[PAGE_BYTES] = {1, 255};
volatile uint8_t selectedPage = 0;

void scanAllChannels();
float findMaxTemp();
void updateFault();
void updateLed();
void buildPages();
void onI2CReceive(int count);
void onI2CRequest();

void setup() {
  pinMode(PIN_A, OUTPUT);
  pinMode(PIN_B, OUTPUT);
  pinMode(PIN_C, OUTPUT);
  digitalWrite(PIN_MUX_ENABLE, HIGH);
  pinMode(PIN_MUX_ENABLE, OUTPUT);
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);

  analogReference(INTERNAL);
  delay(10);
  analogRead(PIN_ADC);

  for (int i = 0; i < NUM_CHANNELS; i++) {
    temps[i] = INVALID_TEMP;
  }
  buildPages();

  Wire.begin(I2C_ADDRESS);
  Wire.onReceive(onI2CReceive);
  Wire.onRequest(onI2CRequest);
}

void loop() {
  unsigned long scanStart = millis();

  scanAllChannels();
  maxTemp = findMaxTemp();
  updateFault();
  hasData = true;
  scanNumber++;
  buildPages();

  do {
    updateLed();
  } while (millis() - scanStart < SCAN_MS);
}

void selectChannel(int channel) {
  digitalWrite(PIN_MUX_ENABLE, HIGH);
  digitalWrite(PIN_A, (channel >> 0) & 1);
  digitalWrite(PIN_B, (channel >> 1) & 1);
  digitalWrite(PIN_C, (channel >> 2) & 1);
  digitalWrite(PIN_MUX_ENABLE, LOW);
  delayMicroseconds(100);
}

float readVoltage(int channel) {
  selectChannel(channel);
  analogRead(PIN_ADC);

  int total = 0;
  for (int i = 0; i < NUM_SAMPLES; i++) {
    total += analogRead(PIN_ADC);
  }

  float average = (float)total / NUM_SAMPLES;
  return average * ADC_REFERENCE / 1024.0f;
}

float voltageToTemp(int channel, float voltage) {
  return TEMP_CAL[channel] +
         (voltage - V_CAL[channel]) / SLOPE[channel];
}

void scanAllChannels() {
  sensorFault = false;

  for (int i = 0; i < NUM_CHANNELS; i++) {
    float voltage = readVoltage(i);
    float temperature = voltageToTemp(i, voltage);

    if (voltage < 0.25f || voltage > 0.95f ||
        temperature < -40.0f || temperature > 125.0f) {
      temps[i] = INVALID_TEMP;
      sensorFault = true;
    } else {
      temps[i] = temperature;
    }

    updateLed();
  }
}

float findMaxTemp() {
  float highest = INVALID_TEMP;
  maxChannel = 255;

  for (int i = 0; i < NUM_CHANNELS; i++) {
    if (temps[i] != INVALID_TEMP &&
        (maxChannel == 255 || temps[i] > highest)) {
      highest = temps[i];
      maxChannel = i;
    }
  }

  return highest;
}

void updateFault() {
  if (maxTemp > FAULT_TEMP) {
    hotAlert = true;
  } else if (maxTemp <= CLEAR_TEMP && !sensorFault) {
    hotAlert = false;
  }
}

void updateLed() {
  if (!hotAlert && !sensorFault) {
    ledOn = false;
    digitalWrite(PIN_LED, LOW);
  } else if (millis() - lastToggle >= BLINK_MS) {
    ledOn = !ledOn;
    lastToggle = millis();
    digitalWrite(PIN_LED, ledOn ? HIGH : LOW);
  }
}

int16_t toCentiDegrees(float temperature) {
  if (temperature == INVALID_TEMP) {
    return -32768;
  }

  float scaled = temperature * 100.0f;
  if (scaled >= 0) {
    return (int16_t)(scaled + 0.5f);
  }
  return (int16_t)(scaled - 0.5f);
}

void write16(uint8_t *buffer, uint16_t value) {
  buffer[0] = (uint8_t)value;
  buffer[1] = (uint8_t)(value >> 8);
}

void buildPages() {
  uint8_t nextPages[2][PAGE_BYTES];
  memset(nextPages, 0, sizeof(nextPages));

  uint8_t flags = 0;
  if (hotAlert) flags |= 1;
  if (sensorFault) flags |= 2;
  if (!CALIBRATED) flags |= 4;
  if (!hasData) flags |= 8;

  for (int page = 0; page < 2; page++) {
    nextPages[page][0] = 1;
    nextPages[page][1] = page;
    write16(&nextPages[page][2], scanNumber);
    nextPages[page][4] = flags;
    nextPages[page][5] = maxChannel;
  }

  write16(&nextPages[0][6], (uint16_t)toCentiDegrees(maxTemp));
  for (int i = 0; i < NUM_CHANNELS; i++) {
    write16(&nextPages[1][6 + 2 * i],
            (uint16_t)toCentiDegrees(temps[i]));
  }

  noInterrupts();
  memcpy(pages, nextPages, sizeof(pages));
  interrupts();
}

void onI2CReceive(int count) {
  if (count == 1 && Wire.available()) {
    int page = Wire.read();
    selectedPage = page >= 0 && page < 2 ? page : 255;
  } else {
    selectedPage = 255;
  }

  while (Wire.available()) {
    Wire.read();
  }
}

void onI2CRequest() {
  if (selectedPage < 2) {
    Wire.write(pages[selectedPage], PAGE_BYTES);
  } else {
    Wire.write(badPage, PAGE_BYTES);
  }
}
