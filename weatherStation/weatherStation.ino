#include <HTTPClient.h> // for https communication
#include <TimeLib.h> //maybe not needed
#include <Time.h> //maybe not needed
#include <WiFi.h>
//#include <Wire.h>
#include <ArduinoJson.h> // https://github.com/bblanchon/ArduinoJson
#include <WiFiUdp.h> // needed to get time
#include <HardwareSerial.h> // to communicate with nextion display

// nextion display sdcard has to be FAT32+2048 cluster

#define DEBUG

//constants

// Wifi configuration
const char* ssid = "";
const char* password = "";

//WiFiServer server(80);

// Time configuration, e.g. NTP server
unsigned int localPort = 2390;                  // local port to listen for UDP packets
IPAddress timeServerIP;                         // IP address of random server 
const char* ntpServerName = "europe.pool.ntp.org";     // server pool
byte packetBuffer[48];                          // buffer to hold incoming and outgoing packets
const int timeZoneoffsetGMT = 3600;                   // offset from Greenwich Meridan Time
boolean DST = true;                            // daylight saving time
WiFiUDP clockUDP;                               // initialize a UDP instance

const String apiURL = "https://api.darksky.net/forecast/";
const String APIKEY = "";
const String location = "";
const String unit = "si";

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

// 
// settings
//
String result;
int startupDelay = 1000;                      // startup delay
int loopDelay = 3000;                         // main loop delay between sensor updates

int timeServerDelay = 1000;                   // delay for the time server to reply
int timeServerPasses = 4;                     // number of tries to connect to the time server before timing out
int timeServerResyncNumOfLoops = 3000;        // number of loops before refreshing the time. one loop takes approx. 28 seconds
int timeServerResyncNumOfLoopsCounter = 0;
boolean timeServerConnected = false;          // is set to true when the time is read from the server

HardwareSerial nexSerial(2);

long int weatherDayLastUpdate = 0;
const int weatherDayUpdateTime = 900000; //time in ms

long int timeLastUpdate = 0;
const int timeUpdateTime = 60000; //time in ms

String myIP;

void setup() {
	// intialise connection to nextion display
	nexSerial.begin(115200, SERIAL_8N1, 19, 22);
	endNextionCommand();
	//set baud rate for Nextion display
	//nexSerial.print("bauds=115200");
	//endNextionCommand();
	// lower light
	nexSerial.print("dim=50");
	endNextionCommand();
	// reset display
	nexSerial.print("page 0");
	endNextionCommand();
	// start wifi
	connectToWifi();

	// get time from NTP server and update local time
	clockUDP.begin(localPort);
	getTimeFromServer();
	// current time can be seen using now().
	
}

