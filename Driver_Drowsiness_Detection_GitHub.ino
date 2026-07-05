#include <Wire.h>
#include <TinyGPS++.h>
#include <LiquidCrystal.h>

#include <ArduinoIoTCloud.h>
#include <Arduino_ConnectionHandler.h>

#include <WiFi.h>
#include <WiFiUdp.h>
#include <WiFiClientSecure.h>
#include <Base64.h>

/* ---------------- IOT CLOUD ---------------- */
const char DEVICE_LOGIN_NAME[]  = "YOUR_DEVICE_LOGIN_NAME";
const char SSID[]               = "YOUR_WIFI_SSID";
const char PASS[]               = "YOUR_WIFI_PASSWORD";
const char DEVICE_KEY[]         = "YOUR_DEVICE_KEY";

String status = "Starting...";
CloudLocation mapi;
bool alerti = false;

void onStatusChange(){}
void onMapiChange(){}
void onAlertiChange(){}

void initProperties()
{
  ArduinoCloud.setBoardId(DEVICE_LOGIN_NAME);
  ArduinoCloud.setSecretDeviceKey(DEVICE_KEY);

  ArduinoCloud.addProperty(status, READWRITE, ON_CHANGE, onStatusChange);
  ArduinoCloud.addProperty(mapi, READWRITE, ON_CHANGE, onMapiChange);
  ArduinoCloud.addProperty(alerti, READWRITE, ON_CHANGE, onAlertiChange);
}

WiFiConnectionHandler ArduinoIoTPreferredConnection(SSID, PASS);

/* ---------------- GPS ---------------- */
TinyGPSPlus gps;
HardwareSerial gpsSerial(2);
#define GPS_RX 16
#define GPS_TX 17

/* ---------------- LCD ---------------- */
LiquidCrystal lcd(2,15,19,18,5,4);

/* ---------------- MOTOR ---------------- */
#define MOTOR_PIN 27

/* ---------------- BUZZER ---------------- */
#define BUZZER_PIN 14

/* ---------------- UDP ---------------- */
WiFiUDP udp;
const int UDP_PORT = 4210;
char incomingPacket[255];

/* ---------------- SMS (Twilio) ---------------- */
const char* sid      = "YOUR_TWILIO_ACCOUNT_SID";
const char* token    = "YOUR_TWILIO_AUTH_TOKEN";
const char* fromNum  = "YOUR_TWILIO_PHONE_NUMBER";
const char* toNum    = "RECIPIENT_PHONE_NUMBER";

String deviceName = "DRIVER ALERT";

/* ---------------- VARIABLES ---------------- */
double lat = 0;
double lng = 0;

bool smsTriggered = false;
bool alertSent = false;

/* ---------------- MOTOR FUNCTION ---------------- */
void slowDownMotor()
{
  for(int speed = 255; speed >= 0; speed -= 5)
  {
    analogWrite(MOTOR_PIN, speed);
    delay(50);
  }
}

/* ---------------- SMS ---------------- */
void triggerSMS()
{
  if(alertSent) return;
  smsTriggered = true;
  Serial.println("SMS Triggered");
}

void handleSMS()
{
  if(!smsTriggered) return;

  if(WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WiFi NOT connected");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();

  if(!client.connect("api.twilio.com", 443))
  {
    Serial.println("Connection FAILED");
    return;
  }

  String bodyMsg;

  if(gps.location.isValid())
  {
    String locationURL = "http://maps.google.com/?q=" + String(lat,6) + "," + String(lng,6);
    bodyMsg = deviceName + " DRIVER SLEEP DETECTED! " + locationURL;
  }
  else
  {
    bodyMsg = deviceName + " DRIVER SLEEP DETECTED!";
  }

  String msg = "To=" + String(toNum) +
               "&From=" + String(fromNum) +
               "&Body=" + bodyMsg;

  String url = "/2010-04-01/Accounts/" + String(sid) + "/Messages.json";

  client.println("POST " + url + " HTTP/1.1");
  client.println("Host: api.twilio.com");
  client.println("Authorization: Basic " + base64::encode(String(sid) + ":" + String(token)));
  client.println("Content-Type: application/x-www-form-urlencoded");
  client.println("Content-Length: " + String(msg.length()));
  client.println("Connection: close");
  client.println();
  client.print(msg);

  while (client.connected())
  {
    while (client.available())
    {
      client.readStringUntil('\n');
    }
  }

  client.stop();

  smsTriggered = false;
  alertSent = true;
}

/* ---------------- SETUP ---------------- */
void setup()
{
  Serial.begin(115200);
  Wire.begin(21,22);

  lcd.begin(16,2);
  lcd.print("Initializing...");

  pinMode(23,OUTPUT);
  pinMode(26,OUTPUT);
  pinMode(25,OUTPUT);

  pinMode(MOTOR_PIN, OUTPUT);
  analogWrite(MOTOR_PIN, 255); // full speed

  pinMode(BUZZER_PIN, OUTPUT);

  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);

  WiFi.begin(SSID, PASS);

  delay(3000);

  if(WiFi.status() == WL_CONNECTED)
  {
    initProperties();
    ArduinoCloud.begin(ArduinoIoTPreferredConnection);
    udp.begin(UDP_PORT);
  }

  lcd.clear();
}

/* ---------------- LOOP ---------------- */
void loop()
{
  if(WiFi.status() == WL_CONNECTED)
  {
    ArduinoCloud.update();
  }

  handleSMS();

  while(gpsSerial.available())
    gps.encode(gpsSerial.read());

  if(gps.location.isUpdated() && gps.location.isValid())
  {
    lat = gps.location.lat();
    lng = gps.location.lng();
    mapi = {lat, lng};
  }

  if(WiFi.status() == WL_CONNECTED)
  {
    int packetSize = udp.parsePacket();
    if(packetSize)
    {
      int len = udp.read(incomingPacket, 255);
      if(len > 0) incomingPacket[len] = 0;

      String data = String(incomingPacket);

      if(data == "A")
      {
        status = "Driver Sleep!";
        


        digitalWrite(25,LOW);//Green
        digitalWrite(26,HIGH);//red
        digitalWrite(23,HIGH);//Buzzer
        
        digitalWrite(BUZZER_PIN, HIGH);
        alerti = true;

        slowDownMotor();
        triggerSMS();
      }
      else if(data == "B")
      {
        status = "Normal Driving";
        alerti = false;
        alertSent = false;
        digitalWrite(25,HIGH);//Green
        digitalWrite(26,LOW);//red
        digitalWrite(23,LOW);//Buzzer
        
        digitalWrite(BUZZER_PIN, LOW);

        analogWrite(MOTOR_PIN, 255);
      }
    }
  }

  lcd.setCursor(0,0);
  lcd.print(status);
  lcd.print("        ");

  lcd.setCursor(0,1);
  if(gps.location.isValid())
    lcd.print("Lt:" + String(lat,2) + " Lg:" + String(lng,2));
  else
    lcd.print("Searching GPS...");

  delay(500);
}


****it was my project code and having large dataset
Shall i include it or notvoid setup() {
  // put your setup code here, to run once:

}

void loop() {
  // put your main code here, to run repeatedly:

}
