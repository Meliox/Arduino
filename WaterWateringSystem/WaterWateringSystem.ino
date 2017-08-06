#include <SPI.h>
#include <U8g2lib.h>

/* Constructor */;
U8G2_SSD1306_128X32_UNIVISION_1_HW_I2C display(U8G2_R0, SCL, SDA, U8X8_PIN_NONE);

#define CH_PD 7 // digital pin for resetting esp8266

// Wifi configuration
#define SSID ""
#define PASSWORD ""
#define HOSTNAME ""
#define WEBSERVERPORT "80"
//change baud rate AT+UART_DEF=115200,8,1,0,0

// enable and disbale pumps and connected sensors
// pump is [i][0], while any associated sensor is the next [0][i], e.g. 4 sensors possible per pump
int sys[5][5] = {
  {2, 0, -1, -1, -1} ,
  { -1, -1, -1, -1, -1} ,
  { -1, -1, -1, -1, -1} ,
  { -1, -1, -1, -1, -1} ,
  { -1, -1, -1, -1, -1} ,
};
// limit
#define PUMPTIME "2000"
#define SENSORLIMIT "350"
#define CHECKINTERVAL "900000" // how often to check the sensors for each pump

// CODE BELOW
///////////////////////////////////////////////////////////////////////////////////////
int i;
int k;
int j;
int webparsedresponse[3];
const byte numChars = 32;
char receivedChars[numChars];   // an array to store the received data
char serialmessage[32];
boolean newData = false;
int arrInfo[5];
long int lastupdate[5] = { -1, -1, -1, -1, -1};
long int lastdisplayupdate;
String webpage;
String displaytext;
boolean lcdon = false;
short int lcdloop = 0;

////////////////////////////////////////////////////////////////////////////////////////
void setup(void) {
  // initialise all connected parts
  // turn off all pumpts
  pumpoff();
  // start display
  display.begin();
  writetodisplay(F("Initialising wifi"));
  Serial.begin(115200);
  reset8266();
  InitWifiModule(); // Iniciate module as WebServer
  writetodisplay(F("Done"));
  delay(1000);
  display.clear();
}

/* draw something on the display with the `firstPage()`/`nextPage()` loop*/
void loop(void) {
  check8266(); // check webserver and respond
  updatedisplay(); // update display information
  checksensors(); // check sensors and pump water is deemed necessary
}

void updatedisplay() {
  // limit updating of the display
  if (lcdon) {
    if (millis() >= (lastdisplayupdate + 10000)) {
      // only display information for enabled pumps
      while (sys[lcdloop][0] < 0) {
        ++lcdloop;
        // reset loop
        if (lcdloop = 4 ) {
          lcdloop = 0;
        }
      }
      // Assembly display message
      displaytext = F("P: ");
      displaytext += lcdloop;
      displaytext += F(" ");
      getsenorinfo(lcdloop);
      for (i = 0 ; i < 4 ; ++i ) {
        if (arrInfo[i] >= 0) {
          displaytext += F("S: ");
          displaytext += lcdloop;
          displaytext += F(": ");
          displaytext += arrInfo[i];
          displaytext += F(",");
        }
      }
      // Print to display
      writetodisplay(displaytext);
      // reset loop
      if (lcdloop >= 4 ) {
        lcdloop = 0;
      } else {
        ++lcdloop;
      }
      // clear display text and pause display update
      displaytext = "";
      lastdisplayupdate = millis();
    }
  }
}

void pumpoff() {
  // turn off all pumps
  for (i = 0 ; i < 5 ; ++i ) {
    if (sys[i][0] >= 0) {
      pinMode(sys[i][0], OUTPUT);
      digitalWrite(sys[i][0], LOW);
    }
  }
}

void check8266() {
  if (Serial.available()) {
    // check if 8266 is sending data
    if (Serial.find("+IPD,")) {
      // look for an reponse
      Serial.println("reading");
      read8266();
      parseresponse8266();
      reply8266();
    }
  }
}

