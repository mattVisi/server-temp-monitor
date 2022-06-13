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


//#define DEBUG
//#define NO_MAIL
//#define CONFIG_ON_STARTUP

#define ONE_WIRE_BUS 4
const int ledBluePin = 18;
const int ledGreenPin = 19;
const int ledRedPin = 21;
const int buttonPin = 5;
String status = "IDLE"; // System status IDLE/PRE_ALARM/ALARM/SENSOR_FAILURE/CONFIG
String previousStatus = "IDLE";  // System status at the end of the previous loop
int tempReadingErrotCnt = 0; // Counts how many consecutive temperature reading errors happend
unsigned long lastFailureEmailTime;   // Last time (millis) a failure email was sent
unsigned long lastAlarmEmailTime;   // Last time (millis) an alarm email was sent
#define BUTTON_TIME_CONFIG 30 // Time to hold the button pressed to enable the configuration interface
bool isPressed = false;
int buttonCnt = 0;
int buttonState;
#define SERIAL_BUFFER_SIZE 128
#define MODE_CLEAR_TEXT 0
#define MODE_PASSWORD 1

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

char *EAP_ID;       // Enterprise WiFi credentials. Leave empty when not using WPA2 Enterprise
char *EAP_USERNAME; // Enterprise WiFi credentials. Leave empty when not using WPA2 Enterprise
char *EAP_PASSWORD; // Enterprise WiFi credentials. Leave empty when not using WPA2 Enterprise

// TEMPERATURE CONFIGURATION
float PRE_ALARM_TEMPERATURE; // Temperature above wich the pre alarm is triggered
float ALARM_TEMPERATURE;     // Temperature above wich the alarm sends a notify via email
float ALARM_RESET_THRESHOLD; // Temperature threshold subtracted to the pre alarm threshold under which the alarm is reactivated
unsigned long mesurementInterval;  // Time intervall (milliseconds) beetween mesurements
unsigned long alarmEmailInterval;  // Time intervall (milliseconds) beetween each alarm emails
bool firstTempAlarm = true;   // Flag which is true until a temperature alarm is triggered
bool firstSensorAlarm = true;   // Flag which is true until any sensor failure alarm is triggered
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

void printConfig(int mode);
unsigned long TimeDiff(unsigned long lastTime, unsigned long currTime);
int getStringFromSerial(char *serialBuffer, String prompt, int mode);
String strToAst(String inputString);  // Converts input string as a string of asterisks, leaving clear only the first and the last charaters
void serialConfiguration();
void connectToWiFi();

