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

/* 
################################
###### USER CONFIGURATION ######
### DO NOT EDIT THIS SECTION ###
### USE PREFERENCES IN SETUP ###
################################
*/
//WiFi CONFIGURATION
char *ssid;  //WiFi ssid
char *passwd;  //WiFi password. Leave empty if using WPA2 enterprise 
bool isWPAenterprise;  //Set to TRUE when using WPA2 Enterprise access to the network

char *EAP_ID;  //Enterprise WiFi credentials. Leave empty when not using WPA2 Enterprise
char *EAP_USERNAME;   //Enterprise WiFi credentials. Leave empty when not using WPA2 Enterprise
char *EAP_PASSWORD;   //Enterprise WiFi credentials. Leave empty when not using WPA2 Enterprise

//TEMPERATURE CONFIGURATION
const float HIGH_ALARM_TEMPERATURE = 23;    //temperature above wich the alarm sends a notify via email
const float ALARM_RESET_THRESHOLD = 1.5;    //temperature threshold subtracted to the alarm threshold under which the alarm is reactivated
const long mesurementInterval = 6000;    //time intervall (milliseconds) beetween mesurements

/*
#############################
##### CONFIGURATION END #####
#############################
*/

Preferences userSettings;

void connectToWiFi();

/*TEMPERATURE SENSOR STUFF AND FUNCTIONS*/
#define ONE_WIRE_BUS 4
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress rackThermometer;
float tempC;
long lastMesurementTime = 0;
int getTemperature(float &tempVar) {
  sensors.requestTemperatures();
  //variable to store temperature value
  float matt = sensors.getTempCByIndex(0);
  // Check if reading was successful
  if(matt != DEVICE_DISCONNECTED_C) {
    tempVar = matt;
    return 0;
  } else return 1;
}

/*EMAIL STUFF*/
// Define the SMTP Session object which used for SMTP transport
SMTPSession smtp;
// Define a callback function for smtp debug via the serial monitor
void smtpCallback(SMTP_Status status);
// Function to send email
void sendEmail(String messageType);

const int ledPin = 2;
bool ledState = HIGH;
long lastBlink = 0;
void blinkLed(long blinkInterval, int pinNum);

void setup() {
  Serial.begin(115200);
  //while(!Serial) {;}    //Wait for the serial port to open. Uncomment ONLY to debug via serial monitor, keep commented otherwise
  
  userSettings.begin("network");
  userSettings.putString("ssid", "");
  userSettings.putBool("isWpaEnterprise", false);
  userSettings.putString("passwd", "");
  userSettings.putString("eap_id", "");
  userSettings.putString("eap_username", "");
  userSettings.putString("eap_password", "");
  userSettings.end();

  userSettings.begin("temperature");
  userSettings.putFloat("alarm_threshold", 23);
  userSettings.putFloat("reset_threshold", 1.5);
  userSettings.putLong("mesure_interval", 60000);
  userSettings.end();

  userSettings.begin("email");
  userSettings.putString("smtp_server", "");
  userSettings.putUInt("smpt_port", 465);
  userSettings.putString("sender_address", "");
  userSettings.putString("email_password", "");
  userSettings.putString("author_name", "ESP32 - Server temp monitor");
  userSettings.putString("email_recipient", "");
  userSettings.end();
  


  Serial.print("Connecting to WiFi");

  connectToWiFi();
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println();
  
  Serial.println("Initializing temperature sensor...");
  //initialize DallasTemperature
  sensors.begin();
  // locate devices on the bus
  Serial.print("Found ");
  Serial.print(sensors.getDeviceCount(), DEC);
  Serial.println(" devices.");
  Serial.println();

  // enable or disable the email debug via Serial port
  smtp.debug(true);
  // Set the callback function to get the sending results
  smtp.callback(smtpCallback);

  pinMode(ledPin, OUTPUT);
} 

bool isAlarmActive = true;

void loop() {
  if(millis() - lastMesurementTime > mesurementInterval) {
    Serial.println();
    Serial.print(millis());
    Serial.print( " Alarm status: " + String(isAlarmActive));
    if (tempC >= HIGH_ALARM_TEMPERATURE && isAlarmActive) {
  	  isAlarmActive = false;
    	Serial.println(F(" #### Reached alarm temperature! Sending email... ####"));
      sendEmail("ALARM");
    } else if (tempC <= HIGH_ALARM_TEMPERATURE-ALARM_RESET_THRESHOLD && !isAlarmActive){
      isAlarmActive = true;
      sendEmail("ALARM_RESET");
    } 
    if(getTemperature(tempC)) {
      Serial.println(" Failed temp");
    }
    else Serial.println(" Temperature: " + String(tempC));
    lastMesurementTime = millis();
  }
  if (isAlarmActive == false) blinkLed(300, ledPin);
  else {
  	digitalWrite(ledPin, HIGH);
  	ledState = HIGH;
  	}
  //restart the system every 3 days to dump the RAM and reset the clocks
  if(millis()>259200000) {
    Serial.println(F("Restarting system . . ."));
    ESP.restart(); 
  }
}

void connectToWiFi() {
  char ssidBuff[32];
  char passwdBuff[50];
  char eapIDBuff[50];
  char eapUsernameBuff[50];
  char eapPasswordBuff[50];

  userSettings.begin("network");
  userSettings.getString("ssid").toCharArray(ssidBuff, 32);
  ssid = ssidBuff;
  if(!userSettings.getBool("isWpaEnterprise")) {
    userSettings.getString("passwd").toCharArray(passwdBuff, 50);
    passwd = passwdBuff;
    WiFi.begin(ssid, passwd);
  } else {
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
}

void smtpCallback(SMTP_Status status){ 
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

void sendEmail(String messageType) {
  // Declare the session config data
	ESP_Mail_Session session;
 	// Set the session config
  userSettings.begin("email");
	session.server.host_name = userSettings.getString("smtp_server");
	session.server.port = userSettings.getUInt("smpt_port");
	session.login.email = userSettings.getString("sender_address");
	session.login.password = userSettings.getString("email_password");
	session.login.user_domain = "";

 	// Set the NTP config time
	Serial.println(F("Getting time from NTP server..."));
	session.time.ntp_server = "pool.ntp.org";
	session.time.gmt_offset = 1;
	session.time.day_light_offset = 0;
	Serial.println(F("Done."));

  // Declare the message class
	SMTP_Message message;

  if(messageType == "ALARM") {
    // Set the message headers
    message.sender.name = userSettings.getString("author_name");
    message.sender.email = userSettings.getString("sender_address");
    message.subject = "Allarme temperatura server";
    message.addRecipient("Tecnici", userSettings.getString("email_recipient"));
    // Set the message content
    message.text.content = "La temperatura ha superato i " + String(tempC) + " °C.";
  } else if (messageType == "ALARM_RESET") {
    // Set the message headers
    message.sender.name = userSettings.getString("author_name");
    message.sender.email = userSettings.getString("sender_address");
    message.subject = "Allarme rientrato";
    message.addRecipient("Tecnici", userSettings.getString("email_recipient"));
    // Set the message content
    message.text.content = "L'ultima temperatura misurata è stata di " + String(tempC) + " °C.";
  } else return;

  smtp.connect(&session);
	// Start sending Email and close the session
	if (!MailClient.sendMail(&smtp, &message)) Serial.println("Error sending Email, " + smtp.errorReason());
}

void blinkLed(long blinkInterval, int pinNum) {
	if(millis()-lastBlink>blinkInterval) {
		digitalWrite(pinNum, !ledState);
		ledState = !ledState;
		lastBlink=millis();
		return;
	} else return;
}
