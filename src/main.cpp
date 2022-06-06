/*
### Temperature monitor with email alarm notification ###

Produced by Matteo Visintini for ITS Alessandro Volta - Trieste

Support for both WPA2 Personal and WPA2 Enterprise WiFi security
*/

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include "esp_wpa2.h"
#include <OneWireNG.h>
#include <DallasTemperature.h>
#include <ESP_Mail_Client.h>

//#define TEMP_TEST
//#define CONFIG_ON_STARTUP

#define ONE_WIRE_BUS 4
const int ledBluePin = 18;
const int ledGreenPin = 19;
const int ledRedPin = 21;
const int buttonPin = 5;
String status = "IDLE"; // machine status IDLE/PRE_ALARM/ALARM
#define BUTTON_TIME_CONFIG 30 // Time to hold the button pressed to enable the configuration interface
bool isPressed = false;
int buttonCnt = 0;
int buttonState;

/*
#################################
###### CONFIGURATION STUFF ######
### DO NOT EDIT THIS SECTION ####
### USE PREFERENCES IN SETUP ####
#################################
*/
// WiFi CONFIGURATION
char *ssid;           // WiFi ssid
char *passwd;         // WiFi password. Leave empty if using WPA2 enterprise
bool isWPAenterprise; // Set to TRUE when using WPA2 Enterprise access to the network

char *EAP_ID;       // Enterprise WiFi credentials. Leave empty when not using WPA2 Enterprise
char *EAP_USERNAME; // Enterprise WiFi credentials. Leave empty when not using WPA2 Enterprise
char *EAP_PASSWORD; // Enterprise WiFi credentials. Leave empty when not using WPA2 Enterprise

// TEMPERATURE CONFIGURATION
float PRE_ALARM_TEMPERATURE; // temperature above wich the pre alarm is triggered
float ALARM_TEMPERATURE;     // temperature above wich the alarm sends a notify via email
float ALARM_RESET_THRESHOLD; // temperature threshold subtracted to the pre alarm threshold under which the alarm is reactivated
int32_t mesurementInterval;  // time intervall (milliseconds) beetween mesurements
/*
#############################
##### CONFIGURATION END #####
#############################
*/

Preferences userSettings;

hw_timer_t *buttonTimer = NULL;

void IRAM_ATTR onTimer() 
{
  if (!isPressed) {
    buttonState = digitalRead(buttonPin);
    if (!buttonState) buttonCnt++;
    else 
    {
      buttonCnt = 0;
      return;
    }
    if (buttonCnt >= BUTTON_TIME_CONFIG) {
      status = "CONFIG";
      isPressed = !isPressed;
    }
  }
}

void serialConfiguration();
void connectToWiFi();

/*TEMPERATURE SENSOR STUFF AND FUNCTIONS*/
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress rackThermometer;
float tempC;
long lastMesurementTime = 0;
int getTemperature(float &tempVar);

/*EMAIL STUFF*/
// Define the SMTP Session object which used for SMTP transport
SMTPSession smtp;
// Define a callback function for smtp debug via the serial monitor
void smtpCallback(SMTP_Status status);
// Function to send email
void sendEmail(String messageType);

// RGB LED stuff
bool ledState = HIGH;
long lastBlink = 0;
void blinkLed(long millisecondsOn, long millisecondsOff, int ledPin);

void setup()
{
  Serial.begin(115200);
  // while(!Serial) {;}    //Wait for the serial port to open. Uncomment ONLY to debug via serial monitor, keep commented otherwise

  #ifdef CONFIG_ON_STARTUP
  userSettings.begin("network");
  userSettings.putString("ssid", "");
  userSettings.putBool("isWpaEnterprise", false);
  userSettings.putString("passwd", "");
  userSettings.putString("eap_id", "");
  userSettings.putString("eap_username", "");
  userSettings.putString("eap_password", "");
  userSettings.end();

  userSettings.begin("temperature");
  userSettings.putFloat("pre_alarm", 29.0);       // temperature above wich the pre alarm is triggered
  userSettings.putFloat("alarm_threshold", 30.5); // temperature above wich the alarm sends a notify via email
  userSettings.putFloat("reset_threshold", 1.0);  // temperature threshold subtracted to the pre alarm threshold under which the alarm is reactivated
  userSettings.putInt("mesure_interval", 6000);   // time intervall (milliseconds) beetween mesurements
  userSettings.end();

  userSettings.begin("email");
  userSettings.putString("smtp_server", "");
  userSettings.putUInt("smpt_port", 465);
  userSettings.putString("sender_address", "");
  userSettings.putString("sender_password", "");
  userSettings.putString("author_name", "ESP32 - Server temp monitor");
  userSettings.putString("recipient_1", "");
  userSettings.end();
  #endif
 
  Serial.print("Initializing timer . . . ");
  buttonTimer = timerBegin(0, 80, true);
  timerAttachInterrupt(buttonTimer, &onTimer, true);
  timerAlarmWrite(buttonTimer, 100000, true);
  timerAlarmEnable(buttonTimer);
  Serial.println("DONE");

  Serial.print("Connecting to WiFi ");
  connectToWiFi();

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" DONE");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  Serial.println("Initializing temperature sensor . . .");
  // initialize DallasTemperature
  sensors.begin();
  sensors.setResolution(9);
  // locate devices on the bus
  Serial.print("Found ");
  Serial.print(sensors.getDeviceCount(), DEC);
  Serial.println(" devices.");
  Serial.println();

  // TEMPERATURE CONFIGURATION
  userSettings.begin("temperature");
  PRE_ALARM_TEMPERATURE = userSettings.getFloat("pre_alarm");
  ALARM_TEMPERATURE = userSettings.getFloat("alarm_threshold");
  ALARM_RESET_THRESHOLD = userSettings.getFloat("reset_threshold");
  mesurementInterval = userSettings.getInt("mesure_interval");
  userSettings.end();

  // enable or disable the email debug via Serial port
  smtp.debug(false);
  // Set the callback function to get the sending results
  smtp.callback(smtpCallback);

  pinMode(ledBluePin, OUTPUT);
  pinMode(ledRedPin, OUTPUT);
  pinMode(ledGreenPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);
}

