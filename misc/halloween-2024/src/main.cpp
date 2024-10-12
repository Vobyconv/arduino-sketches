#include <Automaton.h>
#include <Adafruit_NeoPixel.h>
#include <CircularBuffer.hpp>

/**
 * Proximity sensors.
 */

const uint8_t PROX_SENSORS_NUM = 3;
const uint8_t PROX_SENSORS_PINS[PROX_SENSORS_NUM] = {2, 3, 4};
Atm_button proxSensorsBtn[PROX_SENSORS_NUM];

/**
 * LED strips.
 */

const uint8_t LED_BRIGHTNESS = 190;
const uint8_t LED_TORCHES_NUM = 24;
const uint8_t LED_TORCHES_PINS[PROX_SENSORS_NUM] = {6, 7, 8};

const uint32_t TORCH_COLOR = Adafruit_NeoPixel::Color(0, 0, 200);

Adafruit_NeoPixel ledsTorches[PROX_SENSORS_NUM] = {
    Adafruit_NeoPixel(LED_TORCHES_NUM, LED_TORCHES_PINS[0], NEO_GRB + NEO_KHZ800),
    Adafruit_NeoPixel(LED_TORCHES_NUM, LED_TORCHES_PINS[1], NEO_GRB + NEO_KHZ800),
    Adafruit_NeoPixel(LED_TORCHES_NUM, LED_TORCHES_PINS[2], NEO_GRB + NEO_KHZ800)};

const uint16_t LED_BONFIRE_NUM = 150;
const uint8_t LED_BONFIRE_PIN = 12;

Adafruit_NeoPixel ledBonfire = Adafruit_NeoPixel(LED_BONFIRE_NUM, LED_BONFIRE_PIN, NEO_GRB + NEO_KHZ800);

/**
 * Relay.
 */

const int PIN_RELAY = 9;

/**
 * Audio FX.
 */

const uint8_t PIN_AUDIO_RST = A0;
const uint8_t PIN_AUDIO_ACT = A1;
const uint8_t PIN_AUDIO_FINAL = A2;

/**
 * Program state.
 */

const uint8_t SENSOR_SOLUTION[PROX_SENSORS_NUM] = {1, 2, 0};

CircularBuffer<uint8_t, PROX_SENSORS_NUM> sensorHistory;

Atm_timer timerState;
const int STATE_TIMER_MS = 60;

bool isTorchActive[PROX_SENSORS_NUM];

typedef struct programState
{
  bool *isTorchActive;
  bool isBonfireActive;
} ProgramState;

ProgramState progState = {
    .isTorchActive = isTorchActive,
    .isBonfireActive = false};

void cleanState()
{
  for (uint8_t i = 0; i < PROX_SENSORS_NUM; i++)
  {
    progState.isTorchActive[i] = false;
  }

  progState.isBonfireActive = false;
}

/**
 * Functions to manage the relay.
 */

void lockRelay()
{
  digitalWrite(PIN_RELAY, LOW);
}

void openRelay()
{
  digitalWrite(PIN_RELAY, HIGH);
}

void initRelay()
{
  pinMode(PIN_RELAY, OUTPUT);
  lockRelay();
}

/**
 * Audio FX functions.
 */

bool isTrackPlaying()
{
  return digitalRead(PIN_AUDIO_ACT) == LOW;
}

void playTrack(uint8_t trackPin)
{
  const unsigned long pinDelayMs = 300;

  if (isTrackPlaying())
  {
    Serial.println(F("Skipping: Audio still playing"));
    return;
  }

  Serial.print(F("Playing track on pin: "));
  Serial.println(trackPin);

  digitalWrite(trackPin, LOW);
  pinMode(trackPin, OUTPUT);
  delay(pinDelayMs);
  pinMode(trackPin, INPUT);
}

void initAudioPins()
{
  pinMode(PIN_AUDIO_ACT, INPUT);
  pinMode(PIN_AUDIO_RST, INPUT);
  pinMode(PIN_AUDIO_FINAL, INPUT);
}

