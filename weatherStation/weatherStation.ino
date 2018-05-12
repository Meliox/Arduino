#include <NTPClient.h>
#include <HTTPClient.h> // for https communication
#include <Timezone.h> // https://github.com/JChristensen/Timezone
#include <WiFi.h>
#include <ArduinoJson.h> // https://github.com/bblanchon/ArduinoJson
#include <WiFiUdp.h> // needed to get time
#include <HardwareSerial.h> // to communicate with nextion display
#include <Wire.h>
#include "Adafruit_BME280.h" //Bme280_I2C_ESP32 by Takatsuki0204

//enable code debugging below
#define DEBUG //simple debug
//#define DEBUGALL //display json response from forecast api

// Wifi configuration
const char* SSID = "";
const char* PASSWORD = "";
const char* WIFI_HOSTNAME = "";

//bme280 configuration
#define I2C_SDA 0 //SDA pin
#define I2C_SCL 4 //SDL pin
#define SEALEVELPRESSURE_HPA (1019.9)
#define BME280_ADD 0x76 //find using i2cscanner
Adafruit_BME280 bme(I2C_SDA, I2C_SCL);
const float INDOOR_TEMP_CORRECTION = 2.0;

//nextion configuration
// nextion display sdcard has to be FAT32+2048 cluster
//set baud rate for Nextion display
//nexSerial.print("bauds=115200");
//endNextionCommand();
#define RX 19 //SDA pin
#define TX 22 //SDL pin
#define NEXTION_BAUDRATE 115200
#define NEXTION_CONFIG SERIAL_8N1

// Time configuration
const char* ntpServerName = "europe.pool.ntp.org";     //server pool
const int timeZoneOffset = 1;                   //offset from Greenwich Meridan Time
TimeChangeRule myDST = { "GMT", Fourth, Sun, Mar, 2, timeZoneOffset * 60 };  //UTC+1
TimeChangeRule mySTD = { "DST", Fourth, Sun, Nov, 3, timeZoneOffset * 60 + 60 };   //UTC +2

const String URL = F("http://api.wunderground.com/api/"); //api returns localised time
const String API_KEY = F(""); //api key
											  //below is used for forecast and astromi data
const String COUNTRY_ISO3166 = F(""); //country code
const String CITY = F(""); //city
									 //below is used for current weather (local weather station)
const String PWS = F("");

const int WEATHER_CURRENT_UPDATE_TIME = 30; //time in mins
const int WEATHER_FORECAST_UPDATE_TIME = 180; //time in mins
const int SCREENSAVER_CHANGE_TIME = 600; //change time in seconds
const int TIMEOUT_TO_WEATHER_DISPLAY = 15; //timeout in seconds to return to main weather display if page is changed manually or screensaver is on
const int RETRY_UPDATE_TIME = 60; //retry time in seconds

								  //global variables
long int weatherDayLastUpdate = 0;
long int weatherForecastLastUpdate = 0;
long int weatherAstronomiLastUpdate = 0;
long int sensorLastUpdate = 0;
long int returnToWeatherMainNewTime = 0;
long int timeNewUpdate = 0;
String timeTmp = "0";
long int screensaverNewTime = 0;
Timezone myTZ(myDST, mySTD);
time_t sunriseTime;
time_t sunsetTime;

//code
const char* VERSION = "1.0"; //weather station VERSION
HardwareSerial nexSerial(2);					//hardware port used on arduino for Nextion display
WiFiUDP clockUDP;                               // initialize a UDP instance
unsigned int LOCAL_PORT = 2390;                  // local port to listen for UDP packets
IPAddress timeServerIP;                         // IP address of random server 
byte packetBuffer[48];                          // buffer to hold incoming and outgoing packets
int TIME_SERVER_DELAY = 1000;                   // delay for the time server to reply
int TIME_SERVER_PASSES = 4;                     // number of tries to connect to the time server before timing out
boolean TIME_SERVER_CONNECTED = false;          // is set to true when the time is read from the server

void setup() {
	// intialise nextion display
	initDisplay();

	// intialise wifi
	connectToWifi();

	// get time from NTP server and update local time
	initTime();

	//init sensor
	initBME280();
}