/*TEMPERATURE SENSOR STUFF AND FUNCTIONS*/
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress rackThermometer;
float tempC;
unsigned long lastMesurementTime = 0;
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
  delay(2000);
  // while(!Serial) {;}    //Waits for the serial port to open. uSE ONLY when debugging via serial port

  #ifdef CONFIG_ON_STARTUP
  userSettings.begin("network");
  userSettings.putString("ssid", "your_ssid");
  userSettings.putString("isWpaEnterprise", "no");
  userSettings.putString("passwd", "password");
  userSettings.putString("eap_id", "id");
  userSettings.putString("eap_username", "user");
  userSettings.putString("eap_password", "password");
  userSettings.end();

  userSettings.begin("alarms");
  userSettings.putFloat("pre_alarm", 30.0);       // temperature above wich the pre alarm is triggered
  userSettings.putFloat("alarm_threshold", 35.0); // temperature above wich the alarm sends a notify via email
  userSettings.putFloat("reset_threshold", 1.0);  // temperature threshold subtracted to the pre alarm threshold under which the alarm is reactivated
  userSettings.putInt("mesure_interval", 60);   // time intervall (seconds) beetween mesurements
  userSettings.putInt("alarm_interval", 10);   // time intervall (minutes) beetween alarm emails
  userSettings.end();

  userSettings.begin("email");
  userSettings.putString("smtp_server", "smtp.email.something");
  userSettings.putUInt("smpt_port", 465);
  userSettings.putString("sender_address", "you@email.something");
  userSettings.putString("sender_password", "password");
  userSettings.putString("author_name", "ESP32 - Server temp monitor");
  userSettings.putString("recipient_1", "");
  userSettings.putString("imAliveMessage", "no");
  userSettings.putInt("imAlive_intrvl", 30);  // time intervall (days) beetween "Im alive" emails
  userSettings.end();
  #endif
  
  #ifdef DEBUG
  printConfig(MODE_CLEAR_TEXT);
  #endif
  #ifndef DEBUG
  printConfig(MODE_PASSWORD);
  #endif

  pinMode(ledBluePin, OUTPUT);
  pinMode(ledRedPin, OUTPUT);
  pinMode(ledGreenPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);

  digitalWrite(ledBluePin, LOW);
  digitalWrite(ledRedPin, LOW);
  digitalWrite(ledGreenPin, LOW);

  Serial.print("Initializing timer . . .");
  buttonTimer = timerBegin(0, 80, true);
  timerAttachInterrupt(buttonTimer, &onTimer, true);
  timerAlarmWrite(buttonTimer, 100000, true);
  timerAlarmEnable(buttonTimer);
  Serial.println("DONE");

  Serial.println("Connecting to WiFi");
  connectToWiFi();
  long wifiBeginMillis = millis();
  while (WiFi.status() != WL_CONNECTED)
    {
    Serial.print(".");
    if (millis() - wifiBeginMillis > 30000) break; 
    delay(500);   
    }
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println();
      Serial.println("ERROR: WiFi connect timeout");
      Serial.println("WiFi not connected. Check your network configuration.");
      delay(2000);
      ESP.restart();
    } else {
      Serial.println();
      Serial.println(" DONE");
      Serial.println("WiFi connected.");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
    }
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

  // ALARMS CONFIGURATION
  userSettings.begin("alarms");
  PRE_ALARM_TEMPERATURE = userSettings.getFloat("pre_alarm");
  ALARM_TEMPERATURE = userSettings.getFloat("alarm_threshold");
  ALARM_RESET_THRESHOLD = userSettings.getFloat("reset_threshold");
  mesurementInterval = userSettings.getInt("mesure_interval")*1000;
  alarmEmailInterval = userSettings.getInt("alarm_interval")*60000;
  userSettings.end();

  #ifdef DEBUG
  Serial.println();
  Serial.println(PRE_ALARM_TEMPERATURE);
  Serial.println(ALARM_TEMPERATURE);
  Serial.println(ALARM_RESET_THRESHOLD);
  Serial.println(mesurementInterval);
  Serial.println(alarmEmailInterval);
  Serial.println();
  #endif

  // enable or disable the email debug via Serial port
  smtp.debug(1);
  // Set the callback function to get the sending results
  smtp.callback(smtpCallback);
}

