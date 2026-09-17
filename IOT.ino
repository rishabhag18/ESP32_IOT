/**********************************************************************************
 *  TITLE: Techvein - ESP RainMaker (Ultra-Fast Dual-Core)
 *  Architecture: FreeRTOS Core Separation, CSS Toggles, Embedded QR Code
 **********************************************************************************/

#include <EEPROM.h>
#include "RMaker.h"
#include "WiFi.h"
#include "WiFiProv.h"
#include <WebServer.h>

//==================================================
// USER CONFIGURATION
//==================================================

const uint8_t MAX_RELAYS = 8; 
const uint8_t TOTAL_RELAYS = 2;   // Currently testing 2 relays

const bool RELAY_ACTIVE_LOW   = true;  // 1 = ON, 0 = OFF
const bool ENABLE_EEPROM      = true;   

#define EEPROM_SIZE 32

const char *service_name = "Robotics_Lab";
const char *pop          = "Robotics123";
const char *nodeName     = "Robotics_Lab_Node";

const char *local_ap_ssid = "Robotics_Lab_Local";
const char *local_ap_pass = "Robotics123";

WebServer server(80);

const char *deviceName[MAX_RELAYS] = {
  "Board 1 Light", "Board 1 Fan", "Relay 3", "Relay 4", 
  "Relay 5", "Relay 6", "Relay 7", "Relay 8"
};

// Bulletproof 30-Pin GPIO Map (Avoiding 0, 2, 5, 12, 15)
const uint8_t relayPin[MAX_RELAYS]  = { 23, 22, 21, 19, 18, 26, 25, 32 };
const uint8_t switchPin[MAX_RELAYS] = { 13, 14, 27, 33, 4, 16, 17, 34 }; // 16=RX2, 17=TX2

const uint8_t wifiLed    = 2;
const uint8_t gpio_reset = 0;

//==================================================
// GLOBAL VARIABLES
//==================================================

bool relayState[MAX_RELAYS] = {false};

// Cloud Sync Variables
unsigned long lastCloudSync[MAX_RELAYS] = {0};
bool pendingCloudState[MAX_RELAYS] = {false};
bool syncNeeded[MAX_RELAYS] = {false};

// Custom High-Speed Debounce Variables (Running on Core 1)
bool lastPhysicalState[MAX_RELAYS] = {HIGH};
bool stableSwitchState[MAX_RELAYS] = {HIGH};
unsigned long lastDebounceTime[MAX_RELAYS] = {0};

uint8_t relayIndex0 = 0, relayIndex1 = 1, relayIndex2 = 2, relayIndex3 = 3;
uint8_t relayIndex4 = 4, relayIndex5 = 5, relayIndex6 = 6, relayIndex7 = 7;

TaskHandle_t HardwareTask;

//==================================================
// FORWARD DECLARATIONS
//==================================================
void setRelay(uint8_t relayIndex, bool state, bool saveToEEPROM = true);
void updateRainMakerSwitch(uint8_t relay, bool state);
void handleRoot();
void handleToggle();
void write_callback(Device *device, Param *param, const param_val_t val, void *priv_data, write_ctx_t *ctx);
void checkResetButtons();

//==================================================
// PROVISIONING EVENT HANDLER
//==================================================

void sysProvEvent(arduino_event_t *sys_event)
{
  switch (sys_event->event_id)
  {
    case ARDUINO_EVENT_PROV_START:
      Serial.printf("Provisioning Started : %s\n", service_name);
      digitalWrite(wifiLed, LOW);
      break;
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      Serial.println("WiFi Connected to Router");
      digitalWrite(wifiLed, HIGH);
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.println("WiFi Disconnected from Router");
      digitalWrite(wifiLed, LOW);
      break;
    default:
      break;
  }
}

//==================================================
// EEPROM & RELAY FUNCTIONS
//==================================================

void writeEEPROM(uint8_t addr, bool state)
{
  if (!ENABLE_EEPROM) return;
  if (EEPROM.read(addr) != state)
  {
      EEPROM.write(addr, state);
      EEPROM.commit();
  }
}

bool readEEPROM(uint8_t addr)
{
  if (!ENABLE_EEPROM) return false;
  return EEPROM.read(addr);
}

