#include <SoftwareSerial.h>
#include <SerialRFID.h>

const byte RX_PIN = 12;
const byte TX_PIN = 13;

const byte RELAY_PIN_1 = 11;
const byte RELAY_PIN_2 = 10;
const byte RELAY_PIN_3 = 9;

SoftwareSerial sSerial(RX_PIN, TX_PIN);
SerialRFID rfid(sSerial);

char tag[SIZE_TAG_ID];

char tagBoiler01[SIZE_TAG_ID] = "5C00CAC9633C";
char tagBoiler02[SIZE_TAG_ID] = "10001B96A73A";

char tagGate01[SIZE_TAG_ID] = "5C00CB17C444";
char tagGate02[SIZE_TAG_ID] = "15002E953F91";

char tagStorm01[SIZE_TAG_ID] = "0C007E195E35";
char tagStorm02[SIZE_TAG_ID] = "10001B02ABA2";

void openBoiler() {
  Serial.println("Opening boiler room");
  digitalWrite(RELAY_PIN_1, HIGH);
}

void openGate() {
  Serial.println("Opening gate");
  digitalWrite(RELAY_PIN_2, HIGH);
}

void openStormRoom() {
  Serial.println("Opening storm room");
  digitalWrite(RELAY_PIN_3, HIGH);
}

void initRelays() {
  pinMode(RELAY_PIN_1, OUTPUT);
  pinMode(RELAY_PIN_2, OUTPUT);
  pinMode(RELAY_PIN_3, OUTPUT);

  digitalWrite(RELAY_PIN_1, LOW);
  digitalWrite(RELAY_PIN_2, LOW);
  digitalWrite(RELAY_PIN_3, LOW);
}

void setup() {
  Serial.begin(9600);
  sSerial.begin(9600);

  initRelays();

  Serial.println(">> Starting Earth Globe program");
}

void loop() {
  if (rfid.readTag(tag, sizeof(tag))) {
    Serial.print("Tag: ");
    Serial.println(tag);

    if (SerialRFID::isEqualTag(tag, tagBoiler01) ||
        SerialRFID::isEqualTag(tag, tagBoiler02)) {
      openBoiler();
    } else if (SerialRFID::isEqualTag(tag, tagGate01) ||
               SerialRFID::isEqualTag(tag, tagGate02)) {
      openGate();
    } else if (SerialRFID::isEqualTag(tag, tagStorm01) ||
               SerialRFID::isEqualTag(tag, tagStorm02)) {
      openStormRoom();
    }
  }
}