void checksensors() {
  // loop through all anabled pumps and sensors
  for (i = 0 ; i < 5 ; ++i ) {
    // only enabled pumps
    if (sys[i][0] >= 0 && millis() > lastupdate[i] + CHECKINTERVAL )
    {
      // loop through all sensors
      for (j = 1 ; i < 4 ; ++i ) {
        //only enabled sensors
        if (sys[i][j] >= 0)
        {
          // if sensor is above limit, we need to pump water
          if (analogRead(sys[i][j]) > SENSORLIMIT ) {
            // pump water
            pumpwater(sys[i]);
            // exit loop so water can settle for pump and sensors, go to next pump
            break;
          }
          // update last check interval
          lastupdate[i] = millis();
        }
      }
    }
  }
}

void pumpwater(int i) {
  // turn on pump for a given time
  digitalWrite(i, HIGH);
  delay(PUMPTIME);
  digitalWrite(i, LOW);
}

void parseresponse8266() {
  // possible reponses from esp8266
  // receivedChars="0,424:GET /?pin=2&on=65 HTTP/1.";
  // receivedChars="0,424:GET /?pin=sd HTTP/1.1";
  // receivedChars="0,424:GET HTTP/1.1 xxxxxxxxxxxxxxxx";
  strcpy(serialmessage, receivedChars);
  // get connection id
  webparsedresponse[0] = parser(serialmessage, 0);
  // parse pin if present
  strcpy(serialmessage, receivedChars);
  if (strstr(serialmessage, "pin")) { // todo: broken
    webparsedresponse[1] = parser(serialmessage, 3);
  } else {
    webparsedresponse[1] = -1;
  }
  strcpy(serialmessage, receivedChars);
  // parse enable if present
  if (strstr(serialmessage, "on")) {
    webparsedresponse[2] = 1;
  } else {
    webparsedresponse[2] = -1;
  }
  // parse lcd if present
  strcpy(serialmessage, receivedChars);
  if (strstr(serialmessage, "lcd")) {
    if (lcdon) {
      lcdon = false;
    } else {
      lcdon = true;
    }
  }
  //reset data
  serialmessage[32];
  receivedChars[32];
  newData = false;
}

int parser(char arr[], int i) {
  // parse int from esp8266 response
  char * strtokIndx;
  strtokIndx = strtok(arr, ", =&");
  int arr2[8];
  j = 0;
  while ((strtokIndx != NULL)) {
    arr2[j] = atoi(strtokIndx);
    strtokIndx = strtok(NULL, ", =&");
    ++j;
  }
  return arr2[i];
}

void reply8266() {
  // parse reply from esp8266 and return a response based on reply
  webpage = F("<h1>Water sys</h1><h2>");
  if (webparsedresponse[1] >= 0 && webparsedresponse[2] == 1 ) {
    // selected pump must be turned on
    pumpwater(webparsedresponse[1]);
    webpage += F("Pump ");
    webpage += webparsedresponse[1];
    webpage += F(" is turned on.");
    webpage += F("</h2>");
  }
  else if (webparsedresponse[1] >= 0 ) {
    // return info on sensors for selected pump
    webpage += F("Pump: ");
    webpage += webparsedresponse[1];
    getsenorinfo(webparsedresponse[1]);
    webpage += F("<br>");
    for (i = 0 ; i < 4 ; ++i ) {
      if (arrInfo[i] >= 0) {
        webpage += F("Sensor: ");
        webpage += i;
        webpage += F(",");
        webpage += arrInfo[i];
        webpage += F("<br>");
      }
    }
    webpage += F("</h2>");
  }
  else {
    // main page
    webpage += F("<form action=""/"">");
    webpage += F("Pump selc:<br>");
    webpage += F("<input type=""text"" name=""pin"" value=""><br>");
    webpage += F("Water:<input type="" checkbox"" name=""on"" value=""1""><br>");
    webpage += F("<input type=""submit"" value=""Submit"">");
    webpage += F("</form>");
    webpage += F("Display:");
    if (lcdon) {
      webpage += F("on");
    } else {
      webpage += F("off");
    }
    webpage += F("<br>");
    webpage += F("<form action=""/"">");
    webpage += F("<input type=""hidden"" name=""lcd"" value=""1"">");
    webpage += F("<input type=""submit"" value=""ON/OFF""/>");
    webpage += F("</form>");
    webpage += F("</h2>");
  }
  String cipSend = F("AT+CIPSEND=");
  cipSend += webparsedresponse[0];
  cipSend += ",";
  cipSend += webpage.length();
  sendData(cipSend, 1000);
  sendData(webpage, 1000);
  delay(200);
  String closeCommand = F("AT+CIPCLOSE=");
  closeCommand += webparsedresponse[0]; // append connection id
  sendData(closeCommand, 1000);
  //reset webparsedresponse
  webparsedresponse[3];
  //reset info array
  arrInfo[5];
  //clear webpage
  webpage = "";
}
void getsenorinfo(int i) {
  // get info from all enabled sensors for pump i
  for (j = 1 ; j < 5 ; ++j ) {
    if (sys[i][j] >= 0 ) {
      arrInfo[j - 1] = analogRead(sys[i][j]);
    } else {
      arrInfo[j - 1] = -1;
    }
  }
}