void loop()
{
  if (TimeDiff(lastMesurementTime, millis()) > mesurementInterval && status != "CONFIG")
  {
    Serial.print(millis());
    #ifdef DEBUG
    Serial.print(" | Last mesure time diff ");
    Serial.print(TimeDiff(lastMesurementTime, millis()));
    #endif

    // Getting temperature and counting reading errors
    if (getTemperature(tempC))
    {
      tempReadingErrotCnt++;
      Serial.print(" - Failed temp");
    }
    else if (status == "SENSOR_FAILURE")
    {
      status = "IDLE";
      firstSensorAlarm = true;
      tempReadingErrotCnt = 0;
      Serial.print(" - Temperature: " + String(tempC));
    }
    else
    {
      tempReadingErrotCnt = 0;
      Serial.print(" - Temperature: " + String(tempC));
    }
    #ifdef DEBUG
    Serial.print(" | Fail count: " + String(tempReadingErrotCnt));
    #endif

    lastMesurementTime = millis();

    if (tempReadingErrotCnt >= 5)
      status = "SENSOR_FAILURE";
    // Done counting errors

    if ((status == "IDLE" || status == "PRE_ALARM") && (tempC >= PRE_ALARM_TEMPERATURE && tempC < ALARM_TEMPERATURE))
    {
      if (firstTempAlarm || TimeDiff(lastAlarmEmailTime, millis()) > alarmEmailInterval)
      {
        status = "PRE_ALARM";
        firstTempAlarm = false;
        sendEmail("PRE_ALARM");
        lastAlarmEmailTime = millis();
      }
    }
    else if ((status == "PRE_ALARM" || status == "ALARM") && tempC >= ALARM_TEMPERATURE)
    {
      if (firstTempAlarm || TimeDiff(lastAlarmEmailTime, millis()) > alarmEmailInterval)
      {
        status = "ALARM";
        firstTempAlarm = false;
        sendEmail("ALARM");
        lastAlarmEmailTime = millis();
      }
    }
    if ((status == "PRE_ALARM" || status == "ALARM") && tempC <= PRE_ALARM_TEMPERATURE - ALARM_RESET_THRESHOLD)
    {
      status = "IDLE";
      firstTempAlarm = true;
      sendEmail("ALARM_RESET");
    }

    #ifdef DEBUG
    Serial.print(" | Last alarm  time diff ");
    Serial.print(TimeDiff(lastAlarmEmailTime, millis()));
    Serial.print(" | Last failure time diff ");
    Serial.print(TimeDiff(lastFailureEmailTime, millis()));
    #endif
    Serial.print(" | System status: " + status);
    Serial.println(" | Previous system status: " + previousStatus);
  }
  if (status == "SENSOR_FAILURE")
  {
    if (firstSensorAlarm || TimeDiff(lastFailureEmailTime, millis()) > alarmEmailInterval)
    {
      firstSensorAlarm = false;
      lastFailureEmailTime = millis();
      sendEmail("SENSOR_FAILURE");
    }
  }
  else if (status == "CONFIG")
  {
    serialConfiguration();
  }

  if (status == "IDLE")
    blinkLed(50, 950, ledGreenPin);
  else if (status == "PRE_ALARM")
    blinkLed(50, 950, ledRedPin);
  else if (status == "ALARM")
    blinkLed(50, 250, ledRedPin);
  else if (status == "CONFIG")
    blinkLed(50, 950, ledBluePin);
  
  if (WiFi.status() != WL_CONNECTED) {
      Serial.println();
      Serial.println("ERROR: WiFi disconnected");
      delay(2000);
      ESP.restart();
    }
  previousStatus = status;
}

unsigned long TimeDiff(unsigned long lastTime, unsigned long currTime)
{
  if (currTime < lastTime)

    return (0xffffffff - lastTime+ currTime);

  return (currTime- lastTime);
}

int getStringFromSerial(char *serialBuffer, String prompt, int mode)
{
  int i = 0;
  int incomingByte = 0; // for incoming serial data

  Serial.print(prompt);
  while (incomingByte != 13 && i <= SERIAL_BUFFER_SIZE)
  {
    if (Serial.available()  > 0)
    {
      incomingByte = Serial.read(); // read the incoming byte:
      if (incomingByte == 13) {
        break;  // exits the function if CR is received
      } else if (incomingByte == 127) {
        if (i>0) i--;
        else Serial.write(' ');
        Serial.write(incomingByte);
        serialBuffer[i] = 0;
      } else {
        serialBuffer[i] = incomingByte; // saves the incoming byte to the buffer
        i++;
      }
      if (mode == 0 && incomingByte != 127) Serial.write(incomingByte); // print the received byte
      else if (mode == 1 && incomingByte != 127)  {
        Serial.write(incomingByte);
        delay(300);
        Serial.write(127);
        Serial.write('*');
      }
      if (i == SERIAL_BUFFER_SIZE+1)
      {
        i--;
        serialBuffer[i] = 0;
        Serial.write(127);
        Serial.write(7);
      }
    }
  }
  serialBuffer[i] = 0;
  return(i);
}

String strToAst(String inputString) {
  if (inputString.length() == 0) return "";
  for (int i = 1; i<(inputString.length()-1); i++) inputString.setCharAt(i, '*');
  return inputString;
}

