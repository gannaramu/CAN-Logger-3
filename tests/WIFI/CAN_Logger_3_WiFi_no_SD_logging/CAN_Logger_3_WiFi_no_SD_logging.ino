#include <FlexCAN.h>
#include <WiFi101.h>
#include <TimeLib.h>
#include "arduino_secrets.h" 
///////please enter your sensitive data in the Secret tab/arduino_secrets.h
char ssid[] = SECRET_SSID;        // your network SSID (name)
char pass[] = SECRET_PASS;    // your network password (use for WPA, or use as key for WEP)
int keyIndex = 0;                // your network key Index number (needed only for WEP)


int port = 80;
WiFiServer server(port);


//Define message from FlexCAN library
CAN_message_t rxmsg;


//Create a counter to keep track of message traffic
uint32_t RXCount0 = 0;
uint32_t RXCount1 = 0;

//Define LED
#define YELLOW_LED_PIN 5
#define RED_LED_PIN 14
#define BLUE_LED_PIN 39
#define GREEN_LED_PIN 6

boolean YELLOW_LED_state; 
boolean RED_LED_state;
boolean BLUE_LED_state; 
boolean GREEN_LED_state; 

//Define default baudrate
#define BAUDRATE250K 250000

#define BUFFER_SIZE 100
uint8_t data_buffer[BUFFER_SIZE];
uint16_t current_position = 0;
uint8_t current_channel;
uint16_t counter;

elapsedMicros microsecondsPerSecond;
elapsedMicros timer;
boolean last_buffer;

int status = WL_IDLE_STATUS;
boolean WiFi_status;

//Define the pins for WiFi chip
#define WiFi_EN 24
#define WiFi_RST 25
#define WiFi_CS 31
#define WiFi_IRQ 23

void load_buffer(){
  timer = 0;

  //Toggle the LED
  GREEN_LED_state = !GREEN_LED_state;
  digitalWrite(GREEN_LED_PIN, GREEN_LED_state);
    
  data_buffer[current_position] = current_channel;
  current_position += 1;
  
  time_t timeStamp = now();
  memcpy(&data_buffer[current_position], &timeStamp, 4);
  current_position += 4;
  
  memcpy(&data_buffer[current_position], &rxmsg.micros, 4);
  current_position += 4;

  memcpy(&data_buffer[current_position], &rxmsg.id, 4);
  current_position += 4;

  // Store the message length as the most significant byte and use the 
  // lower 24 bits to store the microsecond counter for each second.
  uint32_t DLC = (rxmsg.len << 24) | (0x00FFFFFF & uint32_t(microsecondsPerSecond));
  memcpy(&data_buffer[current_position], &DLC, 4);
  current_position += 4;

  memcpy(&data_buffer[current_position], &rxmsg.buf, 8); 
  current_position += 8;
  if (current_position >= BUFFER_SIZE-1){
    current_position = 0;
    server.write(data_buffer,BUFFER_SIZE);
    YELLOW_LED_state = !YELLOW_LED_state;
    digitalWrite(YELLOW_LED_PIN, YELLOW_LED_state);
  }
  counter = current_position;
  last_buffer = true; 
}
time_t getTeensy3Time(){
  microsecondsPerSecond = 0;
  return Teensy3Clock.get();
}

void setup() {

    //CAN Initialize
  Can1.begin(BAUDRATE250K);
  Can0.begin(BAUDRATE250K);

    //The default filters exclude the extended IDs, so we have to set up CAN filters to allow those to pass.
  CAN_filter_t allPassFilter;
  allPassFilter.ext=1;
  for (uint8_t filterNum = 0; filterNum < 8;filterNum++)
  { //only use half the available filters for the extended IDs
   Can0.setFilter(allPassFilter,filterNum); 
   Can1.setFilter(allPassFilter,filterNum); 
  }

  pinMode(BLUE_LED_PIN,OUTPUT);
  pinMode(YELLOW_LED_PIN,OUTPUT);
  pinMode(GREEN_LED_PIN,OUTPUT);
  pinMode(RED_LED_PIN,OUTPUT);

  
  //Initialize WiFi module
  WiFi.setPins(WiFi_CS,WiFi_IRQ,WiFi_RST);
  pinMode(WiFi_EN, OUTPUT);
  digitalWrite(WiFi_EN,HIGH);
  //Initialize serial and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  Serial.println("Access Point Web Server");

  // check for the presence of the shield:
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WiFi shield not present");
    WiFi_status = false;
  }

  else if (WiFi.status() == !WL_NO_SHIELD){
    WiFi_status = true;
    digitalWrite(BLUE_LED_PIN,HIGH);
  }

  // by default the local IP address of will be 192.168.1.1
  // you can override it with the following:
  //WiFi.config(IPAddress(10, 0, 0, 1));

  // print the network name (SSID);
  Serial.print("Creating access point named: ");
  Serial.println(ssid);

  // Create open network. Change this line if you want to create an WEP network:
  status = WiFi.beginAP(ssid, pass);
  if (status != WL_AP_LISTENING) {
    Serial.println("Creating access point failed");
    WiFi_status = false;
  }

  server.begin();
    // Setup timing services
  setSyncProvider(getTeensy3Time);
  if (timeStatus()!= timeSet) {
    Serial.println("Unable to sync with the RTC");
  } else {
    Serial.println("RTC has set the system time");
  }
  setSyncInterval(1);
  

  // you're connected now, so print out the status
  printWiFiStatus();
}


void loop() {
  // compare the previous status to the current status
  if (status != WiFi.status()) {
    // it has changed update the variable
    status = WiFi.status();

    if (status == WL_AP_CONNECTED) {
      byte remoteMac[6];

      // a device has connected to the AP
      Serial.print("Device connected to AP, MAC address: ");
      WiFi.APClientMacAddress(remoteMac);
      printMacAddress(remoteMac);
    } else {
      // a device has disconnected from the AP, and we are back in listening mode
      Serial.println("Device disconnected from AP");
    }
  }
  WiFiClient client = server.available();
  if (client) {                             // if you get a client,
    Serial.println("new client");           // print a message out the serial port
    while (client.connected()) {
  if (Can0.available()) {
    Can0.read(rxmsg);
    RXCount0++;
    current_channel = 0;
    load_buffer();
  }
  
  if (Can1.available()) {
    Can1.read(rxmsg);
    RXCount1++;
    current_channel = 1;
    load_buffer();
  }
 
  if (timer >=2000000 && last_buffer == true){
    server.write(data_buffer,counter);
    last_buffer = false;
    client.stop();
    }
            }
  
  client.stop();
  Serial.println("client disconnected");
}
}
void printWiFiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");

}

void printMacAddress(byte mac[]) {
  for (int i = 5; i >= 0; i--) {
    if (mac[i] < 16) {
      Serial.print("0");
    }
    Serial.print(mac[i], HEX);
    if (i > 0) {
      Serial.print(":");
    }
  }
  Serial.println();
}