void setRelay(uint8_t relayIndex, bool state, bool saveToEEPROM)
{
  if (relayIndex >= TOTAL_RELAYS) return;
  relayState[relayIndex] = state;
  digitalWrite(relayPin[relayIndex], RELAY_ACTIVE_LOW ? !state : state);
  if (saveToEEPROM) writeEEPROM(relayIndex, state);
}

//==================================================
// PROFESSIONAL HTML WEB PAGE (Toggle Switches)
//==================================================

void handleRoot()
{
  String qrPayload = "{\"ver\":\"v1\",\"name\":\"" + String(service_name) + "\",\"pop\":\"" + String(pop) + "\",\"transport\":\"ble\"}";
  String urlEncodedPayload = "%7B%22ver%22%3A%22v1%22%2C%22name%22%3A%22" + String(service_name) + "%22%2C%22pop%22%3A%22" + String(pop) + "%22%2C%22transport%22%3A%22ble%22%7D";
  
  // The API URL that generates the QR code image
  String qrUrl = "https://api.qrserver.com/v1/create-qr-code/?size=200x200&data=" + urlEncodedPayload;

  String html = "<!DOCTYPE html><html><head>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>Techvein - Dashboard</title>";
  html += "<style>";
  html += "body { font-family: 'Segoe UI', Tahoma, sans-serif; background: #0f172a; color: #f8fafc; text-align: center; margin: 0; padding: 20px; }";
  html += ".container { max-width: 420px; margin: auto; background: #1e293b; padding: 25px; border-radius: 16px; box-shadow: 0 10px 30px rgba(0,0,0,0.6); }";
  html += "h2 { color: #38bdf8; margin-bottom: 2px; letter-spacing: 1px; }";
  html += "p { color: #94a3b8; font-size: 14px; margin-bottom: 25px; }";
  html += ".card { background: #334155; padding: 18px 20px; border-radius: 12px; margin-bottom: 15px; display: flex; justify-content: space-between; align-items: center; box-shadow: inset 0 2px 4px rgba(0,0,0,0.1); }";
  html += ".card span { font-size: 16px; font-weight: 600; }";
  
  // CSS for Professional Toggle Switch
  html += ".switch { position: relative; display: inline-block; width: 56px; height: 30px; }";
  html += ".switch input { opacity: 0; width: 0; height: 0; }";
  html += ".slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #ef4444; transition: .3s; border-radius: 30px; box-shadow: inset 0 2px 5px rgba(0,0,0,0.2); }";
  html += ".slider:before { position: absolute; content: ''; height: 22px; width: 22px; left: 4px; bottom: 4px; background-color: white; transition: .3s; border-radius: 50%; box-shadow: 0 2px 4px rgba(0,0,0,0.3); }";
  html += "input:checked + .slider { background-color: #22c55e; }";
  html += "input:checked + .slider:before { transform: translateX(26px); }";
  
  html += ".provision-box { background: #0f172a; padding: 20px; border-radius: 12px; margin-top: 25px; border: 1px dashed #475569; }";
  html += ".footer { margin-top: 25px; font-size: 12px; color: #64748b; }";
  html += "</style></head><body>";
  html += "<div class='container'>";
  html += "<h2>Techvein</h2>";
  html += "<p>Robotics Lab Offline Dashboard</p>";

  // Generate Toggle Cards
  for (uint8_t i = 0; i < TOTAL_RELAYS; i++) {
    html += "<div class='card'>";
    html += "<span>" + String(deviceName[i]) + "</span>";
    html += "<label class='switch'>";
    html += "<input type='checkbox' onchange=\"window.location.href='/toggle?relay=" + String(i) + "'\" " + (relayState[i] ? "checked" : "") + ">";
    html += "<span class='slider'></span>";
    html += "</label>";
    html += "</div>";
  }

  // Display QR Code Image directly on the page
  html += "<div class='provision-box'>";
  html += "<span style='color:#38bdf8; font-weight:600; display:block; margin-bottom:12px; font-size:15px;'>Scan to Pair with App</span>";
  html += "<img src='" + qrUrl + "' alt='Pairing QR Code' style='width:180px; height:180px; border-radius:10px; padding:10px; background:white;'>";
  html += "<div style='font-size:12px; color:#94a3b8; margin-top:12px; font-weight:bold;'>POP: " + String(pop) + "</div>";
  html += "</div>";
  
  html += "<div class='footer'>Secure Dual-Core Operation Active</div>";
  html += "</div></body></html>";

  server.send(200, "html", html);
}