void loop()
{
  if (millis() - lastMesurementTime > mesurementInterval && status != "CONFIG")
  {
    if (getTemperature(tempC))
    {
      Serial.print(" Failed temp");
    }
    else
      Serial.print(" Temperature: " + String(tempC));
    lastMesurementTime = millis();

    Serial.print(millis());
    Serial.println(" Alarm status: " + status);

    if (status == "IDLE" && tempC >= PRE_ALARM_TEMPERATURE)
    {
      status = "PRE_ALARM";
      sendEmail("PRE_ALARM");
    }
    if (status == "PRE_ALARM" && tempC >= ALARM_TEMPERATURE)
    {
      status = "ALARM";
      sendEmail("ALARM");
    }
    else if ((status == "PRE_ALARM" || status == "ALARM") && tempC <= PRE_ALARM_TEMPERATURE - ALARM_RESET_THRESHOLD)
    {
      status = "IDLE";
      sendEmail("ALARM_RESET");
    }
  } 
  else if(status == "CONFIG")
  {
    serialConfiguration();
  }
  
  if (status == "IDLE") blinkLed(50, 950, ledGreenPin);
  else if (status == "PRE_ALARM") blinkLed(50, 950, ledRedPin);
  else if (status == "ALARM") blinkLed(50, 250, ledRedPin);
  else if (status == "CONFIG") blinkLed(50, 950, ledBluePin);
  
  // restart the system every 3 days to dump the RAM and reset the clocks
  if (millis() > 259200000)
  {
    Serial.println(F("Restarting system . . ."));
    ESP.restart();
  }
}

void connectToWiFi()
{
  char ssidBuff[32];
  char passwdBuff[50];
  char eapIDBuff[50];
  char eapUsernameBuff[50];
  char eapPasswordBuff[50];

  if (!userSettings.getBool("isWpaEnterprise"))
  {
    userSettings.begin("network");
    userSettings.getString("ssid").toCharArray(ssidBuff, 32);
    ssid = ssidBuff;
    userSettings.getString("passwd").toCharArray(passwdBuff, 50);
    passwd = passwdBuff;
    userSettings.end();

    WiFi.begin(ssid, passwd);
  }
  else
  {
    userSettings.begin("network");
    userSettings.getString("ssid").toCharArray(ssidBuff, 32);
    ssid = ssidBuff;
    userSettings.getString("eap_id").toCharArray(eapIDBuff, 50);
    EAP_ID = eapIDBuff;
    userSettings.getString("eap_username").toCharArray(eapUsernameBuff, 50);
    EAP_USERNAME = eapUsernameBuff;
    userSettings.getString("eap_password").toCharArray(eapPasswordBuff, 50);
    EAP_PASSWORD = eapPasswordBuff;
    userSettings.end();

    // WPA2 enterprise magic starts here.
    WiFi.disconnect(true);
    WiFi.mode(WIFI_STA);
    esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)EAP_ID, strlen(EAP_ID));
    esp_wifi_sta_wpa2_ent_set_username((uint8_t *)EAP_USERNAME, strlen(EAP_USERNAME));
    esp_wifi_sta_wpa2_ent_set_password((uint8_t *)EAP_PASSWORD, strlen(EAP_PASSWORD));
    esp_wifi_sta_wpa2_ent_enable();
    // WPA2 enterprise magic ends here
    WiFi.begin(ssid);
  }
}