void loop() {
	updateTime(1); //update time
	getSensorData(); //get indoor data
	getWeather(); //get weather data
	screensaver(); //change to a screensaver to avoid display burn in
#ifdef DEBUG
	delay(500);
#endif
}

void initBME280() {
	bool status;
	status = bme.begin(BME280_ADD);
#ifdef DEBUG
	if (!status) {
		Serial.println("Could not find a valid BME280 sensor, check wiring!");
	}
#endif // DEBUG
}

void getSensorData() {
	if (millis() > sensorLastUpdate + 15000) {
		String tmp;

		//temperature
		float indoorTemp = bme.readTemperature(); // "22.05"
		float roundedIndoorTemp = round((indoorTemp - INDOOR_TEMP_CORRECTION) * 10) / 10.0;
		tmp = String(roundedIndoorTemp);
		tmp = tmp.substring(0, 4);
		sendToLCD(1, 1, F("indoorT"), tmp);

		//humidity
		tmp = String(round(bme.readHumidity()));
		tmp += F(" %");
		sendToLCD(1, 1, F("indoorH"), tmp);
		sensorLastUpdate = millis();
	}

}

void initTime() {
	clockUDP.begin(LOCAL_PORT);
	getTimeFromServer();
}

void initDisplay() {
	delay(5000); //wait for display to turn on
	nexSerial.begin(NEXTION_BAUDRATE, NEXTION_CONFIG, RX, TX);
	endNextionCommand();

	//reset display
	nexSerial.print(F("rest"));
	endNextionCommand();

	//set display to not to send feedback
	nexSerial.print(F("bkcmd=0"));
	endNextionCommand();

	//lower light //todo: add sensor
	nexSerial.print(F("dim=40"));
	endNextionCommand();

	//set display to loading
	String tmp = F("weather station VERSION ");
	tmp += VERSION;
	sendToLCD(0, 1, F("version"), tmp);
	setDisplay(0);
	sendToLCD(2, 1, F("version"), VERSION);
}

String doubleDigit(int number) {
	// converts a single digit into a double digit number
	String tmp;
	if (number < 10) {
		tmp += "0";
		tmp += String(number);
		return tmp;
	}
	else {
		tmp = String(number);
		return tmp;
	}
}

void screensaver() {
	//display screensaver
	if ((millis() > screensaverNewTime) && (screensaverNewTime != 0)) {
		int pageNumber = 3; //screensavers start from page 3
		pageNumber = pageNumber + random(8); //9 screensavers
		setDisplay(pageNumber);
		screensaverNewTime = millis() + SCREENSAVER_CHANGE_TIME * 1000;
		returnToWeatherMainNewTime = millis() + TIMEOUT_TO_WEATHER_DISPLAY * 1000;
	}
	else if (screensaverNewTime == 0) {
		screensaverNewTime = millis() + SCREENSAVER_CHANGE_TIME * 1000;
		returnToWeatherMainNewTime = millis() + TIMEOUT_TO_WEATHER_DISPLAY * 1000;
	}
	//display main weather page
	if (millis() > returnToWeatherMainNewTime) {
		setDisplay(1);
		returnToWeatherMainNewTime = millis() + TIMEOUT_TO_WEATHER_DISPLAY * 1000;
	}
}

void updateTime(int type) {
	// 1 updates time on display
	// 2 prints time to serial
	String timeDate;
	timeDate += doubleDigit(hour());
	timeDate += ':';
	timeDate += doubleDigit(minute());
	if (type == 1) {
		if (millis() >= timeNewUpdate) {
			timeDate += ' ';
			String tmp = dayStr(weekday());
			timeDate += tmp.substring(0, 3); //show three letter day, e.g. fri.
			timeDate += ' ';
			timeDate += String(day());
			timeDate += ". ";
			tmp = monthStr(month());
			timeDate += tmp.substring(0, 3); //show three letter month, e.g. Apr.
			sendToLCD(1, 1, "time", timeDate);
			timeNewUpdate = millis() + 60000; // next update time
		}
	}
	else if (type == 2) {
		timeDate += ':';
		timeDate += doubleDigit(second());
		timeDate += " - ";
		timeDate += doubleDigit(day());
		timeDate += '/';
		timeDate += doubleDigit(month());
		timeDate += '/';
		timeDate += year();
		timeTmp = timeDate;
	}
}