void resetAudio()
{
  const unsigned long pinDelayMs = 10;
  const unsigned long postResetDelayMs = 2000;

  Serial.println(F("Audio FX reset"));

  digitalWrite(PIN_AUDIO_RST, LOW);
  pinMode(PIN_AUDIO_RST, OUTPUT);
  delay(pinDelayMs);
  pinMode(PIN_AUDIO_RST, INPUT);

  Serial.println(F("Waiting for Audio FX startup"));

  delay(postResetDelayMs);
}

/**
 * Sensor functions.
 */

void onProxSensor(int idx, int v, int up)
{
  Serial.print(F("Prox. sensor: #"));
  Serial.println(idx);

  progState.isTorchActive[idx] = !progState.isTorchActive[idx];

  if (progState.isTorchActive[idx])
  {
    sensorHistory.push(idx);
  }
}

void initProxSensors()
{
  for (int i = 0; i < PROX_SENSORS_NUM; i++)
  {
    proxSensorsBtn[i]
        .begin(PROX_SENSORS_PINS[i])
        .onPress(onProxSensor, i);
  }
}

/**
 * LED functions.
 */

void initLeds()
{
  for (uint8_t i = 0; i < PROX_SENSORS_NUM; i++)
  {
    ledsTorches[i].begin();
    ledsTorches[i].setBrightness(LED_BRIGHTNESS);
    ledsTorches[i].clear();
    ledsTorches[i].show();
  }

  ledBonfire.begin();
  ledBonfire.setBrightness(LED_BRIGHTNESS);
  ledBonfire.clear();
  ledBonfire.show();
}

/**
 * Program state functions.
 */

bool isSensorSolutionOk()
{
  if (sensorHistory.size() < PROX_SENSORS_NUM)
  {
    return false;
  }

  for (uint8_t i = 0; i < PROX_SENSORS_NUM; i++)
  {
    if (sensorHistory[i] != SENSOR_SOLUTION[i])
    {
      return false;
    }
  }

  return true;
}

void loopBonfire()
{
  if (!progState.isBonfireActive)
  {
    return;
  }

  uint8_t startRed = 255, startGreen = 165, startBlue = 0; // Yellow-Orange
  uint8_t endRed = 255, endGreen = 0, endBlue = 0;         // Pure Red

  for (uint16_t i = 0; i < LED_BONFIRE_NUM; i++)
  {
    float ratio = (float)i / (LED_BONFIRE_NUM - 1);
    float red = startRed + ratio * (endRed - startRed);
    float green = startGreen + ratio * (endGreen - startGreen);
    float blue = startBlue + ratio * (endBlue - startBlue);

    int flicker = random(0, 51) - 10;
    red = constrain(red, 0, 255);
    green = constrain(green + flicker, 0, 255);
    blue = constrain(blue + flicker, 0, 255);

    ledBonfire.setPixelColor(i, Adafruit_NeoPixel::Color(red, green, blue));
  }

  ledBonfire.show();
}

void updateTorchLeds()
{
  for (uint8_t i = 0; i < PROX_SENSORS_NUM; i++)
  {
    if (progState.isTorchActive[i])
    {
      ledsTorches[i].fill(TORCH_COLOR);
    }
    else
    {
      ledsTorches[i].clear();
    }

    ledsTorches[i].show();
  }
}

void updateState()
{
  updateTorchLeds();

  if (!progState.isBonfireActive && isSensorSolutionOk())
  {
    progState.isBonfireActive = true;
    openRelay();
    playTrack(PIN_AUDIO_FINAL);
  }

  loopBonfire();
}

void onStateTimer(int idx, int v, int up)
{
  updateState();
}

void initStateTimer()
{
  timerState
      .begin(STATE_TIMER_MS)
      .repeat(-1)
      .onTimer(onStateTimer)
      .start();
}

/**
 * Entry point.
 */

void setup()
{
  Serial.begin(9600);

  cleanState();
  initProxSensors();
  initLeds();
  initRelay();
  initAudioPins();
  resetAudio();
  initStateTimer();

  Serial.println(F(">> Halloween"));
  Serial.flush();
}

void loop()
{
  automaton.run();
}
