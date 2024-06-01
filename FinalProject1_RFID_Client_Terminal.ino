/* Required external libraries (download from Library Manager)
    - MFRC522 by Github Community
    - ArduinoJson by Benoit Blanchon
*/

/* What to do before running this code 
    - Build the client terminal circuit correctly according to the diagram (see pinned messages)
    - Run the FinalProject1_RFID_Tag_Configuation_Updated.ino file to properly configure the RFID tag (see pinned messages)
    - Run the FinalProject1_RFID_Server.py file, preferably on another machine
*/

#include <SPI.h>
#include <MFRC522.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include "driver/timer.h"
#include "driver/gpio.h"

const char* ssid        = "TP-Link_7E7C"; //change to your Wi-Fi SSID
const char* password    = "84457875"; // change to your Wi-Fi password
const char* serverIp    = "192.168.0.26"; // change to your server IP address
const int serverPort    = 8000;

const int RST_PIN       = 3;
const int SS_PIN        = 21;
const int MODE_SWITCH   = 26;
const int loadModeLED   = 25;
const int payModeLED    = 33;
const int successLED    = 14;
const int failLED       = 27;
const int builtInLED    = 2; // it's fine if your ESP32 doesn't have a built-in LED

byte device_id_block    = 1;                                          
byte balance_block      = 2;
byte balance[16];
byte idReadBlock[18];
byte balReadBlock[18];
byte idLen              = 16;
byte blLen              = 16;
byte idrbLen            = 18;
byte blrbLen            = 18;

int serverReplyIndex;
int currSwitchState     = 1;
int n1                  = 1;
int transactions        = 0;

bool switchFlag         = true;
bool checkFlag          = false;

char serverReply[86];

timer_config_t timer_config = {
  .alarm_en = TIMER_ALARM_EN,
  .counter_en = TIMER_PAUSE,
  .intr_type = TIMER_INTR_LEVEL,
  .counter_dir = TIMER_COUNT_UP,
  .auto_reload = TIMER_AUTORELOAD_EN,
  .divider = 80
};

MFRC522 mfrc522(SS_PIN, RST_PIN);
WiFiClient client;

static bool IRAM_ATTR checkConnection(void * args) {
  checkFlag = true;
  return false;
}

void setup() {
  Serial.begin(115200);
  SPI.begin();   
  mfrc522.PCD_Init();

  pinMode(MODE_SWITCH, INPUT_PULLUP);
  pinMode(payModeLED, OUTPUT);
  pinMode(loadModeLED, OUTPUT);
  pinMode(successLED, OUTPUT);
  pinMode(failLED, OUTPUT);
  pinMode(builtInLED, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(MODE_SWITCH), switchMode, CHANGE);

  timer_init(TIMER_GROUP_0, TIMER_0, &timer_config);
  timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, 30000000);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  
  while (!client.connected()) {
    if (client.connect(serverIp, serverPort)) {
      Serial.println("Connected to server");
    } else {
      Serial.println("Connection to server failed. Retrying...");
      delay(2000);
    }
  }
  
  digitalWrite(builtInLED, HIGH);

  xTaskCreate(second_thread, "Transaction Counter", 1024, (void*)&n1, 1, NULL);
  timer_start(TIMER_GROUP_0, TIMER_0);
  timer_isr_callback_add(TIMER_GROUP_0, TIMER_0, checkConnection, NULL, 0);
}