void handleToggle()
{
  if (server.hasArg("relay")) {
    uint8_t relay = server.arg("relay").toInt();
    if (relay < TOTAL_RELAYS) {
      bool newState = !relayState[relay];
      setRelay(relay, newState);
      pendingCloudState[relay] = newState;
      syncNeeded[relay] = true;
      Serial.printf("Local Web -> Relay %d : %s\n", relay + 1, newState ? "ON" : "OFF");
    }
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

//==================================================
// FREERTOS: HIGH-SPEED HARDWARE TASK (CORE 1)
//==================================================

void hardwareTaskCode(void * parameter) 
{
  for(;;) 
  {
    // Custom Non-Blocking Debounce Scanner
    for (uint8_t i = 0; i < TOTAL_RELAYS; i++) {
      bool currentState = digitalRead(switchPin[i]);
      
      if (currentState != lastPhysicalState[i]) {
        lastDebounceTime[i] = millis();
        lastPhysicalState[i] = currentState;
      }

      if ((millis() - lastDebounceTime[i]) > 50) {
        if (currentState != stableSwitchState[i]) {
          stableSwitchState[i] = currentState;
          
          bool newState = (currentState == LOW);
          
          if (relayState[i] != newState) {
            setRelay(i, newState);
            pendingCloudState[i] = newState;
            syncNeeded[i] = true;
            Serial.printf("Hardware -> Relay %d : %s\n", i + 1, newState ? "ON" : "OFF");
          }
        }
      }
    }
    
    checkResetButtons();
    vTaskDelay(10 / portTICK_PERIOD_MS); // FreeRTOS mandatory yield
  }
}

//==================================================
// HANDLE RESET BUTTONS
//==================================================

void checkResetButtons()
{
  if (digitalRead(gpio_reset) == LOW)
  {
    vTaskDelay(50 / portTICK_PERIOD_MS); 
    if (digitalRead(gpio_reset) == LOW)
    {
      uint32_t startTime = millis();
      while (digitalRead(gpio_reset) == LOW) { vTaskDelay(20 / portTICK_PERIOD_MS); }
      vTaskDelay(50 / portTICK_PERIOD_MS); 
      uint32_t pressDuration = millis() - startTime;

      if (pressDuration >= 10000) {
        Serial.println("Factory Reset");
        RMakerFactoryReset(2);
      } else if (pressDuration >= 3000) {
        Serial.println("WiFi Reset");
        RMakerWiFiReset(2);
      }
    }
  }
}

//==================================================
// RAINMAKER SWITCH OBJECTS
//==================================================

static Switch my_switch1((char *)deviceName[0], &relayIndex0);
static Switch my_switch2((char *)deviceName[1], &relayIndex1);
static Switch my_switch3((char *)deviceName[2], &relayIndex2);
static Switch my_switch4((char *)deviceName[3], &relayIndex3);
static Switch my_switch5((char *)deviceName[4], &relayIndex4);
static Switch my_switch6((char *)deviceName[5], &relayIndex5);
static Switch my_switch7((char *)deviceName[6], &relayIndex6);
static Switch my_switch8((char *)deviceName[7], &relayIndex7);

void restoreRelayStates()
{
  for (uint8_t i = 0; i < TOTAL_RELAYS; i++) {
    setRelay(i, ENABLE_EEPROM ? readEEPROM(i) : false, false);
  }
}

void initializeGPIO()
{
  for (uint8_t i = 0; i < TOTAL_RELAYS; i++) {
    pinMode(relayPin[i], OUTPUT);
    digitalWrite(relayPin[i], RELAY_ACTIVE_LOW ? HIGH : LOW);
    
    if (switchPin[i] == 34) {
      pinMode(switchPin[i], INPUT);
    } else {
      pinMode(switchPin[i], INPUT_PULLUP);
    }
    
    lastPhysicalState[i] = digitalRead(switchPin[i]);
    stableSwitchState[i] = lastPhysicalState[i];
  }
  pinMode(wifiLed, OUTPUT);
  digitalWrite(wifiLed, LOW);
  pinMode(gpio_reset, INPUT_PULLUP);
}

void updateRainMakerSwitch(uint8_t relay, bool state)
{
  switch (relay) {
    case 0: my_switch1.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, state); break;
    case 1: my_switch2.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, state); break;
    case 2: my_switch3.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, state); break;
    case 3: my_switch4.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, state); break;
    case 4: my_switch5.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, state); break;
    case 5: my_switch6.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, state); break;
    case 6: my_switch7.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, state); break;
    case 7: my_switch8.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, state); break;
  }
}

