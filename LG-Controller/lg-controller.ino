#define MY_DEBUG

#define MY_REPEATER_FEATURE

#define MY_RADIO_RF24
#define MY_RF24_CE_PIN 7
#define MY_RF24_CS_PIN 8
#define MY_RF24_PA_LEVEL RF24_PA_MAX

#include <string.h>
#include <stdlib.h>
#include <MySensors.h>
#include <NeoSWSerial.h>

#define CHILD_ID_POWER 0
#define CHILD_ID_INPUT 1
#define CHILD_ID_VOLUME 2
#define CHILD_ID_CHANNEL 3

MyMessage msgPower(CHILD_ID_POWER, V_STATUS);
MyMessage msgSource(CHILD_ID_INPUT, V_TEXT);
MyMessage msgMute(CHILD_ID_VOLUME, V_STATUS);
MyMessage msgVolume(CHILD_ID_VOLUME, V_PERCENTAGE);
MyMessage msgChannel(CHILD_ID_CHANNEL, V_TEXT);

NeoSWSerial mySerial(3, 2); // RX, TX

const int setId = 1; 

unsigned long intervalMillis = 5 * 1000; // Time before querying TV status
unsigned long timeoutMillis  = 5 * 1000; // Timeout waiting for mySerial response
unsigned long previousMillis = 0; // last time update

void presentation() {
  sendSketchInfo("LgController", "1.1");

  present(CHILD_ID_POWER, S_BINARY);
  present(CHILD_ID_INPUT, S_INFO);
  present(CHILD_ID_VOLUME, S_DIMMER);
  present(CHILD_ID_CHANNEL, S_INFO);
}

void setup() {
  send(msgSource.set("HDMI 1"));
  send(msgChannel.set("2.2"));
#ifdef MY_DEBUG
  Serial.begin(115200);
#endif

  mySerial.begin(9600);
  mySerial.setTimeout(1000);
  mySerial.flush();

  unsigned long serialMillis;

#ifdef MY_DEBUG
  Serial.println("Get initial state");
#endif
  sendStateInfo();
#ifdef MY_DEBUG
  Serial.println("Setup complete");
#endif
}

void loop() {
  // Alway process incoming messages whenever possible
  if (mySerial.available() > 0) {
    String ackString = mySerial.readStringUntil('x');

#ifdef MY_DEBUG
    Serial.print("Received Ack: ");
    Serial.println(ackString);
#endif
  }

  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis > intervalMillis) {
    previousMillis = currentMillis;
    sendStateInfo();
  }
}

void receive(const MyMessage &message) {
  if (message.getSensor() == CHILD_ID_POWER) {
    setPower(message.getBool());
    saveState(message.getSensor(), message.getBool());
  }

  if (message.getSensor() == CHILD_ID_VOLUME) {
    if (message.getType() == V_PERCENTAGE) {
      setVolume(message.getInt());
      saveState(message.getSensor(), message.getInt());
    }
    if (message.getType() == V_STATUS) {
      setMute(!message.getBool());
    }
  }

  if (message.getSensor() == CHILD_ID_CHANNEL) {
    char channel[6];
    message.getString(channel);
    const char delim[1] = {'.'};
    char *major, *minor;
    major = strtok(channel, delim);
    minor = strtok(NULL, delim);
#ifdef MY_DEBUG
    Serial.print("ATSC Major: "); Serial.println(major);
    Serial.print("ATSC Minor: "); Serial.println(minor);
#endif
    setATSC(atoi(major), atoi(minor));
  }

  if (message.getSensor() == CHILD_ID_INPUT) {
#ifdef MY_DEBUG
    Serial.println(message.getString());
#endif
  }
}

void sendStateInfo() {
  // getChannel();
  bool pwred_on = getPower();
  Serial.println(pwred_on);
  send(msgPower.set(pwred_on));
  if (pwred_on) {
    Serial.println("Powered on, get states...");
    send(msgVolume.set(getVolume()));
    send(msgMute.set(getMute()));
  }
}

