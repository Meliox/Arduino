#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <AsyncTCP.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <Timezone.h> // https://github.com/JChristensen/Timezone

#define DEBUG

const String W_VERSION = F("1.0");
const String NAME = F("Water watering system");

// Wifi configuration
const char* SSID = "";
const char* PASSWORD = "";
const char* WIFI_HOSTNAME = "";

// Time configuration
const char* ntpServerName = "europe.pool.ntp.org";     //server pool
const int timeZoneOffset = 1;                   //offset from Greenwich Meridan Time
TimeChangeRule myDST = { "GMT", Fourth, Sun, Mar, 2, timeZoneOffset * 60 };  //UTC+1
TimeChangeRule mySTD = { "DST", Fourth, Sun, Nov, 3, timeZoneOffset * 60 + 60 };   //UTC +2

AsyncWebServer server(80);
WiFiUDP clockUDP;                               // initialize a UDP instance
unsigned int LOCAL_PORT = 2390;                 // local port to listen for UDP packets
IPAddress timeServerIP;                         // IP address of random server 
byte packetBuffer[48];                          // buffer to hold incoming and outgoing packets
int TIME_SERVER_DELAY = 1000;                   // delay for the time server to reply
int TIME_SERVER_PASSES = 4;                     // number of tries to connect to the time server before timing out
boolean TIME_SERVER_CONNECTED = false;          // is set to true when the time is read from the server
Timezone myTZ(myDST, mySTD);


int sensorUpdateTime = 10; //in mins
int minPumpInterval = 10; //in mins

// enable and disbale pumps and connected sensors
// pump is [i][0], while any associated sensor is the next [0][i], e.g. 4 sensors possible per pump
//disabled(0=false, 1=true), pump pin, pumptime (seconds), last pump time, sensor pin, last sensor value, last sensor read, pump water below
int numberOfPumps = 2;
long int sys[2][8] = {
	{0, 32, 2, 0, 34, 3600, 0, 2500},
	{0, 33, 2, 0, 35, 1200, 0, 2500},
};

//code
long int timeNewUpdate = 0;

////////////////////////////////////////////////////////////////////////////////////////
void setup() {
	// turn off all pumps
	initPump();

	// intialise wifi
	connectToWifi();

	// get time from NTP server and update local time
	initTime();
	
	//return json webreply
	webReplyJson(); 

	//return html webpage webreply
	server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
		int params = request->params();
		if (request->hasParam("pump")) { //evaluate return parameters
			AsyncWebParameter* p = request->getParam("pump");
			pumpWater(atoi(p->value().c_str()));
		}
		char tmp[1000];
		webreplyHTML().toCharArray(tmp,1000);
		request->send_P(200, "text/html", tmp); //return html page
	});

	server.begin();
}

void loop() {
	//read soil sensor
	readSensor();
	//pump water when needed
	//evaluateDryness();
	delay(5000);
}

void evaluateDryness() {
	for (int i = 0; i < numberOfPumps; ++i) {
		if (sys[i][5] < sys[i][7]) {
			pumpWater(i);
		}
	}
}

void readSensor() {
	for (int i = 0; i < numberOfPumps; ++i) {
		if (now() >= sys[i][6] + sensorUpdateTime * 60 * 1000 ) {
			if (sys[i][0] == 0) {
				sys[i][5] = analogRead(sys[i][4]); //read sensor value
				sys[i][6] = now(); //update last read time
				delay(50);
			}
		}
	}
}

void initPump() {
	for (int i = 0; i < numberOfPumps; ++i) {
		if (sys[i][0] == 0) {
			pinMode(sys[i][1], OUTPUT); //enable pin for pump
			digitalWrite(sys[i][1], LOW); //disable pin
		}
	}
}