//==================================================
// INITIALIZE RAINMAKER & LOCAL AP/WEB SERVER
//==================================================

void initializeRainMaker()
{
  Node my_node = RMaker.initNode(nodeName);

  if (TOTAL_RELAYS >= 1) { my_switch1.addCb(write_callback); my_node.addDevice(my_switch1); }
  if (TOTAL_RELAYS >= 2) { my_switch2.addCb(write_callback); my_node.addDevice(my_switch2); }

  RMaker.enableOTA(OTA_USING_PARAMS);
  RMaker.enableTZService();
  RMaker.enableSchedule();
  
  WiFi.onEvent(sysProvEvent);
  RMaker.start();

  WiFiProv.beginProvision(NETWORK_PROV_SCHEME_BLE, NETWORK_PROV_SCHEME_HANDLER_FREE_BTDM, NETWORK_PROV_SECURITY_1, pop, service_name);

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(local_ap_ssid, local_ap_pass);
  Serial.printf("Local Access Point Started: %s (IP: 192.168.4.1)\n", local_ap_ssid);

  server.on("/", handleRoot);
  server.on("/toggle", handleToggle);
  server.begin();
  Serial.println("Local Web Server Started");

  for (uint8_t i = 0; i < TOTAL_RELAYS; i++) {
    updateRainMakerSwitch(i, relayState[i]);
  }
}

void write_callback(Device *device, Param *param, const param_val_t val, void *priv_data, write_ctx_t *ctx)
{
  if (strcmp(param->getParamName(), ESP_RMAKER_DEF_POWER_NAME) != 0) return;
  bool state = val.val.b;
  uint8_t relay = *(uint8_t *)priv_data;

  if (relay >= TOTAL_RELAYS) return;

  setRelay(relay, state);
  updateRainMakerSwitch(relay, state);
  Serial.printf("App -> %s : %d\n", device->getDeviceName(), state);
}

//==================================================
// SETUP (CORE 0)
//==================================================

void setup()
{
  Serial.begin(115200);
  if (ENABLE_EEPROM) EEPROM.begin(EEPROM_SIZE);

  initializeGPIO();
  restoreRelayStates();
  initializeRainMaker();
  
  // Launch the FreeRTOS Hardware Task strictly pinned to Core 1
  xTaskCreatePinnedToCore(
    hardwareTaskCode, 
    "HardwareTask", 
    4096, 
    NULL, 
    1, 
    &HardwareTask, 
    1
  );
  Serial.println("FreeRTOS Dual-Core Separation Active");
  Serial.println("Hardware & Local Server Initialized Successfully");
}

//==================================================
// MAIN LOOP (CORE 0 - Network Operations Only)
//==================================================

void loop()
{
  // 1. Process Web Server Requests
  server.handleClient();

  // 2. Commercial Cloud Rate-Limiter Queue
  for (uint8_t i = 0; i < TOTAL_RELAYS; i++) {
    if (syncNeeded[i] && (millis() - lastCloudSync[i] > 1000)) {
      updateRainMakerSwitch(i, pendingCloudState[i]);
      lastCloudSync[i] = millis();
      syncNeeded[i] = false; 
    }
  }

  // 3. Background Wi-Fi Watchdog
  static uint32_t lastWiFiCheck = 0;
  if (millis() - lastWiFiCheck > 10000) {
    if (WiFi.SSID().length() > 0 && WiFi.status() != WL_CONNECTED) {
      Serial.println("Watchdog: Wi-Fi dropped. Reconnecting...");
      WiFi.reconnect();
    }
    lastWiFiCheck = millis();
  }
}