void loop() {
	//while (nexSerial.available() > 0) {
	//	for (int i = 1; i < 16; i++) {
	//		array[i] = nexSerial.read();
	//		Serial.println(array[i]);
	//		delay(20);
	//	}
	//}
	//nexSerial.write(now());
	updateOrDisplayTime(1);
	//getWeather();
	
#ifdef DEBUG
	delay(500);
#endif
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

void updateOrDisplayTime(uint8_t type){
	// 1 updates time on display
	// 2 prints time to serial
	if ((type = 1) && (type = 2)) {
		String timeDate;
		timeDate += doubleDigit(hour());
		timeDate += ':';
		timeDate += doubleDigit(minute());
		if (type = 2) {
			timeDate += ':';
			timeDate += doubleDigit(second());
			timeDate += ' ';
		}
		else {
			timeDate += ' ';
		}
		String tmp = dayStr(weekday());
		timeDate += tmp.substring(0, 3); //show three letter day, e.g. fri.
		timeDate += '-';
		timeDate += day();
		timeDate += '/';
		timeDate += month();
		timeDate += '/';
		timeDate += year();
		if (type = 1) {
			if (millis() >= timeLastUpdate) {
				timeLastUpdate = millis() + timeUpdateTime; // next update time
#ifdef DEBUG
				Serial.print("Time updated: ");
				Serial.print(timeDate);
				Serial.println();
				sendToLCD(1, "time", timeDate);
#endif
			}
		}
		else if (type = 2) {
			Serial.print(timeDate);
			Serial.println();
		}
	}
}

void connectToWifi() {
#ifdef DEBUG
	Serial.println();
	Serial.println();
	Serial.print("Connecting to wifi: ");
#endif	
	WiFi.begin(ssid, password);
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		myIP = WiFi.localIP().toString();
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
		unixTime = (highWord << 16 | lowWord) - 2208988800 + timeZoneoffsetGMT;

		// day light saving time
		if (DST) {
			unixTime = unixTime + 3600;
		}
		setTime(unixTime);
#ifdef DEBUG
		Serial.print("Time received: ");
		updateOrDisplayTime(2);
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

void getWeather(){
	if (millis() >= weatherDayLastUpdate ) {
		getWeatherData();
		weatherDayLastUpdate = millis() + weatherDayUpdateTime; // next update time
#ifdef DEBUG
		Serial.print("Daily weather updated: ");
		updateOrDisplayTime(2);
		Serial.println();
#endif
	}
}

void getWeatherData(){
	String tmp;
	if ((WiFi.status() == WL_CONNECTED)) {
		HTTPClient http;
		String url = apiURL + APIKEY + "/" + location + "?" + "unit=" + unit + "&exclude=flags,alerts,minutely,daily,hourly"; // current
#ifdef DEBUG
		Serial.println(url);
#endif

		http.begin(url, ca_cert); //Specify the URL and certificate
		int httpCode = http.GET();                                                  //Make the request
		if (httpCode > 0) { //Check for the returning code
			result = http.getString();
#ifdef DEBUG
			Serial.println(httpCode);
			Serial.println(result);
#endif
		}
		else {
			Serial.println(httpCode);
			Serial.println("Error on HTTP request");
		}

		http.end(); //Free the resources

		//result.replace('[', ' ');
		//result.replace(']', ' ');

		char jsonArray[result.length() + 1];
		result.toCharArray(jsonArray, sizeof(jsonArray));
		jsonArray[result.length() + 1] = '\0';

		StaticJsonBuffer<1000> json_buf;
		JsonObject &root = json_buf.parseObject(jsonArray);
		if (!root.success()) {
			Serial.println("parseObject() failed");
		}

		String tmp0 = root["currently"]["icon"]; //weather icon
		int tmp1 = root["currently"]["temperature"]; //temperature
		int tmp2 = root["currently"]["humidity"]; //humidity
		int tmp3 = root["currently"]["pressure"]; //pressure
		int tmp4 = root["currently"]["windSpeed"]; //windSpeed
		int tmp5 = root["currently"]["uvIndex"]; //uvIndex
		int tmp6 = root["currently"]["windBearing"]; //windBearing

		sendToLCD(1, "pressure", String(setWeatherPicture(tmp0)));
		sendToLCD(1, "pressure", String(round(tmp1)));
		sendToLCD(1, "uvIndex", String(round(tmp5)));

		//properly display wind and direction
		tmp = String(round(tmp4));
		tmp += "m/s - ";
		tmp += getShortWindDirection(tmp6);
		sendToLCD(1, "windADir", tmp);

		//properly display humidity
		tmp = String(round(tmp2));
		tmp += " %";
		sendToLCD(1, "humidity", tmp);

		//properly display pressure
		tmp = String(round(tmp3));
		tmp += " hPa";
		sendToLCD(1, "pressure", tmp);
	}
}

String getShortWindDirection(int degrees) {
	int sector = ((degrees + 11) / 22.5 - 1);
	switch (sector) {
	case 0: return "N";
	case 1: return "NNE";
	case 2: return "NE";
	case 3: return "ENE";
	case 4: return "E";
	case 5: return "ESE";
	case 6: return "SE";
	case 7: return "SSE";
	case 8: return "S";
	case 9: return "SSW";
	case 10: return "SW";
	case 11: return "WSW";
	case 12: return "W";
	case 13: return "WNW";
	case 14: return "NW";
	case 15: return "NNW";
	}
}

int setWeatherPicture(String icon) {
	if (icon == "clear - day") {
		return 81;
	}
	else if (icon == "clear-night") {
		return 80;
	}
	else if (icon == "rain") {
		return 60;
	}
	else if (icon == "snow") {
		return 62;
	}
	else if (icon == "sleet") {
		return 67;
	}
	else if (icon == "wind") {
		return 72;
	}
	else if (icon == "fog") {
		return 69;
	}
	else if (icon == "cloudy") {
		return 75;
	}
	else if (icon == "partly-cloudy-day") {
		return 79;
	}
	else if (icon == "partly-cloudy-night") {
		return 78;
	}
	else if (icon == "hail") {
		return 84;
	}
	else if (icon == "thunderstorm") {
		return 49;
	}
	else if (icon == "tornado") {
		return 50;
	}
	else {
		return 97; //unknown - not supported
	}
}

void delayCheckTouch(int delayTime) {
	unsigned long startMillis = millis();

	while (millis() - startMillis < delayTime) {
		delay(1000);
	}
}

void endNextionCommand() {
	nexSerial.write(0xff);
	nexSerial.write(0xff);
	nexSerial.write(0xff);
}

void sendToLCD(uint8_t type, String index, String cmd) {
#ifdef DEBUG
	Serial.print("toDisplay:");
	Serial.print(type);
	Serial.print(":");
	Serial.print(index);
	Serial.print(":");
	Serial.print(cmd);
#endif // DEBUG
	if (type == 1) {
		nexSerial.print(index);
		nexSerial.print(".txt=");
		nexSerial.print("\"");
		nexSerial.print(cmd);
		nexSerial.print("\"");
	}
	else if (type == 2) {
		nexSerial.print(index);
		nexSerial.print(".val=");
		nexSerial.print(cmd);
	}
	else if (type == 3) {
		nexSerial.print(index);
		nexSerial.print(".pic=");
		nexSerial.print(cmd);
	}
	else if (type == 4) {
		nexSerial.print("page ");
		nexSerial.print(cmd);
	}

	endNextionCommand();
}

#ifdef DEBUG
	void currentTime() {
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
	}
#endif