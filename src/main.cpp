/*
### Temperature monitor with email alarm notification ###

Written by Matteo Visintini for ITS Alessandro Volta - Trieste

Support for both WPA2 Personal and WPA2 Enterprise WiFi security
*/

#include <Arduino.h>
#include <WiFi.h>
#include "esp_wpa2.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP_Mail_Client.h>

/* 
##############################
##### USER CONFIGURATION #####
##############################
*/
//WiFi CONFIGURATION
const char *ssid = "";  //WiFi ssid
const char *passwd = "";  //WiFi password. Leave empty if using WPA2 enterprise 
const bool isWPAenterprise = false;  //Set to TRUE when using WPA2 Enterprise access to the network

#define EAP_ID ""   //Enterprise WiFi credentials. Leave empty when not using WPA2 Enterprise
#define EAP_USERNAME ""   //Enterprise WiFi credentials. Leave empty when not using WPA2 Enterprise
#define EAP_PASSWORD ""   //Enterprise WiFi credentials. Leave empty when not using WPA2 Enterprise
//TEMPERATURE CONFIGURATION
const float HIGH_ALARM_TEMPERATURE = 24;    //temperature above wich the alarm sends a notify via email
const float ALARM_RESET_THRESHOLD = 1.5;    //temperature threshold subtracted to the alarm threshold under which the alarm is reactivated
const long mesurementInterval = 60000;    //time intervall (microseconds) beetween mesurements
//EMAIL CONFIGURATION
#define SMTP_PORT 465
const char *smtp_server = "smtp.gmail.com";
const char *email_author_name = "ESP32 - Server temp monitor";
const char *email_author = "";
const char *email_password = "";
const char *email_recipient = "";
/*
#############################
##### CONFIGURATION END #####
#############################
*/


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
  Serial.println();

  Serial.print("Connecting to WiFi");

  if(isWPAenterprise) {
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
  else WiFi.begin(ssid, passwd);
  
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


// ####### DEBUG CALLBACK FUNCTION #######
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
	session.server.host_name = smtp_server;
	session.server.port = SMTP_PORT;
	session.login.email = email_author;
	session.login.password = email_password;
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
    message.sender.name = email_author_name;
    message.sender.email = email_author;
    message.subject = "Allarme temperatura server";
    message.addRecipient("Matteo", email_recipient);
    // Set the message content
    message.text.content = "La temperatura ha superato i " + String(tempC) + " °C.";
  } else if (messageType == "ALARM_RESET") {
    // Set the message headers
    message.sender.name = email_author_name;
    message.sender.email = email_author;
    message.subject = "Allarme rientrato";
    message.addRecipient("Matteo", email_recipient);
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
