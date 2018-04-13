#include <NTPClient.h>
#include <HTTPClient.h> // for https communication
#include <Timezone.h>
#include <Time.h> //maybe not needed
#include <WiFi.h>
#include <ArduinoJson.h> // https://github.com/bblanchon/ArduinoJson
#include <WiFiUdp.h> // needed to get time
#include <HardwareSerial.h> // to communicate with nextion display

// nextion display sdcard has to be FAT32+2048 cluster

#define DEBUG
//#define DEBUGALL

//constants
const char* version = "1.0";

// Wifi configuration
const char* ssid = "";
const char* password = "";
const char* wifiHostname = "";

//WiFiServer server(80);

// Time configuration, e.g. NTP server
unsigned int localPort = 2390;                  // local port to listen for UDP packets
IPAddress timeServerIP;                         // IP address of random server 
const char* ntpServerName = "europe.pool.ntp.org";     // server pool
byte packetBuffer[48];                          // buffer to hold incoming and outgoing packets
const int timeZoneOffset = 60;                   // offset from Greenwich Meridan Time
TimeChangeRule myDST = { "GMT", Fourth, Sun, Mar, 2, timeZoneOffset };  //UTC+1
TimeChangeRule mySTD = { "DST", Fourth, Sun, Nov, 3, timeZoneOffset+60 };   //UTC +2

WiFiUDP clockUDP;                               // initialize a UDP instance
const String apiURL = F("https://api.darksky.net/forecast/");
const String APIKEY = F("");
const String location = F("55.67,12.56");
const String unit = F("si");

const char* ca_cert = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIElDCCA3ygAwIBAgIQAf2j627KdciIQ4tyS8+8kTANBgkqhkiG9w0BAQsFADBh\n" \
"MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n" \
"d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD\n" \
"QTAeFw0xMzAzMDgxMjAwMDBaFw0yMzAzMDgxMjAwMDBaME0xCzAJBgNVBAYTAlVT\n" \
"MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxJzAlBgNVBAMTHkRpZ2lDZXJ0IFNIQTIg\n" \
"U2VjdXJlIFNlcnZlciBDQTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEB\n" \
"ANyuWJBNwcQwFZA1W248ghX1LFy949v/cUP6ZCWA1O4Yok3wZtAKc24RmDYXZK83\n" \
"nf36QYSvx6+M/hpzTc8zl5CilodTgyu5pnVILR1WN3vaMTIa16yrBvSqXUu3R0bd\n" \
"KpPDkC55gIDvEwRqFDu1m5K+wgdlTvza/P96rtxcflUxDOg5B6TXvi/TC2rSsd9f\n" \
"/ld0Uzs1gN2ujkSYs58O09rg1/RrKatEp0tYhG2SS4HD2nOLEpdIkARFdRrdNzGX\n" \
"kujNVA075ME/OV4uuPNcfhCOhkEAjUVmR7ChZc6gqikJTvOX6+guqw9ypzAO+sf0\n" \
"/RR3w6RbKFfCs/mC/bdFWJsCAwEAAaOCAVowggFWMBIGA1UdEwEB/wQIMAYBAf8C\n" \
"AQAwDgYDVR0PAQH/BAQDAgGGMDQGCCsGAQUFBwEBBCgwJjAkBggrBgEFBQcwAYYY\n" \
"aHR0cDovL29jc3AuZGlnaWNlcnQuY29tMHsGA1UdHwR0MHIwN6A1oDOGMWh0dHA6\n" \
"Ly9jcmwzLmRpZ2ljZXJ0LmNvbS9EaWdpQ2VydEdsb2JhbFJvb3RDQS5jcmwwN6A1\n" \
"oDOGMWh0dHA6Ly9jcmw0LmRpZ2ljZXJ0LmNvbS9EaWdpQ2VydEdsb2JhbFJvb3RD\n" \
"QS5jcmwwPQYDVR0gBDYwNDAyBgRVHSAAMCowKAYIKwYBBQUHAgEWHGh0dHBzOi8v\n" \
"d3d3LmRpZ2ljZXJ0LmNvbS9DUFMwHQYDVR0OBBYEFA+AYRyCMWHVLyjnjUY4tCzh\n" \
"xtniMB8GA1UdIwQYMBaAFAPeUDVW0Uy7ZvCj4hsbw5eyPdFVMA0GCSqGSIb3DQEB\n" \
"CwUAA4IBAQAjPt9L0jFCpbZ+QlwaRMxp0Wi0XUvgBCFsS+JtzLHgl4+mUwnNqipl\n" \
"5TlPHoOlblyYoiQm5vuh7ZPHLgLGTUq/sELfeNqzqPlt/yGFUzZgTHbO7Djc1lGA\n" \
"8MXW5dRNJ2Srm8c+cftIl7gzbckTB+6WohsYFfZcTEDts8Ls/3HB40f/1LkAtDdC\n" \
"2iDJ6m6K7hQGrn2iWZiIqBtvLfTyyRRfJs8sjX7tN8Cp1Tm5gr8ZDOo0rwAhaPit\n" \
"c+LJMto4JQtV05od8GiG7S5BNO98pVAdvzr508EIDObtHopYJeS4d60tbvVS3bR0\n" \
"j6tJLp07kzQoH3jOlOrHvdPJbRzeXDLz\n" \
"-----END CERTIFICATE-----\n";