void loop() {
  MFRC522::MIFARE_Key key;
  MFRC522::StatusCode status;
  StaticJsonDocument<128> res;
  StaticJsonDocument<128> server_json;

  if(switchFlag) {
    currSwitchState = digitalRead(MODE_SWITCH);
    if(currSwitchState == 0)
      currSwitchState = 1;
    else
      currSwitchState = 0;

    changeModeLED(currSwitchState);
    switchFlag = false;
  }
  else{
    currSwitchState = digitalRead(MODE_SWITCH);
  }

  if(checkFlag) {
    if(client.connected())
      Serial.println("Connection to server: ACTIVE");
    else{
      Serial.println("Connection to server: TERMINATED");
      Serial.println("Restarting connection in 5 seconds...");
      digitalWrite(builtInLED, LOW);
      delay(5000);
      ESP.restart();
    }
  }
  checkFlag = false;
  
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;

  if ( ! mfrc522.PICC_IsNewCardPresent()) {
    return;
  }

  if ( ! mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  //###################### START OF READ BLOCKS ###############################

  /* READ BLOCK 1: Reading "device_id" from the RFID tag */
  //-------------------------------------------

  status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, device_id_block, &key, &(mfrc522.uid)); //line 834 of MFRC522.cpp file
  if (status != MFRC522::STATUS_OK) {
    //Serial.print(F("PCD_Authenticate() for read failed: "));
    //Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  }

  status = mfrc522.MIFARE_Read(device_id_block, idReadBlock, &idrbLen);
  if (status != MFRC522::STATUS_OK) {
    //Serial.print(F("MIFARE_Read() failed: "));
    //Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  }
  //else Serial.println(F("MIFARE_Read() of device_id successful: "));
  //-------------------------------------------

  /* READ BLOCK 2: Reading "balance" from the RFID tag */
  //-------------------------------------------

  status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, balance_block, &key, &(mfrc522.uid)); //line 834 of MFRC522.cpp file
  if (status != MFRC522::STATUS_OK) {
    //Serial.print(F("PCD_Authenticate() for read failed: "));
    //Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  }

  status = mfrc522.MIFARE_Read(balance_block, balReadBlock, &blrbLen);
  if (status != MFRC522::STATUS_OK) {
    //Serial.print(F("MIFARE_Read() failed: "));
    //Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  }
  //else Serial.println(F("MIFARE_Read() of balance successful: "));
  //-------------------------------------------

  //###################### END OF READ BLOCKS ###############################

  res["device_id"] = idReadBlock;
  res["balance"] = balReadBlock;

  if(currSwitchState == 0)
    res["command"] = "load";
  else
    res["command"] = "pay";

  String message_to_server;
  serializeJson(res, message_to_server);
  Serial.print("Message TO server: ");
  Serial.println(message_to_server);

  const char* charArray = message_to_server.c_str();
  client.write(charArray);

  delay(3000);

  if(!client.connected()){
    Serial.println("Cannot complete transaction due to server disconnection");
    Serial.println("Restarting connection in 5 seconds...");
    digitalWrite(builtInLED, LOW);
    delay(5000);
    ESP.restart();
  }

  serverReplyIndex = 0;

  while (client.available()>0) {
    char c = client.read();
    serverReply[serverReplyIndex] = c;
    serverReplyIndex++;
  }

  client.flush();

  Serial.print("Message FROM server: ");
  Serial.println(serverReply);
  deserializeJson(server_json, serverReply);

  String replyID = server_json["device_id"];
  String replyComm = server_json["command"];
  String replyBal = server_json["updated_balance"];
  byte replyBalBlock[16];
  replyBal.getBytes(replyBalBlock, 16);

  if(replyComm == "S") {
    digitalWrite(successLED, HIGH);
    Serial.println("Transaction SUCCESSFUL");
  }
  else {
    digitalWrite(failLED, HIGH);
    Serial.println("Transaction FAILED");
  }

  //###################### START OF BALANCE UPDATE BLOCK ###############################

  status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, balance_block, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
    //Serial.print(F("PCD_Authenticate() failed: "));
    //Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  }
  //else Serial.println(F("PCD_Authenticate() successful: "));

  status = mfrc522.MIFARE_Write(balance_block, replyBalBlock, 16);
  if (status != MFRC522::STATUS_OK) {
    //Serial.print(F("MIFARE_Write() failed: "));
    //Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  }
  //else Serial.println(F("MIFARE_Write() of updated_balance successful: "));

  //###################### END OF BALANCE UPDATE BLOCK ###############################

  //###################### START OF UPDATED BALANCE READ BLOCK ###############################

  status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, balance_block, &key, &(mfrc522.uid)); //line 834 of MFRC522.cpp file
  if (status != MFRC522::STATUS_OK) {
    //Serial.print(F("PCD_Authenticate() for read failed: "));
    //Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  } 
  //else Serial.println(F("PCD_Authenticate() successful: "));

  status = mfrc522.MIFARE_Read(balance_block, balReadBlock, &blrbLen);
  if (status != MFRC522::STATUS_OK) {
    //Serial.print(F("MIFARE_Read() failed: "));
    //Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  }
  //else Serial.println(F("MIFARE_Read() of updated_balance successful: "));

  //###################### END OF UPDATED BALANCE READ BLOCK ###############################

  Serial.print("Updated Balance: ");
  for (uint8_t i = 0; i < 15; i++)
    Serial.write(balReadBlock[i]);
  Serial.println(" ");

  delay(3000);

  if(replyComm == "S") {
    digitalWrite(successLED, LOW);
    transactions++;
  }
  else
    digitalWrite(failLED, LOW);

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();

}

void switchMode() {
  switchFlag = true;
}

void changeModeLED(int switchState) {
  if(switchState == 0) { 
    digitalWrite(payModeLED, HIGH);
    digitalWrite(loadModeLED, LOW);
  }
  else {                
    digitalWrite(payModeLED, LOW);
    digitalWrite(loadModeLED, HIGH);
  }
}

void second_thread(void *thread_num) {
  while (1){
    Serial.print("Number of transactions: ");
    Serial.println(transactions);
    vTaskDelay(10000/portTICK_PERIOD_MS);
  }

}
