#include "rdm630.h"
#include <Automaton.h>
//#include <Atm_servo.h>
#include <Adafruit_NeoPixel.h>

/**
 * Relay.
 */

const byte RELAY_PIN = 11;

/**
 * RFID readers.
 */

const byte NUM_READERS = 4;

// RX, TX

RDM6300 rfid01(2, 3);
RDM6300 rfid02(4, 5);
RDM6300 rfid03(6, 7);
RDM6300 rfid04(8, 9);

RDM6300 rfidReaders[NUM_READERS] = {
    rfid01,
    rfid02,
    rfid03,
    rfid04};

const unsigned int EMPTY_TOLERANCE = 1;

String currentTags[NUM_READERS];

String validTags[NUM_READERS] = {
    "2B00463A2100",
    "1D0027F79300",
    "2B0045A3D800",
    "3C00D5846F00"};

/**
 * LED.
 */

const int LED_BRIGHTNESS = 150;
const int LED_NUM = 40;
const int LED_PIN = 12;

Adafruit_NeoPixel ledStrip = Adafruit_NeoPixel(
    LED_NUM,
    LED_PIN,
    NEO_GRB + NEO_KHZ800);

const uint32_t LED_COLOR = Adafruit_NeoPixel::Color(250, 0, 250);
const uint32_t LED_COLOR_ERROR = Adafruit_NeoPixel::Color(250, 0, 0);

const int LED_TIMER_MS = 30;
const int LED_BLOB_LEN = 5;
const int LED_ERROR_ITERS = 10;
const int LED_ERROR_DELAY_MS = 100;

Atm_timer timerLed;

/**
 * Servo.
 */

const int SERVO_PIN = 10;
const int SERVO_STEP_SIZE = 150;
const int SERVO_STEP_TIME = 0;
const int SERVO_TIMER_MS = 60;
const int SERVO_POS_HI = 20;
const int SERVO_POS_LO = 150;

//Atm_servo servo;
//Atm_timer timerServo;

/**
 * Program state.
 */

typedef struct programState
{
    bool isRelayOpen;
    bool isServoEnabled;
    bool isRfidEnabled;
    bool isLedBlobEnabled;
    int currLedIndex;
    unsigned int emptyReadCount[NUM_READERS];
} ProgramState;

ProgramState progState = {
    .isRelayOpen = false,
    .isServoEnabled = false,
    .isRfidEnabled = true,
    .isLedBlobEnabled = false,
    .currLedIndex = 0,
    .emptyReadCount = {0, 0, 0, 0}};

void enableEffects()
{
    progState.isServoEnabled = true;
    progState.isLedBlobEnabled = true;
    progState.isRfidEnabled = false;
}

void disableEffects()
{
    progState.isServoEnabled = false;
    progState.isLedBlobEnabled = false;
    progState.isRfidEnabled = true;
}

void updateState()
{
    const int effectDelayMs = 5000;

    bool defined = areTagsDefined();
    bool valid = areTagsValid();

    if (defined && valid && !progState.isRelayOpen)
    {
        Serial.println(F("State :: Defined && Valid && !Open"));

        enableEffects();
        unsigned long endMillis = millis() + effectDelayMs;

        while (millis() < endMillis)
        {
            automaton.run();
        }

        disableEffects();
        emptyTags();
        openRelay();
    }
    else if (defined && !valid && !progState.isRelayOpen)
    {
        Serial.println(F("State :: Defined && !Valid && !Open"));
        emptyTags();
        showLedError();
    }
    else if (!valid && progState.isRelayOpen)
    {
        Serial.println(F("State :: !Valid && Open"));
        emptyTags();
        lockRelay();
    }
}

/**
 * Servo functions.
 */

//void onServoTimer(int idx, int v, int up)
//{
//    if (!progState.isServoEnabled)
//    {
//        return;
//    }
//
//    int servoPos = up % 2 == 0 ? SERVO_POS_HI : SERVO_POS_LO;
//
//    Serial.print(F("Servo: "));
//    Serial.println(servoPos);
//
//    servo.position(servoPos);
//}
//
//void initServo()
//{
//    servo
//        .begin(SERVO_PIN)
//        .step(SERVO_STEP_SIZE, SERVO_STEP_TIME);
//
//    timerServo.begin(SERVO_TIMER_MS)
//        .repeat(-1)
//        .onTimer(onServoTimer)
//        .start();
//}