void connectToWifi() {
#ifdef DEBUG
	Serial.println();
	Serial.println();
	Serial.print("Connecting to wifi: ");
#endif	
	WiFi.begin(SSID, PASSWORD);
	WiFi.setHostname(WIFI_HOSTNAME);
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
#ifdef DEBUG
		Serial.print(".");
#endif	
	}

#ifdef DEBUG
	Serial.print("connected");
	Serial.println();
	Serial.print("IP address: ");
	Serial.println(WiFi.localIP());
#endif
	String tmp;
	tmp = WiFi.localIP().toString();
	tmp += " (";
	tmp += WIFI_HOSTNAME;
	tmp += ")";
	sendToLCD(2, 1, F("ipAddress"), tmp);
}

void getTimeFromServer() {
#ifdef DEBUG
	Serial.print("Getting time from server:");
#endif

	int connectStatus = 0, i = 0;
	unsigned long unixTime;

	while (i < TIME_SERVER_PASSES && !connectStatus) {
#ifdef DEBUG
		Serial.print(".");
#endif
		WiFi.hostByName(ntpServerName, timeServerIP);
		sendNTPpacket(timeServerIP);
		delay(TIME_SERVER_DELAY / 2);
		connectStatus = clockUDP.parsePacket();
		delay(TIME_SERVER_DELAY / 2);
		i++;
	}

	if (connectStatus) {
#ifdef DEBUG
		Serial.println("connected");
#endif
		TIME_SERVER_CONNECTED = true;
		clockUDP.read(packetBuffer, 48);

		// the timestamp starts at byte 40 of the received packet and is four bytes, or two words, long.
		unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
		unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
		// the timestamp is in seconds from 1900, add 70 years to get Unixtime
		unixTime = (highWord << 16 | lowWord) - 2208988800 + timeZoneOffset * 60 * 60;
		unixTime = myTZ.toLocal(unixTime);
		setTime(unixTime);
#ifdef DEBUG
		Serial.print("Time received: ");
		updateTime(2);
		Serial.print(timeTmp);
		Serial.println("");
#endif
	}
}

unsigned long sendNTPpacket(IPAddress& address) {
	memset(packetBuffer, 0, 48);
	packetBuffer[0] = 0b11100011;     // LI, version, Mode
	packetBuffer[1] = 0;              // Stratum, or type of clock
	packetBuffer[2] = 6;              // Polling Interval
	packetBuffer[3] = 0xEC;           // Peer Clock Precision
									  // 8 bytes of zero for Root Delay & Root Dispersion
	packetBuffer[12] = 49;
	packetBuffer[13] = 0x4E;
	packetBuffer[14] = 49;
	packetBuffer[15] = 52;

	clockUDP.beginPacket(address, 123);    //NTP requests are to port 123
	clockUDP.write(packetBuffer, 48);
	clockUDP.endPacket();
}

void setDisplay(int pageNumber) {
	nexSerial.print("page ");
	nexSerial.print(pageNumber);
	endNextionCommand();
}

void getWeather() {
	boolean booting;
	if (weatherForecastLastUpdate == 0) { // when device is booting, force change display page
		booting = true;
	}
	else {
		booting = false;
	}
	if (millis() >= weatherAstronomiLastUpdate) {
		if (getAstronomi()) {
			weatherAstronomiLastUpdate = millis() + weatherForecastLastUpdate * 60 * 1000; // next update time
		}
		else {
			weatherAstronomiLastUpdate = millis() + RETRY_UPDATE_TIME * 60 * 1000;
		}
		delay(300);
	}
	if (millis() >= weatherForecastLastUpdate) {
		if (getWeatherForecast()) {
			weatherForecastLastUpdate = millis() + WEATHER_FORECAST_UPDATE_TIME * 60 * 1000; // next update time
		}
		else {
			weatherForecastLastUpdate = millis() + RETRY_UPDATE_TIME * 60 * 1000;
		}
		delay(300);
	}
	if (millis() >= weatherDayLastUpdate) {
		if (getWeatherCurrent()) {
			weatherDayLastUpdate = millis() + WEATHER_CURRENT_UPDATE_TIME * 60 * 1000; // next update time
		}
		else {
			weatherDayLastUpdate = millis() + RETRY_UPDATE_TIME * 60 * 1000;
		}
		delay(300);
	}
	if (booting) {
		setDisplay(1);
	}
}