int timeServerDelay = 1000;                   // delay for the time server to reply
int timeServerPasses = 4;                     // number of tries to connect to the time server before timing out
											  //int timeServerResyncNumOfLoops = 3000;        // number of loops before refreshing the time. one loop takes approx. 28 seconds
											  //int timeServerResyncNumOfLoopsCounter = 0;
boolean timeServerConnected = false;          // is set to true when the time is read from the server

const int weatherDayUpdateTime = 900000; //time in ms
const int timeUpdateTime = 60000; //time in ms
const int screensaverChangeTime = 60000; // 10mins
const int screensaverOnTime = 15000; // 15 seconds

//hardware stuff
HardwareSerial nexSerial(2);

//global variables
long int weatherDayLastUpdate = 0;
long int timeNewUpdate = 0;
String timeTmp = "0";
long int screensaverNewTime = 0;
Timezone myTZ(myDST, mySTD);

void setup() {
	// intialise nextion display
	initDisplay();
	
	// intialise wifi
	connectToWifi();

	// get time from NTP server and update local time
	initTime();
}

void loop() {
	updateTime(1);
	getWeather();
	//change to a screensaver every X mins to avoid display burn in
	//screensaver();
#ifdef DEBUG
	delay(500);
#endif
}

void initTime() {
	clockUDP.begin(localPort);
	getTimeFromServer();
}

void initDisplay() {
	nexSerial.begin(115200, SERIAL_8N1, 19, 22);
	endNextionCommand();
	//set baud rate for Nextion display
	//nexSerial.print("bauds=115200");
	//endNextionCommand();
		//set display to send to feedback
	nexSerial.print(F("bkcmd=0"));
	endNextionCommand();
		//lower light //todo: add sensor
	nexSerial.print(F("dim=50"));
	endNextionCommand();
	//set display to loading
	String tmp = F("weather station version ");
	tmp += version;
	sendToLCD(0, 1, F("t0"), tmp);
	setDisplay(0);
	sendToLCD(2, 1, F("version"), version);
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
	if (millis() > screensaverNewTime && screensaverNewTime != 0) {
		setDisplay(3);
		Serial.println(1);
	}
	else if (screensaverNewTime == 0) {
		screensaverNewTime = millis() + screensaverChangeTime;
		Serial.println(2);
	}
	else if (millis() > screensaverOnTime + screensaverNewTime - screensaverChangeTime) {
		setDisplay(1);
		Serial.println(3);
	}
}

void updateTime(int type){
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
		timeNewUpdate = millis() + timeUpdateTime; // next update time
		}
	}
	else if (type == 2) {
		timeDate += ':';
		timeDate += doubleDigit(second());
		timeDate += ' ';
		timeDate += '-';
		timeDate += day();
		timeDate += '/';
		timeDate += month();
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
	WiFi.begin(ssid, password);
	WiFi.setHostname(wifiHostname);
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
	tmp += wifiHostname;
	tmp += ")";
	sendToLCD(2, 1, F("ipAddress"), tmp);
}