int getTemperature(float &tempVar)
{
  sensors.requestTemperatures();
  // variable to store temperature value
  float matt = sensors.getTempCByIndex(0);
  // Check if reading was successful
  if (matt != DEVICE_DISCONNECTED_C)
  {
    tempVar = matt;
    return 0;
  }
  else
    return 1;
}

void smtpCallback(SMTP_Status status)
{
  Serial.println(status.info());
  if (status.success())
  {
    Serial.println("----------------");
    ESP_MAIL_PRINTF("Message sent success: %d\n", status.completedCount());
    ESP_MAIL_PRINTF("Message sent failled: %d\n", status.failedCount());
    Serial.println("----------------\n");
    struct tm dt;
    for (size_t i = 0; i < smtp.sendingResult.size(); i++)
    {
      SMTP_Result result = smtp.sendingResult.getItem(i);
      time_t ts = (time_t)result.timestamp;
      localtime_r(&ts, &dt);
      ESP_MAIL_PRINTF("Message No: %d\n", i + 1);
      ESP_MAIL_PRINTF("Status: %s\n", result.completed ? "success" : "failed");
      ESP_MAIL_PRINTF("Date/Time: %d/%d/%d %d:%d:%d\n", dt.tm_year + 1900, dt.tm_mon + 1, dt.tm_mday, dt.tm_hour, dt.tm_min, dt.tm_sec);
      ESP_MAIL_PRINTF("Recipient: %s\n", result.recipients);
      ESP_MAIL_PRINTF("Subject: %s\n", result.subject);
    }
    Serial.println("----------------\n");
  }
}

void sendEmail(String messageType)
{
  // Declare the session config data
  ESP_Mail_Session session;
  // Set the session config
  userSettings.begin("email");
  session.server.host_name = userSettings.getString("smtp_server");
  session.server.port = userSettings.getUInt("smpt_port");
  session.login.email = userSettings.getString("sender_address");
  session.login.password = userSettings.getString("sender_password");
  session.login.user_domain = "";
  userSettings.end();

  // Set the NTP config time
  Serial.println(F("Getting time from NTP server..."));
  session.time.ntp_server = "pool.ntp.org";
  session.time.gmt_offset = 1;
  session.time.day_light_offset = 0;
  Serial.println(F("Done."));

  // Declare the message class
  SMTP_Message message;

  if (messageType == "PRE_ALARM")
  {
    Serial.println("Setting email headers.");
    // Set the message headers
    userSettings.begin("email");
    message.sender.name = userSettings.getString("author_name");
    message.sender.email = userSettings.getString("sender_address");
    message.subject = "Temperatura server - Soglia di attenzione superata";
    message.addRecipient("Recipient 1", userSettings.getString("recipient_1"));
    userSettings.end();
    // Set the message content
    message.text.content = "La temperatura ha superato i " + String(tempC) + " °C.";
  }
  else if (messageType == "ALARM")
  {
    Serial.println("Setting email headers.");
    // Set the message headers
    userSettings.begin("email");
    message.sender.name = userSettings.getString("author_name");
    message.sender.email = userSettings.getString("sender_address");
    message.subject = "Allarme temperatura server";
    message.addRecipient("Recipient 1", userSettings.getString("recipient_1"));

    userSettings.end();
    // Set the message content
    message.text.content = "La temperatura ha superato i " + String(tempC) + " °C.";
  }
  else if (messageType == "ALARM_RESET")
  {
    Serial.println("Setting email headers.");
    // Set the message headers
    userSettings.begin("email");
    message.sender.name = userSettings.getString("author_name");
    message.sender.email = userSettings.getString("sender_address");
    message.subject = "Allarme rientrato";
    message.addRecipient("Tecnici", userSettings.getString("recipient_1"));
    userSettings.end();
    // Set the message content
    message.text.content = "La temperatura è tornata sotto la soglia di attenzione. L'ultima misurazione è stata di " + String(tempC) + " °C.";
  }
  else
    return;

  Serial.println(F("Connecting..."));
  smtp.connect(&session);
  // Start sending Email and close the session
  Serial.println(F("Sending..."));
  if (!MailClient.sendMail(&smtp, &message))
    Serial.println("Error sending Email, " + smtp.errorReason());
  else
    Serial.println(F("Email sent successfully"));
}

void blinkLed(long millisecondsOn, long millisecondsOff, int ledPin)
{
  if (ledState == HIGH && millis() - lastBlink > millisecondsOn)
  {
    digitalWrite(ledPin, LOW);
    ledState = !ledState;
    lastBlink = millis();
    return;
  }
  else if (ledState == LOW && millis() - lastBlink > millisecondsOff)
  {
    digitalWrite(ledPin, HIGH);
    ledState = !ledState;
    lastBlink = millis();
    return;
  }
  else
    return;
}

void serialConfiguration() 
{
  Serial.println(" ---- CONFIGURATION ---- ");
  delay(3000);
}