/**
 * RFID functions.
 */

void emptyTags()
{
    for (int i = 0; i < NUM_READERS; i++)
    {
        currentTags[i] = "";
    }
}

void initRfidReaders()
{
    for (int i = 0; i < NUM_READERS; i++)
    {
        rfidReaders[i].begin();
    }
}

void pollRfidReaders()
{
    String tagId;

    for (int i = 0; i < NUM_READERS; i++)
    {
        tagId = rfidReaders[i].getTagId();

        if (tagId.length())
        {
            progState.emptyReadCount[i] = 0;
        }
        else if (progState.emptyReadCount[i] <= EMPTY_TOLERANCE)
        {
            progState.emptyReadCount[i] += 1;
        }

        if (!tagId.length() &&
            currentTags[i].length() &&
            progState.emptyReadCount[i] <= EMPTY_TOLERANCE)
        {
            Serial.print(F("Ignoring empty read on reader: "));
            Serial.println(i);
            continue;
        }

        currentTags[i] = tagId;
    }
}

bool areTagsDefined()
{
    for (int i = 0; i < NUM_READERS; i++)
    {
        if (!isTagDefined(i))
        {
            return false;
        }
    }

    return true;
}

bool isTagDefined(int idx)
{
    return currentTags[idx].length() > 0;
}

bool areTagsValid()
{
    for (int i = 0; i < NUM_READERS; i++)
    {
        if (!isTagDefined(i))
        {
            return false;
        }

        if (validTags[i].compareTo(currentTags[i]) != 0)
        {
            return false;
        }
    }

    return true;
}

void printCurrentTags()
{

    Serial.print(F("## Current tags :: "));
    Serial.println(millis());

    for (int i = 0; i < NUM_READERS; i++)
    {
        Serial.print(i);
        Serial.print(F(" :: "));
        Serial.println(currentTags[i]);
    }
}

/**
 * LED functions.
 */

void showLedError()
{
    if (progState.isLedBlobEnabled)
    {
        Serial.println(F("Skipping: LED blob is enabled"));
        return;
    }

    for (int i = 0; i < LED_ERROR_ITERS; i++)
    {
        ledStrip.fill(LED_COLOR_ERROR);
        ledStrip.show();

        delay(LED_ERROR_DELAY_MS);

        ledStrip.clear();
        ledStrip.show();

        delay(LED_ERROR_DELAY_MS);
    }

    ledStrip.clear();
    ledStrip.show();
}

void onLedTimer(int idx, int v, int up)
{
    if (!progState.isLedBlobEnabled)
    {
        return;
    }

    progState.currLedIndex = (progState.currLedIndex + 1) %
                             (LED_NUM - LED_BLOB_LEN);

    ledStrip.clear();

    for (int i = 0; i < LED_BLOB_LEN; i++)
    {
        ledStrip.setPixelColor(
            progState.currLedIndex + i,
            LED_COLOR);
    }

    ledStrip.show();
}

void initLed()
{
    ledStrip.begin();
    ledStrip.setBrightness(LED_BRIGHTNESS);
    ledStrip.clear();
    ledStrip.show();

    timerLed.begin(LED_TIMER_MS)
        .repeat(-1)
        .onTimer(onLedTimer)
        .start();
}

/**
 * Relay functions.
 */

void lockRelay()
{
    digitalWrite(RELAY_PIN, LOW);
    progState.isRelayOpen = false;
}

void openRelay()
{
    digitalWrite(RELAY_PIN, HIGH);
    progState.isRelayOpen = true;
}

void initRelay()
{
    pinMode(RELAY_PIN, OUTPUT);
    lockRelay();
}

/**
 * Entrypoint.
 */

void setup()
{
    Serial.begin(9600);

    initRfidReaders();
    initRelay();
    initLed();

    Serial.println(F(">> Starting blender program"));
}

void loop()
{
    if (progState.isRfidEnabled)
    {
        pollRfidReaders();
        printCurrentTags();
    }
    else
    {
        emptyTags();
    }

    updateState();
}