int getAstronomi() {
	String result;
	String tmp;
	if ((WiFi.status() == WL_CONNECTED)) {
		HTTPClient http;
		String url = URL + API_KEY + F("/astronomy/q/") + COUNTRY_ISO3166 + F("/") + CITY + F(".json");
#ifdef DEBUG
		Serial.println(url);
#endif
		http.begin(url); //Specify the URL and certificate
		int httpCode = http.GET(); //Make the request
		if (httpCode > 0) { //Check for the returning code
			result = http.getString();
#ifdef DEBUGALL
			Serial.println(httpCode);
			Serial.println(result);
#endif
		}
#ifdef DEBUG
		else {
			Serial.println(httpCode);
			Serial.println("Error on HTTP request");
		}
#endif
		http.end(); //Free the resources

		DynamicJsonBuffer jsonBuffer(1000);
		JsonObject &root = jsonBuffer.parseObject(result);
		if (root.success()) {
			JsonObject& response = root["response"];

			JsonObject& moon_phase = root["moon_phase"];
			//moon phase todo:broken
			int moon_phase_percentIlluminated = moon_phase["percentIlluminated"]; // "17"
			int moonPicture = getMoonPicture(moon_phase_percentIlluminated);
			sendToLCD(1, 3, F("moon"), String(moonPicture));

			JsonObject& sun_phase = root["sun_phase"];
			//sunrise
			int sun_phase_sunrise_hour = root["sun_phase"]["sunrise"]["hour"]; // "5"
			sun_phase_sunrise_hour = sun_phase_sunrise_hour;
			tmp += doubleDigit(sun_phase_sunrise_hour);
			tmp += ":";
			int sun_phase_sunrise_minute = root["sun_phase"]["sunrise"]["minute"]; // "07"
			tmp += doubleDigit(sun_phase_sunrise_minute);
			sendToLCD(1, 1, F("sunrise"), tmp);
			sunriseTime = tmConvert_t(year(), month(), day(), sun_phase_sunrise_hour, sun_phase_sunrise_minute, 0);

			//sunset
			int sun_phase_sunset_hour = root["sun_phase"]["sunset"]["hour"]; // "5"
			sun_phase_sunset_hour = sun_phase_sunset_hour;
			tmp = doubleDigit(sun_phase_sunset_hour);
			tmp += ":";
			int sun_phase_sunset_minute = root["sun_phase"]["sunset"]["minute"]; // "07"
			tmp += doubleDigit(sun_phase_sunset_minute);
			sendToLCD(1, 1, F("sunset"), tmp);
			sunsetTime = tmConvert_t(year(), month(), day(), sun_phase_sunset_hour, sun_phase_sunset_minute, 0);

			//last update time
			updateTime(2);
			sendToLCD(2, 1, F("timeAstronomi"), timeTmp);
			return 0;
		}
		else {
			updateTime(2);
			tmp = timeTmp;
			tmp += F("(ERROR)");
			sendToLCD(2, 1, F("timeAstronomi"), tmp);
			return 1;
#ifdef DEBUG
			Serial.println("parseObject() failed");
#endif
		}
#ifdef DEBUG
		Serial.print("Current astronomi updated");
		updateTime(2);
		Serial.print(timeTmp);
		Serial.println("");
#endif
	}
}

time_t tmConvert_t(int YYYY, byte MM, byte DD, byte hh, byte mm, byte ss)
{
	tmElements_t tmSet;
	tmSet.Year = YYYY - 1970;
	tmSet.Month = MM;
	tmSet.Day = DD;
	tmSet.Hour = hh;
	tmSet.Minute = mm;
	tmSet.Second = ss;
	return makeTime(tmSet);
}