String webreplyHTML() {
	String tmp;
	tmp = F("<html>");
	tmp += F("<style>table, th, td { border: 1px solid black; border-collapse: collapse; }</style>");
	tmp += F("<body>");
	tmp += F("<h1>Automatic water pumping system</h1>");
	tmp += F("<table style=\"width:100 %\">");
	tmp += F("<tr>");
	tmp += F("<th>Pump no.</th>");
	tmp += F("<th>Pumping time (s)</th>");
	tmp += F("<th>Last pump time</th>");
	tmp += F("<th>Moisture lvl.</th>");
	tmp += F("<th>Last reading</th>");
	tmp += F("<th>Pump</th>");
	tmp += F("</tr>");
	for (int i = 0; i < numberOfPumps; ++i) {
		tmp += F("<tr>");
		tmp += F("<td>");
		tmp += i;
		tmp += F("</td>");
		tmp += F("<td>");
		tmp += sys[i][2];
		tmp += F("</td>");
		tmp += F("<td>");
		tmp += displayTime(sys[i][3]);
		tmp += F("</td>");
		tmp += F("<td>");
		tmp += getDrynessScale(sys[i][5]);
		tmp += F("</td>");
		tmp += F("<td>");
		tmp += displayTime(sys[i][6]);
		tmp += F("</td>");
		tmp += F("<td>");
		tmp += "<form action = \"/\"><button type=\"submit\" name=\"pump\" value=\"";
		tmp += i;
		tmp += "\">ON</button></form>";
		tmp += F("</td>");
		tmp += F("</tr>");
	}
	tmp += F("</table>");
	tmp += F("</body>");
	tmp += F("</html>");
	return tmp;
}

String getDrynessScale(int t) {
	if (t >= 3500) {
		return "very dry";
	}
	else if (t < 3500 && t > 2500) {
		return "dry";
	}
	else if (t < 2500 && t > 1500) {
		return "moist";
	}
	else if (t < 1500) {
		return "wet";
	}
}

void webReplyJson() {
	server.on("/json", HTTP_GET, [](AsyncWebServerRequest *request) {
		AsyncResponseStream *reply = request->beginResponseStream("text/json");
		StaticJsonBuffer<1000> JSONbuffer;
		JsonObject& root = JSONbuffer.createObject();
		JsonObject& response = root.createNestedObject("response");
		response["name"] = NAME;
		response["version"] = W_VERSION;
		response["time"] = now();
		JsonObject& sensor = root.createNestedObject("sensor");
		for (int i = 0; i < numberOfPumps; ++i) {
			JsonObject& sensorNumber = sensor.createNestedObject(String(i));
			sensorNumber[F("Pumping time")] = sys[i][2];
			sensorNumber[F("Last pump time")] = sys[i][3];
			sensorNumber[F("Moisture lvl.")] = getDrynessScale(sys[i][5]);
			sensorNumber[F("Last reading time")] = sys[i][6];
		}
		root.printTo(*reply);
		request->send(reply);
	});
}

String displayTime(time_t t) {
	String timeTmp;
	timeTmp += doubleDigit(hour(t));
	timeTmp += ':';
	timeTmp += doubleDigit(minute(t));
	timeTmp += ':';
	timeTmp += doubleDigit(second(t));
	timeTmp += " - ";
	timeTmp += doubleDigit(day(t));
	timeTmp += '/';
	timeTmp += doubleDigit(month(t));
	timeTmp += '/';
	timeTmp += year(t);
	return timeTmp;
}

void pumpWater(int p) {
	digitalWrite(p, HIGH);
	delay(sys[p][2] * 1000); //pump time
	digitalWrite(p, LOW);
	sys[p][3] = now(); //set last pump time
	Serial.print("pumping: ");
	Serial.print(p);
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
	Serial.println();
	Serial.print("IPAddress: ");
	Serial.println(WiFi.localIP());
#endif
}

void initTime() {
	clockUDP.begin(LOCAL_PORT);
	getTimeFromServer();
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
		Serial.print(displayTime(now()));
		Serial.println("");
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