void printConfig(int mode) {
  Serial.println();
  Serial.println("## Current configuration ##");
  Serial.println("Network:");
  userSettings.begin("network");
  Serial.println("    ssid - " + userSettings.getString("ssid"));
  Serial.println("    Is an enterprise login? - " + userSettings.getString("isWpaEnterprise"));
  if (String(userSettings.getString("isWpaEnterprise")) == String("no")) {
    Serial.print("    Password - ");
    if (mode == 0) Serial.print(userSettings.getString("passwd"));
    else if (mode == 1) Serial.print(strToAst(userSettings.getString("passwd")));
    Serial.println();
  } else if (String(userSettings.getString("isWpaEnterprise")) == String("yes")) {
  Serial.println("  WPA2 enterprise login:");
  Serial.println("    User ID - " + userSettings.getString("eap_id"));
  Serial.println("    Username - " + userSettings.getString("eap_username"));
  Serial.print("    Password - ");
    if (mode == 0) Serial.print(userSettings.getString("eap_password"));
    else if (mode == 1) Serial.print(strToAst(userSettings.getString("eap_password")));
    Serial.println();
  }
  userSettings.end();
  Serial.println("Temperature:");
  userSettings.begin("alarms");
  Serial.println("    Pre alarm temperature - " + String(userSettings.getFloat("pre_alarm")) + " °C");
  Serial.println("    Alarm temperature - " + String(userSettings.getFloat("alarm_threshold")) + " °C");
  Serial.println("    Alarm reset threshold - " + String(userSettings.getFloat("reset_threshold")) + " °C");
  Serial.println("    Time intervall beetween mesurements - " + String(userSettings.getInt("mesure_interval")) + " seconds");
  Serial.println("    Time intervall beetween alarm email - " + String(userSettings.getInt("alarm_interval")) + " minutes");
  userSettings.end();
  Serial.println("Email:");
  userSettings.begin("email");
  Serial.println("    Smtp server - " + userSettings.getString("smtp_server"));
  Serial.println("    Port - " + String(userSettings.getUInt("smpt_port")));
  Serial.println("    Sender address - " + userSettings.getString("sender_address"));
  Serial.print("    SMTP password - ");
    if (mode == 0) Serial.print(userSettings.getString("sender_password"));
    else if (mode == 1) Serial.print(strToAst(userSettings.getString("sender_password")));
    Serial.println();
  Serial.println("    Author name - " + userSettings.getString("author_name"));
  Serial.println("    Email recipient - " + userSettings.getString("recipient_1"));
  userSettings.end();
  Serial.println();
}