int getWeatherCurrent() {
	String result;
	String tmp;
	if ((WiFi.status() == WL_CONNECTED)) {
		HTTPClient http;
		String url = URL + API_KEY + F("/conditions/q/pws:") + PWS + F(".json");
#ifdef DEBUG
		Serial.println(url);
#endif
		http.begin(url); //Specify the URL and certificate
		int httpCode = http.GET(); //Make the request
		if (httpCode > 0) { //Check for the returning code
			result = http.getString();
#ifdef DEBUGALL
			Serial.println(httpCode);
			Serial.println(result);
#endif
		}
#ifdef DEBUG
		else {
			Serial.println(httpCode);
			Serial.println("Error on HTTP request");
		}
#endif
		http.end(); //Free the resources

		DynamicJsonBuffer jsonBuffer(1000);
		JsonObject &root = jsonBuffer.parseObject(result);
		if (root.success()) {
			JsonObject& response = root["response"];
			JsonObject& current_observation = root["current_observation"];

			//location
			JsonObject& current_observation_display_location = current_observation["display_location"];
			String current_observation_display_location_full = current_observation_display_location["full"];
			sendToLCD(2, 1, F("location"), String(current_observation_display_location_full));

			//weather icon
			String current_observation_icon = current_observation["icon"];
			sendToLCD(1, 3, F("day0"), String(getWeatherPicture(current_observation_icon, 1)));

			//temperature
			float current_observation_temp_c = current_observation["temp_c"];
			sendToLCD(1, 1, F("dayCurrent"), String(round(current_observation_temp_c)));

			//humidity todo
			String current_observation_relative_humidity = current_observation["relative_humidity"];
			tmp = current_observation_relative_humidity.substring(0, current_observation_relative_humidity.length() - 1);
			tmp += " %";
			sendToLCD(1, 1, F("humidity"), tmp);

			//wind and direction
			float current_observation_wind_kph = current_observation["wind_kph"];
			int current_observation_wind_degrees = current_observation["wind_degrees"];
			tmp = String(round(current_observation_wind_kph * 1000 / 3600));
			tmp += " m/s from ";
			tmp += getShortWindDirection(current_observation_wind_degrees);
			sendToLCD(1, 1, F("windADir"), tmp);

			//display pressure
			float current_observation_pressure_mb = current_observation["pressure_mb"];
			tmp = String(round(current_observation_pressure_mb));
			tmp += " hPa";
			sendToLCD(1, 1, F("pressure"), tmp);

			//UV index
			int current_observation_UV = current_observation["UV"];
			tmp = String(current_observation_UV);
			sendToLCD(1, 1, F("UVindex"), tmp);

			//last update time
			time_t current_observation_observation_epoch = current_observation["observation_epoch"]; // "1526054880"
			tmp = String(doubleDigit(hour(current_observation_observation_epoch)));
			tmp += ":";
			tmp += String(doubleDigit(minute(current_observation_observation_epoch)));
			tmp += ":";
			tmp += String(doubleDigit(second(current_observation_observation_epoch)));
			tmp += " - ";
			tmp += String(doubleDigit(day(current_observation_observation_epoch)));
			tmp += "/";
			tmp += String(doubleDigit(month(current_observation_observation_epoch)));
			tmp += "/";
			tmp += String(doubleDigit(year(current_observation_observation_epoch)));
			sendToLCD(2, 1, F("timeCurrent"), tmp);
			return 0;
		}
		else {
			updateTime(2);
			tmp = timeTmp;
			tmp += F("(ERROR)");
			sendToLCD(2, 1, F("timeCurrent"), tmp);
			return 1;
#ifdef DEBUG
			Serial.println("parseObject() failed");
#endif
		}
#ifdef DEBUG
		Serial.print("Current weather updated");
		updateTime(2);
		Serial.print(timeTmp);
		Serial.println("");
#endif
	}
}