bool getPower() {
  bool state = getStateInt('k', 'a') == 1;
#ifdef MY_DEBUG
  Serial.print("Power state: ");
  Serial.println(state);
#endif
  return state;
}

void setPower(bool pwrOn) {
  // Data 00: Power Off
  // Data 01: Power On
  sendCommand('k', 'a', pwrOn ? 1 : 0);
}

int getVolume() {
  int level = getStateInt('k', 'f');
#ifdef MY_DEBUG
  Serial.print("Volume level: ");
  Serial.println(level);
#endif
  return level;
}

void setVolume(int level) {
  // Data Min: 00 ~ Max: 64 (*transmit by Hexadecimal code)
  sendCommand('k', 'f', max(0, min(level, 100)));
}

bool getMute() {
  bool state = getStateInt('k', 'e') == 1;
#ifdef MY_DEBUG
  Serial.print("Mute state: ");
  Serial.println(state);
#endif
  return state;
}

void setMute(bool mute) {
  // Data 00: Volume mute on (Volume off)
  // Data 01: Volume mute off (Volume on)
  sendCommand('k', 'e', mute ? 0 : 1);
}


void getChannel() {
  char data[16];
  getState('m', 'a', data);
#ifdef MY_DEBUG
  Serial.println(data);
#endif
}

void setATSC(int major, int minor) {
  int data[6] = {0, 0, major, 0, minor, 34};
  sendCommand('m', 'a', data);
}

int getStateInt(char cmd1, char cmd2) {
  char value[16];
  getState(cmd1, cmd2, value);
  return strtol(value, NULL, 16);
}

void getState(char cmd1, char cmd2, char* state) {
  // Send status request command
  sendCommand(cmd1, cmd2, 0xFF);
  wait(250); // Wait for buffer to fill

  unsigned long startMillis = millis();

  // Wait until data exists or timeout
  while (!mySerial.available() && millis() - startMillis < timeoutMillis);

  while (mySerial.available() > 0) {
    char recv[32];
    char *cmd, *setId, *data;
    
    // Read and terminate string
    size_t len = mySerial.readBytesUntil('x', recv, 32);
    recv[len] = '\0';
    
#ifdef MY_DEBUG
    Serial.print("Received: ");
    Serial.println(recv);
#endif
    cmd = strtok(recv, " ");
    setId = strtok(NULL, " ");
    data = strtok(NULL, " ");

#ifdef MY_DEBUG
    Serial.print("cmd: "); Serial.println(cmd);
    Serial.print("setId: "); Serial.println(setId);
    Serial.print("data: "); Serial.println(data);
#endif

    // If 'OK' was received, return the value
    if (strstr(data, "OK") != NULL) {
      strncpy(state, data + 2, 16);
#ifdef MY_DEBUG
      Serial.print("Got OK: "); Serial.println(data);
      Serial.print("State Value: "); Serial.println(state);
#endif
    }
  }
}

void serialPrint(Stream &port, char cmd1, char cmd2, int data[6]) {
  port.print(cmd1);
  port.print(cmd2);
  port.print(' ');
  port.print(setId, HEX);
  for (int i = 0; i < 6; i++) {
    if (data[i] != -1) {
      port.print(' ');
      port.printf("%02X", data[i]);
    }
  }  
  port.print('\r');
}

void sendStatusRequest(char cmd1, char cmd2) {
  sendCommand(cmd1, cmd2, 0xFF);
}

void sendCommand(char cmd1, char cmd2, int data[6]) {
#ifdef MY_DEBUG
  Serial.print("Send Command: ");
  serialPrint(Serial, cmd1, cmd2, data);
  Serial.print("\n");
#endif
  serialPrint(mySerial, cmd1, cmd2, data);
}

void sendCommand(char cmd1, char cmd2, int data) {
  int data_arr[6] = {data, -1, -1, -1, -1, -1};

  sendCommand(cmd1, cmd2, data_arr);
}