void read8266() {
  // read reply from esp8266 using serial communication
  static byte ndx = 0;
  char endMarker = '\n';
  char rc;
  while (Serial.available() > 0 && newData == false) {  
    rc = Serial.read();
    Serial.println(rc);
    if (rc != endMarker) {
      receivedChars[ndx] = rc;
      ndx++;
      if (ndx >= numChars) {
        ndx = numChars - 1;
      }
    }
    else {
      receivedChars[ndx] = '\0'; // terminate the string
      ndx = 0;
      newData = true;
    }
  }
  Serial.println(newData);
  Serial.print("h: ");
  for (i = 0; i < 32 ; ++i) {
    Serial.print(receivedChars[i]);
  }
  Serial.println("");
  // empty buffer
  emptyserialbuffer();
}

void reset8266 () {
  // Pin CH_PD needs a reset before start communication
  pinMode(CH_PD, OUTPUT);
  digitalWrite(CH_PD, LOW);
  delay(300);
  digitalWrite(CH_PD, HIGH);
}

void writetodisplay(String t) {
  // display only handles 50 char at a time in two lines
  if (t.length() < 50 ) {
    display.firstPage();
    do {
      display.setFont(u8g2_font_ncenB08_tr);
      if ( t.length() > 25 ) {
        display.setCursor(0, 10);
        display.print(t.substring(1, 25));
        display.setCursor(0, 25);
        display.print(t.substring(26, 50));
      } else {
        display.setCursor(0, 10);
        display.print(t);
      }
    } while ( display.nextPage() ); //todo: fix to write two lines
  } else {
    display.firstPage();
    do {
      display.setFont(u8g2_font_ncenB08_tr);
      display.setCursor(0, 10);
      display.print(F("error"));
    } while ( display.nextPage() );
  }
}

void InitWifiModule() {
  // initialise esp8266
  sendData(F("AT+RST"), 2000); // reset
  delay(1000);
  sendData(F("AT+CWMODE=1"), 1000); // enable connection to wireless
  delay(1000);
  sendData(F("AT+CWHOSTNAME=\""HOSTNAME"\""), 1000); // set hostname of esp8266
  delay(1000);
  sendData(F("AT+CWJAP_CUR=\""SSID"\",\""PASSWORD"\""), 2000); // login to wifi network
  delay(5000);
  sendData(F("AT+CIPMUX=1"), 1000); // Multiple connections (5)
  delay(1000);
  sendData(F("AT+CIPSERVER=1,"WEBSERVERPORT""), 1000); // start webserver at port
}

void emptyserialbuffer() {
  // empty serial buffer
  while (Serial.available()) {
    Serial.read();
  }
}
/*************************************************/
// Send AT commands to esp8266
void sendData(String command, const int timeout) {
  Serial.println(command);
  // add delay
  delay(100);
  long int time = millis();
  while ( (time + timeout) > millis()) {
    emptyserialbuffer();
  }
}