int getWeatherForecast() {
	String result;
	String tmp;
	if ((WiFi.status() == WL_CONNECTED)) {
		HTTPClient http;
		String url = URL + API_KEY + F("/forecast/q/") + COUNTRY_ISO3166 + F("/") + CITY + F(".json");
#ifdef DEBUG
		Serial.println(url);
#endif
		http.begin(url); //Specify the URL and certificate
		int httpCode = http.GET(); //Make the request
		if (httpCode > 0) { //Check for the returning code
			result = http.getString();
#ifdef DEBUGALL
			Serial.println(httpCode);
			Serial.println(result);
#endif
		}
#ifdef DEBUG
		else {
			Serial.println(httpCode);
			Serial.println("Error on HTTP request");
		}
#endif
		http.end(); //Free the resources

		DynamicJsonBuffer jsonBuffer(1000);
		JsonObject &root = jsonBuffer.parseObject(result);
		if (root.success()) {
			JsonObject& response = root["response"];

			JsonArray& forecast_simpleforecast_forecastday = root["forecast"]["simpleforecast"]["forecastday"];

			//day+0
			JsonObject& forecast_simpleforecast_forecastday0 = forecast_simpleforecast_forecastday[0];

			int forecast_simpleforecast_forecastday0_high_celsius = forecast_simpleforecast_forecastday0["high"]["celsius"]; // "19"
			sendToLCD(1, 1, F("day0high"), String(forecast_simpleforecast_forecastday0_high_celsius));
			int forecast_simpleforecast_forecastday0_low_celsius = forecast_simpleforecast_forecastday0["low"]["celsius"]; // "9"
			sendToLCD(1, 1, F("day0low"), String(forecast_simpleforecast_forecastday0_low_celsius));

			//rain todo: handle snow
			float forecast_simpleforecast_forecastday0_qpf_night_mm = forecast_simpleforecast_forecastday0["qpf_allday"]["mm"]; // 0
			if (forecast_simpleforecast_forecastday0_qpf_night_mm != 0) {
				tmp = F("Rain ");
				tmp += round(forecast_simpleforecast_forecastday0_high_celsius);
				tmp += F(" mm");

			}
			else {
				tmp = "";
			}
			sendToLCD(1, 1, F("precipitation"), tmp);

			//day+1
			JsonObject& forecast_simpleforecast_forecastday1 = forecast_simpleforecast_forecastday[1];
			String forecast_simpleforecast_forecastday1_date_weekday_short = forecast_simpleforecast_forecastday1["date"]["weekday_short"]; // "Sat"
			sendToLCD(1, 1, F("day1name"), forecast_simpleforecast_forecastday1_date_weekday_short);

			int forecast_simpleforecast_forecastday1_high_celsius = forecast_simpleforecast_forecastday1["high"]["celsius"]; // "19"
			sendToLCD(1, 1, F("day1high"), String(forecast_simpleforecast_forecastday1_high_celsius));
			int forecast_simpleforecast_forecastday1_low_celsius = forecast_simpleforecast_forecastday1["low"]["celsius"]; // "9"
			sendToLCD(1, 1, F("day1low"), String(forecast_simpleforecast_forecastday1_low_celsius));
			String forecast_simpleforecast_forecastday1_icon = forecast_simpleforecast_forecastday1["icon"]; // "clear"
			sendToLCD(1, 3, F("day1"), String(getWeatherPicture(forecast_simpleforecast_forecastday1_icon, 2)));

			//day+2
			JsonObject& forecast_simpleforecast_forecastday2 = forecast_simpleforecast_forecastday[2];
			String forecast_simpleforecast_forecastday2_date_weekday_short = forecast_simpleforecast_forecastday2["date"]["weekday_short"]; // "Sat"
			sendToLCD(1, 1, F("day2name"), forecast_simpleforecast_forecastday2_date_weekday_short);

			int forecast_simpleforecast_forecastday2_high_celsius = forecast_simpleforecast_forecastday2["high"]["celsius"]; // "19"
			sendToLCD(1, 1, F("day2high"), String(forecast_simpleforecast_forecastday2_high_celsius));
			int forecast_simpleforecast_forecastday2_low_celsius = forecast_simpleforecast_forecastday2["low"]["celsius"]; // "9"
			sendToLCD(1, 1, F("day2low"), String(forecast_simpleforecast_forecastday2_low_celsius));
			String forecast_simpleforecast_forecastday2_icon = forecast_simpleforecast_forecastday2["icon"]; // "clear"
			sendToLCD(1, 3, F("day2"), String(getWeatherPicture(forecast_simpleforecast_forecastday2_icon, 2)));

			//day+3
			JsonObject& forecast_simpleforecast_forecastday3 = forecast_simpleforecast_forecastday[3];
			String forecast_simpleforecast_forecastday3_date_weekday_short = forecast_simpleforecast_forecastday3["date"]["weekday_short"]; // "Sat"
			sendToLCD(1, 1, F("day3name"), forecast_simpleforecast_forecastday3_date_weekday_short);

			int forecast_simpleforecast_forecastday3_high_celsius = forecast_simpleforecast_forecastday3["high"]["celsius"]; // "19"
			sendToLCD(1, 1, F("day3high"), String(forecast_simpleforecast_forecastday3_high_celsius));
			int forecast_simpleforecast_forecastday3_low_celsius = forecast_simpleforecast_forecastday3["low"]["celsius"]; // "9"
			sendToLCD(1, 1, F("day3low"), String(forecast_simpleforecast_forecastday3_low_celsius));
			String forecast_simpleforecast_forecastday3_icon = forecast_simpleforecast_forecastday3["icon"]; // "clear"
			sendToLCD(1, 3, F("day3"), String(getWeatherPicture(forecast_simpleforecast_forecastday3_icon, 2)));

			//last update time
			updateTime(2);
			sendToLCD(2, 1, F("timeForecast"), timeTmp);
			return 0;
		}
		else {
			updateTime(2);
			tmp = timeTmp;
			tmp += F("(ERROR)");
			sendToLCD(2, 1, F("timeForecast"), tmp);
			return 1;
#ifdef DEBUG
			Serial.println("parseObject() failed");
#endif
		}
#ifdef DEBUG
		Serial.print("Forecast weather updated");
		updateTime(2);
		Serial.print(timeTmp);
		Serial.println("");
#endif
	}
}