void connectToWiFi()
{
  char ssidBuff[32];
  char passwdBuff[50];
  char eapIDBuff[50];
  char eapUsernameBuff[50];
  char eapPasswordBuff[50];

  userSettings.begin("network");
  if (userSettings.getString("isWpaEnterprise") == String("no"))
  {
    Serial.println("Not using wpa enterprise.");
    userSettings.getString("ssid").toCharArray(ssidBuff, 32);
    ssid = ssidBuff;
    userSettings.getString("passwd").toCharArray(passwdBuff, 50);
    passwd = passwdBuff;
    WiFi.begin(ssid, passwd);
  }
  else
  {
    Serial.println("Using wpa enterprise.");
    userSettings.getString("ssid").toCharArray(ssidBuff, 32);
    ssid = ssidBuff;
    userSettings.getString("eap_id").toCharArray(eapIDBuff, 50);
    EAP_ID = eapIDBuff;
    userSettings.getString("eap_username").toCharArray(eapUsernameBuff, 50);
    EAP_USERNAME = eapUsernameBuff;
    userSettings.getString("eap_password").toCharArray(eapPasswordBuff, 50);
    EAP_PASSWORD = eapPasswordBuff;

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
  userSettings.end();
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
  #ifndef NO_MAIL 
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
    message.subject = "Temperatura sala server - Soglia di attenzione superata";
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
    message.subject = "Temperatura sala server - ALLARME";
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
    message.subject = "Temperatura sala server - Allarme rientrato";
    message.addRecipient("Tecnici", userSettings.getString("recipient_1"));
    userSettings.end();
    // Set the message content
    message.text.content = "La temperatura è tornata sotto la soglia di attenzione. L'ultima misurazione è stata di " + String(tempC) + " °C.";
  }
  else if (messageType == "SENSOR_FAILURE")
  {
    // Set the message headers
    userSettings.begin("email");
    message.sender.name = userSettings.getString("author_name");
    message.sender.email = userSettings.getString("sender_address");
    message.subject = "Server Temp Monitor - SENSORE GUASTO";
    message.addRecipient("Tecnici", userSettings.getString("recipient_1"));
    userSettings.end();
    // Set the message content
    message.text.content = "Le ultime 5 letture della temperatura non hanno avuto successo. \nLa temperatura della sala server non è sotto controllo. \n\n Ogni cinque letture fallite verrà inviata questa email.";
    tempReadingErrotCnt = 0;
  }
  else if (messageType == "IM_ALIVE")
  {
    // Set the message headers
    userSettings.begin("email");
    message.sender.name = userSettings.getString("author_name");
    message.sender.email = userSettings.getString("sender_address");
    message.subject = "Temperatura sala server - I'm alive!";
    message.addRecipient("Tecnici", userSettings.getString("recipient_1"));
    userSettings.end();
    // Set the message content
    message.text.content = "Sono vivo e sto controllando la sala server. L'ultima misurazione è stata di " + String(tempC) + " °C." + "Sono acceso da " + String(millis()/1000) + " secondi";
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
  #endif
  #ifdef NO_MAIL
  
  Serial.print("  | Sending mail " + messageType + "  | ");

  #endif
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
  char buf[SERIAL_BUFFER_SIZE+1];
  float floatBuf;
  int intBuf;
  Serial.println();
  Serial.println();
  Serial.println(" ---- CONFIGURATION ---- ");
  Serial.println();
  Serial.println("You can digit using your keyboard. Press <ENTER> to confirm the inserted value. If <ENTER> is pressed the previously configured value will remain in memory.");
  
  Serial.println();
  Serial.println("Network configuration:");
  userSettings.begin("network");
  getStringFromSerial(buf, "  SSID (" + userSettings.getString("ssid") + "): ", MODE_CLEAR_TEXT);
  if(String(buf) != String("")) userSettings.putString("ssid", String(buf));
  Serial.println();

  while(true) {
    getStringFromSerial(buf, "  Are you using enterprise login? yes/no (" + userSettings.getString("isWpaEnterprise") + "): ", MODE_CLEAR_TEXT);
    if(String(buf) == String("")) break;
    else if (String(buf) == String("yes") || String(buf) == String("no")) {
      userSettings.putString("isWpaEnterprise", String(buf));
      Serial.println();
      break;
    }
  }
  Serial.println();
  if (String(userSettings.getString("isWpaEnterprise")) == String("no")) {
    getStringFromSerial(buf, "  Password (" + strToAst(userSettings.getString("passwd")) + "): ", MODE_PASSWORD);
    if(String(buf) != String("")) userSettings.putString("passwd", String(buf));
    Serial.println();
  } else if (String(userSettings.getString("isWpaEnterprise")) == String("yes")) {
    getStringFromSerial(buf, "  User ID (" + userSettings.getString("eap_id") + "): ", MODE_CLEAR_TEXT);
    if(String(buf) != String("")) userSettings.putString("eap_id", String(buf));
    Serial.println();
    getStringFromSerial(buf, "  Username (" + userSettings.getString("eap_username") + "): ", MODE_CLEAR_TEXT);
    if(String(buf) != String("")) userSettings.putString("eap_username", String(buf));
    Serial.println();
    getStringFromSerial(buf, "  Password (" + strToAst(userSettings.getString("eap_password")) + "): ", MODE_PASSWORD);
    if(String(buf) != String("")) userSettings.putString("eap_password", String(buf));
    Serial.println();
  }
  userSettings.end();

  Serial.println();
  Serial.println("Temperature sensor configuration");
  Serial.println("Note: Temperatures cannot be set at 0.00 °C");
  
  userSettings.begin("alarms");
  while(true) {
    do
    {
      getStringFromSerial(buf, "  Pre alarm temperature (" + String(userSettings.getFloat("pre_alarm")) + "°C ): ", MODE_CLEAR_TEXT);
      if (String(buf).toFloat() != float(0.0) /*This basically means isNaN(buf)*/ && String(buf) != String(""))
      {
        userSettings.putFloat("pre_alarm", String(buf).toFloat());
      }
      else if (String(buf) == String("")) break;
    } while (String(buf).toFloat() == float(0.0));
    
    do
    {
      Serial.println();
      getStringFromSerial(buf, "  Alarm temperature (" + String(userSettings.getFloat("alarm_threshold")) + "°C ): ", MODE_CLEAR_TEXT);
      if (String(buf).toFloat() != float(0.0) && String(buf) != String(""))
      {
        userSettings.putFloat("alarm_threshold", String(buf).toFloat());
      }
      else if (String(buf) == String("")) break;
    } while (String(buf).toFloat() == float(0.0));

    if(userSettings.getFloat("pre_alarm") < userSettings.getFloat("alarm_threshold")) break;
    else {
      Serial.println("  ERROR! PRE-ALARM TEMPERATURE CANNOT BE GRATER THAN THE ALARM TEMPERATURE!");
      Serial.println();
      Serial.println("  Retry.");
    }
  }

do
    {
      Serial.println();
      getStringFromSerial(buf, "  Alarm reset threshold (" + String(userSettings.getFloat("reset_threshold")) + "°C ): ", MODE_CLEAR_TEXT);
      if (String(buf).toFloat() != float(0.0) && String(buf) != String(""))
        userSettings.putFloat("reset_threshold", String(buf).toFloat());
      else if (String(buf) == String("")) break;
    } while (String(buf).toFloat() == float(0.0));

  do
    {
      Serial.println();
      getStringFromSerial(buf, "  Intervall between mesurements (" + String(userSettings.getInt("mesure_interval")) + " seconds): ", MODE_CLEAR_TEXT);
      sscanf(buf, "%04d", &intBuf);
      if (intBuf != 0 && String(buf) != String(""))
      {
        userSettings.putInt("mesure_interval", intBuf);
      }
      else if (String(buf) == String("")) break;
    } while (String(buf).toInt() == long(0));

    do
    {
      Serial.println();
      getStringFromSerial(buf, "  Time intervall between alarm emails (" + String(userSettings.getInt("alarm_interval")) + " minutes): ", MODE_CLEAR_TEXT);
      sscanf(buf, "%04d", &intBuf);
      if (intBuf != 0 && String(buf) != String(""))
      {
        userSettings.putInt("alarm_interval", intBuf);
      }
      else if (String(buf) == String("")) break;
    } while (String(buf).toInt() == long(0));
    Serial.println();
  
  userSettings.end();

  Serial.println("Email configuration");
  userSettings.begin("email");
  getStringFromSerial(buf, "  SMTP server address (" + userSettings.getString("smtp_server") + "): ", MODE_CLEAR_TEXT);
  if(String(buf) != String("")) userSettings.putString("smtp_server", String(buf));

  do {
    Serial.println();
    getStringFromSerial(buf, "  SMTP port (" + String(userSettings.getUInt("smpt_port")) + "): ", MODE_CLEAR_TEXT);
    sscanf(buf, "%04u", &intBuf);
    if(intBuf != 0 && String(buf) != String("")) {
      userSettings.putUInt("smpt_port", intBuf);
    }
    else if (String(buf) == String("")) break;
  } while(String(buf).toInt() == long(0));

  Serial.println();
  getStringFromSerial(buf, "  Sender email address (" + userSettings.getString("sender_address") + "): ", MODE_CLEAR_TEXT);
  if(String(buf) != String("")) userSettings.putString("sender_address", String(buf));
  Serial.println();
  getStringFromSerial(buf, "  Sender password (" + strToAst((userSettings.getString("sender_password"))) + "): ", MODE_PASSWORD);
  if(String(buf) != String("")) userSettings.putString("sender_password", String(buf));
  Serial.println();
  getStringFromSerial(buf, "  Sender name (" + userSettings.getString("author_name") + "): ", MODE_CLEAR_TEXT);
  if(String(buf) != String("")) userSettings.putString("author_name", String(buf));
  Serial.println();
  getStringFromSerial(buf, "  Recipient email address (" + userSettings.getString("recipient_1") + "): ", MODE_CLEAR_TEXT);
  if(String(buf) != String("")) userSettings.putString("recipient_1", String(buf));
  
  userSettings.end();
  Serial.println();
  Serial.println();
  delay(1500);
  Serial.println("Configuration completed.");
  delay(1000);
  
  #ifdef DEBUG
  printConfig(MODE_CLEAR_TEXT);
  #endif
  #ifndef DEBUG
  printConfig(MODE_PASSWORD);
  #endif

  while(true) {
    Serial.println();
    getStringFromSerial(buf, "Confirm? yes/no: ", MODE_CLEAR_TEXT);
    if (String(buf) == String("yes") || String(buf) == String("no")) break;
  }
  Serial.println();
  if (String(buf) == String("yes")) {
    delay(500);
    Serial.println("Restarting system ...");
    delay(500);
    ESP.restart();
  } else return;
}