void getTimeFromServer() {
#ifdef DEBUG
	Serial.print("Getting time from server:");
#endif

	int connectStatus = 0, i = 0;
	unsigned long unixTime;

	while (i < timeServerPasses && !connectStatus) {
#ifdef DEBUG
	Serial.print(".");
#endif
		WiFi.hostByName(ntpServerName, timeServerIP);
		sendNTPpacket(timeServerIP);
		delay(timeServerDelay / 2);
		connectStatus = clockUDP.parsePacket();
		delay(timeServerDelay / 2);
		i++;
	}

	if (connectStatus) {
#ifdef DEBUG
	Serial.println("connected");
#endif
		timeServerConnected = true;
		clockUDP.read(packetBuffer, 48);

		// the timestamp starts at byte 40 of the received packet and is four bytes, or two words, long.
		unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
		unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
		// the timestamp is in seconds from 1900, add 70 years to get Unixtime
		unixTime = (highWord << 16 | lowWord) - 2208988800 + timeZoneOffset*60;
		unixTime = myTZ.toLocal(unixTime);
		setTime(unixTime);
#ifdef DEBUG
		Serial.print("Time received: ");
		currentTime();
#endif
	}
}

unsigned long sendNTPpacket(IPAddress& address) {
	memset(packetBuffer, 0, 48);
	packetBuffer[0] = 0b11100011;     // LI, Version, Mode
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

void getWeather(){
	if (millis() >= weatherDayLastUpdate ) {
		getWeatherCurrent();
		getWeatherDataRain();
		getWeatherDataForecast();
		setDisplay(1);
		weatherDayLastUpdate = millis() + weatherDayUpdateTime; // next update time
	}
}

void getWeatherCurrent(){
	String result;
	String tmp;
	if ((WiFi.status() == WL_CONNECTED)) {
		HTTPClient http;
		String url = apiURL + APIKEY + "/" + location + F("?units=") + unit + F("&exclude=flags,alerts,minutely,daily,hourly"); // current
#ifdef DEBUG
		Serial.println(url);
#endif
		http.begin(url, ca_cert); //Specify the URL and certificate
		int httpCode = http.GET();                                                  //Make the request
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

		StaticJsonBuffer<1000> json_buf;
		JsonObject &root = json_buf.parseObject(result);
		if (root.success()) {
			String tmp0 = root["currently"]["icon"]; //weather icon
			float tmp1 = root["currently"]["temperature"]; //temperature
			float tmp2 = root["currently"]["humidity"]; //humidity
			float tmp3 = root["currently"]["pressure"]; //pressure
			float tmp4 = root["currently"]["windSpeed"]; //windSpeed
			float tmp8 = root["currently"]["windGust"]; //windGust
			int tmp5 = root["currently"]["uvIndex"]; //uvIndex
			float tmp6 = root["currently"]["windBearing"]; //windBearing
			float tmp9 = root["currently"]["apparentTemperature"]; //apparentTemperature
			int tmp7 = root["currently"]["ozone"]; //ozone
			String timezone = root["timezone"];; //location

			//temperature (feels like)
			tmp = "(";
			tmp += round(tmp9);
			tmp += ")";
			sendToLCD(1, 1, F("dayFeelsLike"), tmp);

			//location
			sendToLCD(2, 1, F("location"), String(timezone));

			//weather icon
			sendToLCD(1, 3, F("day0"), String(getWeatherPicture(tmp0, 1)));

			//temperature
			sendToLCD(1, 1, F("dayCurrent"), String(round(tmp1)));

			//wind and direction
			tmp = String(round(tmp4));
			tmp += "(";
			tmp += String(round(tmp8));
			tmp += ")";
			tmp += "m/s - ";
			tmp += getShortWindDirection(tmp6);
			sendToLCD(1, 1, F("windADir"), tmp);

			//humidity
			tmp = String(round(tmp2));
			tmp += " %";
			sendToLCD(1, 1, F("humidity"), tmp);

			//UV index
			tmp = String(tmp5);
			tmp += " %";
			sendToLCD(1, 1, F("UVindex"), tmp);

			//ozone
			tmp = String(round(tmp7));
			tmp += " DU";
			sendToLCD(1, 1, F("ozone"), tmp);

			//display pressure
			tmp = String(round(tmp3));
			tmp += " hPa";
			sendToLCD(1, 1, F("pressure"), tmp);

			//last update time
			updateTime(2);
			sendToLCD(2, 1, F("timeCurrent"), timeTmp);
		}
		else {
			updateTime(2);
			tmp = timeTmp;
			tmp += F("(ERROR)");
			sendToLCD(2, 1, F("timeCurrent"), timeTmp);
#ifdef DEBUG
			Serial.println("parseObject() failed");
#endif
		}
#ifdef DEBUG
		Serial.print("Current weather updated");
		currentTime();
#endif
	}
}

void getWeatherDataRain() {
	String tmp;
	String result;
	if ((WiFi.status() == WL_CONNECTED)) {
		HTTPClient http;
		String url = apiURL + APIKEY + "/" + location + F("?units=") + unit + F("&exclude=flags,alerts,minutely,daily,currently"); // hourly
#ifdef DEBUG
		Serial.println(url);
#endif
		http.begin(url, ca_cert); //Specify the URL and certificate
		int httpCode = http.GET();                                                  //Make the request
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
			float precipAccumulation = 0;

			//todo: rewrite to loop
			JsonObject& hourly = root["hourly"];
			JsonArray& hourly_data = hourly["data"];
			JsonObject& hourly_data0 = hourly_data[0];
			float hourly_data0_precipIntensity = hourly_data0["precipIntensity"];
			precipAccumulation = precipAccumulation + hourly_data0_precipIntensity;
			JsonObject& hourly_data1 = hourly_data[1];
			float hourly_data1_precipIntensity = hourly_data1["precipIntensity"];
			precipAccumulation = precipAccumulation + hourly_data1_precipIntensity;
			JsonObject& hourly_data2 = hourly_data[2];
			float hourly_data2_precipIntensity = hourly_data2["precipIntensity"];
			precipAccumulation = precipAccumulation + hourly_data2_precipIntensity;
			JsonObject& hourly_data3 = hourly_data[3];
			float hourly_data3_precipIntensity = hourly_data3["precipIntensity"];
			precipAccumulation = precipAccumulation + hourly_data3_precipIntensity;
			JsonObject& hourly_data4 = hourly_data[4];
			float hourly_data4_precipIntensity = hourly_data4["precipIntensity"];
			precipAccumulation = precipAccumulation + hourly_data4_precipIntensity;
			JsonObject& hourly_data5 = hourly_data[5];
			float hourly_data5_precipIntensity = hourly_data5["precipIntensity"];
			precipAccumulation = precipAccumulation + hourly_data5_precipIntensity;
			tmp += round(precipAccumulation);
			tmp += F(" mm");
			sendToLCD(1, 1, F("precipitation"), tmp);

			//last update time
			updateTime(2);
			sendToLCD(2, 1, F("timeRain"), timeTmp);
		}
		else {
			updateTime(2);
			tmp = timeTmp;
			tmp += F("(ERROR)");
			sendToLCD(2, 1, F("timeCurrent"), timeTmp);
#ifdef DEBUG
			Serial.println("parseObject() failed");
#endif
		}
#ifdef DEBUG
		Serial.print("Rain forecast updated");
		currentTime();
#endif
	}
}

