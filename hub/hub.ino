//#include "capstone_settings.h" // we can include a setting file specially made for this script, else compliler will use the default settings
#include <custom_capstone_lib.h>

int resend_counter=10;
DynamicJsonDocument data_json(DEFAULT_DOC_SIZE);



void setup() {
  Serial.begin(115200);
  printlnd("Hub:");
  //Set device in STA mode to begin with
  WiFi.mode(WIFI_AP);

  // This is the mac address of the Master in Station Mode
  printd("STA MAC: "); printlnd(WiFi.macAddress());
   
  
  data_json=init_doc(data_json);

  serializeJson(data_json, Serial);
  printlnd();
  
  // Init ESPNow with a fallback logic
  InitESPNow();
  // Once ESPNow is successfully Init, we will register for recv CB to
  // get recv packer info.
  esp_now_register_recv_cb(OnDataRecv);
  esp_now_register_send_cb(OnDataSent);
}


  
// callback when data is recv from Master
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
  printlnd("Data Recived!");printlnd("Data Recived!");printlnd("Data Recived!");printlnd("Data Recived!");
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
           
  DynamicJsonDocument message_doc(400);
    message_doc=decode_espnow_message(data, data_len);
  printd("message_doc: ");
  json_printd(message_doc);
  printlnd();
  String message = message_doc["json"].as<String>();
  printlnd(message);
  
  if (resend_counter<0){
      resend_counter = 10;
    }
  
  if (resend_counter>0){
    if (check_package_hash(message_doc)){
        DeserializationError err =  deserializeJson(message_doc, message);
        printd("message_doc: ");
        json_printd(message_doc); printlnd();
        if (err) {
          printd(F("deserializeJson() failed: "));
          printlnd(err.c_str());
          }
        
        resend_counter=0;
      } else {
        printlnd("Asking for resend...");
        ask_for_resend(mac_addr,data_json);
        return;
      }
    resend_counter--;
  } 
  
  printd("Last Packet Recv from: "); printlnd(macStr);
  if (resend_counter==0){
    printlnd("Error: ckecksum not matching");
    printd("Last Packet Recv Data: "); 
    printlnd(*data);
    printlnd("");
    } else {
    printd("Decoded Message:"); 
    json_printd(message_doc);
    printlnd("");
    printlnd("");
    json_printd(message_doc["peers"]["hub"]);
    printlnd("");
    json_printd(message_doc["header"]["DEVICE_ID"]);
    printlnd("");
    resend_counter = 10;
   }  
   
  String command = message_doc["command"].as<String>();
  int sender_device_type = message_doc["header"]["DEVICE_TYPE"].as<int>();
  String sender_DEVICE_ID = message_doc["header"]["DEVICE_ID"].as<String>();

  if (sender_device_type == 0){
  //    Should not be talking to another hub
  } else if (sender_device_type == 1){
  //    Devices are sensors, save the data
      data_json["children"][sender_DEVICE_ID]["header"] = message_doc["header"];
      data_json["children"][sender_DEVICE_ID]["data"] = message_doc["data"];
      data_json["children"][sender_DEVICE_ID]["data"]["timestamp"] = millis();

      printlnd("current data_json:");
      json_printd(data_json);
  } else {
  //    some thing went wrong, ask for clarification  
    printd("Cannot understand data: "); 
    ask_for_resend(mac_addr,data_json);
  }

  if (command.indexOf("A")>=0){
//    Do A
  } else if (command.indexOf("B")>=0){
//    Do B
  } else if (command.indexOf("Callback")>=0){
      data_json["command"] = "Callback";
      printd("Sending commands to: "); printlnd(*mac_addr);
      sendData(mac_addr, package_json(data_json));
      printlnd("Commands sent");
  } else {
//    some thing went wrong, ask for clarification  
    printd("Cannot understand command: "); 
//    printlnd(message_doc["command"]);
 
    command_clarification(mac_addr,data_json);
  }

}

// callback when data is sent from Master to Slave
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  printd("Last Packet Sent to: "); printlnd(macStr);
  printd("Last Packet Send Status: "); printlnd(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

esp_now_peer_info_t slaves[NUMSLAVES] = {};
int SlaveCnt = 0; 
int loop_counter = 0;
int push_counter = 0;
void loop() {
  data_json["header"]["LOCAL_TIME"] = millis();
  printd("Loop number "); printlnd(loop_counter);
  // In the loop we scan for slave
  *slaves = ScanForSlave();
  // If Slave is found, it would be populate in `slave` variable
  // We will check if `slave` is defined and then we proceed further
  SlaveCnt=count_slaves(slaves);
  // TODO: implement eeprom 
  printd("Found "); printd(SlaveCnt); printlnd(" slave(s)");
  printd("Slave addr = "); printlnd(*(slaves[0].peer_addr));
  if (SlaveCnt > 0 & loop_counter>2) { // check if slave channel is defined
    loop_counter = 0;
    // `slave` is defined
    // Add slave as peer if it has not been added already
    manageSlave(slaves);
    // pair success or already paired
    // Send data to device
    for (int i = 0; i < SlaveCnt; i++) {
      data_json["command"] = "Callback";
//      printd("Sending commands to: "); printlnd(*(slaves[i].peer_addr));
//      sendData(slaves[i].peer_addr, package_json(data_json));

      printd("Sending commands to: "); printlnd(*(slaves[i].peer_addr));
      sendData(slaves[i].peer_addr, package_json(data_json));
      printlnd("Commands sent");
    }
  } else {
    // No slave found to process
  }
  if (push_counter>5){
    serializeJson(data_json, Serial);Serial.println("");
    push_counter=0;
    }
  printlnd("Loop End\n");
  loop_counter++;
  push_counter++;
}