String getShortWindDirection(int degrees) {
	int sector = ((degrees + 11) / 22.5 - 1);
	switch (sector) {
	case 0: return F("N");
	case 1: return F("NNE");
	case 2: return F("NE");
	case 3: return F("ENE");
	case 4: return F("E");
	case 5: return F("ESE");
	case 6: return F("SE");
	case 7: return F("SSE");
	case 8: return F("S");
	case 9: return F("SSW");
	case 10: return F("SW");
	case 11: return F("WSW");
	case 12: return F("W");
	case 13: return F("WNW");
	case 14: return F("NW");
	case 15: return F("NNW");
	}
}

int getMoonPicture(int percentage) { // calculate whether moon increases or decreases by comparing today with tomorrow
	percentage = 100 - percentage;
	if (percentage <= 2) {
		return 33; //new moon
	}
	else if (percentage <= 7) {
		return 34;
	}
	else if (percentage <= 12) {
		return 35;
	}
	else if (percentage <= 17) {
		return 36;
	}
	else if (percentage <= 22) {
		return 37;
	}
	else if (percentage <= 27) {
		return 38;
	}
	else if (percentage <= 32) {
		return 39;
	}
	else if (percentage <= 37) {
		return 40;
	}
	else if (percentage <= 42) {
		return 41;
	}
	else if (percentage <= 47) {
		return 42;
	}
	else if (percentage <= 52) {
		return 43; // full moon
	}
	else if (percentage <= 57) {
		return 44;
	}
	else if (percentage <= 62) {
		return 45;
	}
	else if (percentage <= 67) {
		return 46;
	}
	else if (percentage <= 72) {
		return 47;
	}
	else if (percentage <= 77) {
		return 48;
	}
	else if (percentage <= 82) {
		return 49;
	}
	else if (percentage <= 87) {
		return 50;
	}
	else if (percentage <= 92) {
		return 51;
	}
	else if (percentage <= 97) {
		return 52;
	}
}