void getWeatherDataForecast() {
	String tmp, result;
	int tmp1;
	float tmp2;
	long int tmp3;
	if ((WiFi.status() == WL_CONNECTED)) {
		HTTPClient http;
		String url = apiURL + APIKEY + "/" + location + F("?units=") + unit + F("&exclude=flags,alerts,minutely,hourly,currently"); // daily
#ifdef DEBUG
		Serial.println(url);
#endif
		http.begin(url, ca_cert); //Specify the URL and certificate
		int httpCode = http.GET();                                                  //Make the request
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
			JsonObject& daily = root["daily"];
			JsonArray& daily_data = daily["data"];
			JsonObject& daily_data0 = daily_data[0];
			JsonObject& daily_data1 = daily_data[1];
			JsonObject& daily_data2 = daily_data[2];
			JsonObject& daily_data3 = daily_data[3];
			
			//day0
			float daily_data0_moonPhase = daily_data0["moonPhase"]; //moon phase
			int moonPicture = getMoonPicture(daily_data0_moonPhase);
			sendToLCD(1, 3, F("moon"), String(moonPicture));

			tmp3 = daily_data0["sunriseTime"]; // sunrise
			tmp3 = tmp3 + timeZoneOffset * 60;
			tmp3 = myTZ.toLocal(tmp3);
			tmp1 = hour(tmp3);
			tmp += doubleDigit(tmp1);
			tmp += ":";
			tmp1 = minute(tmp3);
			tmp += doubleDigit(tmp1);
			sendToLCD(1, 1, F("sunrise"), tmp);

			tmp3 = daily_data0["sunsetTime"]; // sunset
			tmp3 = tmp3 + timeZoneOffset * 60;
			tmp3 = myTZ.toLocal(tmp3);
			tmp1 = hour(tmp3);
			tmp = doubleDigit(tmp1);
			tmp += ":";
			tmp1 = minute(tmp3);
			tmp += doubleDigit(tmp1);
			sendToLCD(1, 1, F("sunset"), tmp);

			tmp2 = daily_data0["temperatureHigh"];
			sendToLCD(1, 1, "day0high", String(round(tmp2)));
			tmp2 = daily_data0["temperatureLow"]; 
			sendToLCD(1, 1, "day0low", String(round(tmp2)));

			// day1
			tmp3 = daily_data1["time"]; //dayname
			tmp3 = tmp3 + timeZoneOffset * 60;
			tmp3 = myTZ.toLocal(tmp3);
			tmp2 = weekday(tmp3);
			tmp = dayStr(tmp2);
			tmp = String(tmp.substring(0, 3));
			sendToLCD(1, 1, F("day1name"), tmp);
			String tmp4 = daily_data1["icon"];
			sendToLCD(1, 3, F("day1"), String(getWeatherPicture(tmp4,2)));
			tmp2 = daily_data1["temperatureHigh"];
			sendToLCD(1, 1, F("day1high"), String(round(tmp2)));
			tmp2 = daily_data1["temperatureLow"];
			sendToLCD(1, 1, F("day1low"), String(round(tmp2)));

			//day2
			tmp3 = daily_data2["time"]; //dayname
			tmp3 = tmp3 + timeZoneOffset * 60;
			tmp3 = myTZ.toLocal(tmp3);
			tmp2 = weekday(tmp3);
			tmp = dayStr(tmp2);
			tmp = String(tmp.substring(0, 3));
			sendToLCD(1, 1, F("day2name"), tmp);
			String tmp5 = daily_data2["icon"];
			sendToLCD(1, 3, F("day2"), String(getWeatherPicture(tmp5,2)));
			tmp2 = daily_data2["temperatureHigh"];
			sendToLCD(1, 1, F("day2high"), String(round(tmp2)));
			tmp2 = daily_data2["temperatureLow"];
			sendToLCD(1, 1, "day2low", String(round(tmp2)));

			//day3
			tmp3 = daily_data3["time"]; //dayname
			tmp3 = tmp3 + timeZoneOffset * 60;
			tmp3 = myTZ.toLocal(tmp3);
			tmp2 = weekday(tmp3);
			tmp = dayStr(tmp2);
			tmp = String(tmp.substring(0, 3));
			sendToLCD(1, 1, F("day3name"), tmp);
			String tmp6 = daily_data3["icon"];
			sendToLCD(1, 3, F("day3"), String(getWeatherPicture(tmp6,2)));
			tmp2 = daily_data3["temperatureHigh"];
			sendToLCD(1, 1, F("day3high"), String(round(tmp2)));
			tmp2 = daily_data3["temperatureLow"];
			sendToLCD(1, 1, F("day3low"), String(round(tmp2)));

			//last update time
			updateTime(2);
			sendToLCD(2, 1, F("timeForecast"), timeTmp);
		}
		else {
			updateTime(2);
			tmp = timeTmp;
			tmp += F("(ERROR)");
			sendToLCD(2, 1, F("timeCurrent"), timeTmp);
#ifdef DEBUG
			Serial.println("parseObject() failed");
#endif
#ifdef DEBUG
			Serial.print("Forecast+moon phase+sun times updated");
			currentTime();;
#endif
		}
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

int getMoonPicture(float f1) { // calculate whether moon increases or decreases by comparing today with tomorrow
	int percentage = f1 * 100;
	if (percentage <= 2) {
		return 98; //new moon
	}
	else if (percentage <= 7) {
		return 99;
	}
	else if (percentage <= 12) {
		return 100;
	}
	else if (percentage <= 17) {
		return 101;
	}
	else if (percentage <= 22) {
		return 102;
	}
	else if (percentage <= 27) {
		return 103;
	}
	else if (percentage <= 32) {
		return 104;
	}
	else if (percentage <= 37) {
		return 105;
	}
	else if (percentage <= 42) {
		return 106;
	}
	else if (percentage <= 47) {
		return 107;
	}
	else if (percentage <= 52) {
		return 108; // full moon
	}
	else if (percentage <= 57) {
		return 109;
	}
	else if (percentage <= 62) {
		return 110;
	}
	else if (percentage <= 67) {
		return 111;
	}
	else if (percentage <= 72) {
		return 112;
	}
	else if (percentage <= 77) {
		return 113;
	}
	else if (percentage <= 82) {
		return 114;
	}
	else if (percentage <= 87) {
		return 115;
	}
	else if (percentage <= 92) {
		return 116;
	}
	else if (percentage <= 97) {
		return 117;
	}
}

int getWeatherPicture(String icon, int size) {
	// size = 1, 128x128
	// size = 2, 64x64
	if (size == 1) {
		if (icon == F("clear - day")) {
			return 81;
		}
		else if (icon == F("clear-night")) {
			return 80;
		}
		else if (icon == F("rain")) {
			return 60;
		}
		else if (icon == F("snow")) {
			return 62;
		}
		else if (icon == F("sleet")) {
			return 67;
		}
		else if (icon == F("wind")) {
			return 72;
		}
		else if (icon == F("fog")) {
			return 69;
		}
		else if (icon == F("cloudy")) {
			return 75;
		}
		else if (icon == F("partly-cloudy-day")) {
			return 79;
		}
		else if (icon == F("partly-cloudy-night")) {
			return 78;
		}
		else if (icon == F("hail")) {
			return 84;
		}
		else if (icon == F("thunderstorm")) {
			return 49;
		}
		else if (icon == F("tornado")) {
			return 50;
		}
		else {
			return 97; //unknown - not supported
		}
	}
	else if (size == 2) {
		if (icon == F("clear - day")) {
			return 32;
		}
		else if (icon == F("clear-night")) {
			return 31;
		}
		else if (icon == F("rain")) {
			return 11;
		}
		else if (icon == F("snow")) {
			return 13;
		}
		else if (icon == F("sleet")) {
			return 18;
		}
		else if (icon == F("wind")) {
			return 23;
		}
		else if (icon == F("fog")) {
			return 20;
		}
		else if (icon == F("cloudy")) {
			return 26;
		}
		else if (icon == F("partly-cloudy-day")) {
			return 28;
		}
		else if (icon == F("partly-cloudy-night")) {
			return 27;
		}
		else if (icon == F("hail")) {
			return 17;
		}
		else if (icon == F("thunderstorm")) {
			return 3;
		}
		else if (icon == F("tornado")) {
			return 1;
		}
		else {
			return 48; //unknown - not supported
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

#ifdef DEBUG
	void currentTime() {
		Serial.print(": ");
		Serial.print(hour());
		Serial.print(":");
		Serial.print(minute());
		Serial.print(":");
		Serial.print(second());
		Serial.print(" ");
		Serial.print(day());
		Serial.print("-");
		Serial.print(month());
		Serial.print("-");
		Serial.print(year());
		Serial.println();
	}
#endif
