
#include <TimeLib.h>
#include <Time.h>
#include <WiFi.h>
//#include <Wire.h>
#include <ArduinoJson.h> // https://github.com/bblanchon/ArduinoJson
#include <WiFiUdp.h> // needed to get time

#define DEBUG

//constants

// Wifi configuration
char* ssid = "";
char* password = "";

WiFiServer server(80);

// Time configuration, e.g. NTP server
unsigned int localPort = 2390;                  // local port to listen for UDP packets
IPAddress timeServerIP;                         // IP address of random server 
const char* ntpServerName = "europe.pool.ntp.org";     // server pool
byte packetBuffer[48];                          // buffer to hold incoming and outgoing packets
int timeZoneoffsetGMT = 3600;                   // offset from Greenwich Meridan Time
boolean DST = true;                            // daylight saving time
WiFiUDP clockUDP;                               // initialize a UDP instance


char * servername = "api.openweathermap.org";          // remote server with weather info
String APIKEY = "";   // personal api key for retrieving the weather data

const int httpPort = 80;
String result;
int cityIDLoop = 0;

// a list of cities you want to display the forecast for
// get the ID at https://openweathermap.org/
// type the city, click search and click on the town
// then check the link, like this: https://openweathermap.org/city/5128581
// 5128581 is the ID for New York

String cityID = "2618425";  // Copenhagen

// 
// settings
//

int startupDelay = 1000;                      // startup delay
int loopDelay = 3000;                         // main loop delay between sensor updates

int timeServerDelay = 1000;                   // delay for the time server to reply
int timeServerPasses = 4;                     // number of tries to connect to the time server before timing out
int timeServerResyncNumOfLoops = 3000;        // number of loops before refreshing the time. one loop takes approx. 28 seconds
int timeServerResyncNumOfLoopsCounter = 0;
boolean timeServerConnected = false;          // is set to true when the time is read from the server

void setup() {
	// start wifi
#ifdef DEBUG
	Serial.println();
	Serial.println();
	Serial.print("Connecting to wifi: ");
#endif	
	WiFi.begin(ssid, password);
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
#endif // DEBUG

	// get time from NTP server and update local time
	clockUDP.begin(localPort);
	getTimeFromServer();

}

void loop() {
	Serial.println(now());
	delay(2500);
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