int getWeatherPicture(String icon, int size) {
	// size = 1, 128x128
	// size = 2, 64x64
	if (size == 1) {
		//find out weather it is day or night
		if ((now() >= sunriseTime && now() <= sunsetTime) || sunriseTime == 0) {
			if (icon == F("chanceofflurries")) {
				return 15; //todo
			}
			else if (icon == F("chanceofrain")) {
				return 16; //todo
			}
			else if (icon == F("chanceofsleet")) {
				return 17; //todo
			}
			else if (icon == F("chanceofsnow")) {
				return 18; //todo
			}
			else if (icon == F("chanceofthunderstorm")) {
				return 19; //todo
			}
			else if (icon == F("clear")) {
				return 20;
			}
			else if (icon == F("cloudy")) {
				return 21;
			}
			else if (icon == F("flurries")) {
				return 15; //todo
			}
			else if (icon == F("hazy")) {
				return 22;
			}
			else if (icon == F("mostlycloudy")) {
				return 23;
			}
			else if (icon == F("mostlysunny")) {
				return 24; //todo
			}
			else if (icon == F("partlycloudy")) {
				return 25;
			}
			else if (icon == F("partlysunny")) {
				return 25; //todo
			}
			else if (icon == F("rain")) {
				return 26;
			}
			else if (icon == F("sleet")) {
				return 17;
			}
			else if (icon == F("snow")) {
				return 18;
			}
			else if (icon == F("sunny")) {
				return 20;
			}
			else if (icon == F("thunderstorm")) {
				return 19;
			}
			else if (icon == F("unknown")) {
				return 27;
			}
			else {
				return 27; //unknown - not supported
			}
		}
		else { //night
			if (icon == F("chanceofflurries")) {
				return 15; //todo
			}
			else if (icon == F("chanceofrain")) {
				return 16; //todo
			}
			else if (icon == F("chanceofsleet")) {
				return 17; //todo
			}
			else if (icon == F("chanceofsnow")) {
				return 18; //todo
			}
			else if (icon == F("chanceofthunderstorm")) {
				return 19; //todo
			}
			else if (icon == F("clear")) {
				return 28;
			}
			else if (icon == F("cloudy")) {
				return 21;
			}
			else if (icon == F("flurries")) {
				return 15; //todo
			}
			else if (icon == F("hazy")) {
				return 29;
			}
			else if (icon == F("mostlycloudy")) {
				return 30;
			}
			else if (icon == F("mostlysunny")) {
				return 31; //todo
			}
			else if (icon == F("partlycloudy")) {
				return 32;
			}
			else if (icon == F("partlysunny")) {
				return 32; //todo
			}
			else if (icon == F("rain")) {
				return 26;
			}
			else if (icon == F("sleet")) {
				return 17;
			}
			else if (icon == F("snow")) {
				return 18;
			}
			else if (icon == F("sunny")) {
				return 28;
			}
			else if (icon == F("thunderstorm")) {
				return 19;
			}
			else if (icon == F("unknown")) {
				return 27;
			}
			else {
				return 27; //unknown - not supported
			}
		}
	}
	else if (size == 2) {
		if (icon == F("chanceofflurries")) {
			return 0;
		}
		else if (icon == F("chanceofrain")) {
			return 1;
		}
		else if (icon == F("chanceofsleet")) {
			return 11;
		}
		else if (icon == F("chanceofsnow")) {
			return 2;
		}
		else if (icon == F("chanceofthunderstorm")) {
			return 3;
		}
		else if (icon == F("clear")) {
			return 4;
		}
		else if (icon == F("cloudy")) {
			return 5;
		}
		else if (icon == F("flurries")) {
			return 0;
		}
		else if (icon == F("hazy")) {
			return 6; //hertil
		}
		else if (icon == F("mostlycloudy")) {
			return 7;
		}
		else if (icon == F("mostlysunny")) {
			return 8; //todo
		}
		else if (icon == F("partlycloudy")) {
			return 9;
		}
		else if (icon == F("partlysunny")) {
			return 9; //todo
		}
		else if (icon == F("rain")) {
			return 10;
		}
		else if (icon == F("sleet")) {
			return 11;
		}
		else if (icon == F("snow")) {
			return 12;
		}
		else if (icon == F("sunny")) {
			return 4;
		}
		else if (icon == F("thunderstorm")) {
			return 3;
		}
		else if (icon == F("unknown")) {
			return 13;
		}
		else {
			return 13; //unknown - not supported
		}
	}
}

void endNextionCommand() {
	nexSerial.write(0xff);
	nexSerial.write(0xff);
	nexSerial.write(0xff);
}

void sendToLCD(uint8_t page, uint8_t type, String index, String cmd) {
	String tmp;
	tmp = "page";
	tmp += String(page);
	tmp += ".";
	tmp += index;
	if (type == 1) {
		tmp += F(".txt=\"");
		tmp += cmd;
		tmp += "\"";
	}
	else if (type == 2) {
		tmp += F(".txt=\"");
		tmp += cmd;
	}
	else if (type == 3) {
		tmp += F(".pic=");
		tmp += cmd;
	}
	nexSerial.print(tmp);
	endNextionCommand();
#ifdef DEBUG
	Serial.print("To display: ");
	Serial.println(tmp);
#endif
}