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
// pump is [i][0], while any associated sensor is the next [0][i]
int sys[5][5] = {
  {2, 0, -1, -1, -1} ,
  {3, -1, -1, -1, -1} ,
  { -1, -1, -1, -1, -1} ,
  { -1, -1, -1, -1, -1} ,
  { -1, -1, -1, -1, -1} ,
};
// limit
#define PUMPTIME "2000"
short int limit = 400;
short int checkinterval = 900000; // how often to check the sensors for each pump

// CODE BELOW
///////////////////////////////////////////////////////////////////////////////////////
int i;
int k;
int j;
int webparsedresponse[3];
const byte numChars = 32;
char receivedChars[numChars];   // an array to store the received data
boolean newData = false;
int arrInfo[5];
long int lastupdate[5] = { -1, -1, -1, -1, -1};
long int lastdisplayupdate;
String webpage;

void setup(void) {
  // initialise all connected parts
  // turn off all pumpts
  pumpoff();
  // start display
  display.begin();
  writetodisplay(F("Initialising wifi"));
  reset8266();
  Serial.begin(115200);
  InitWifiModule(); // Iniciate module as WebServer
  writetodisplay(F("Done"));
  delay(1000);
  display.clear();
}

/* draw something on the display with the `firstPage()`/`nextPage()` loop*/
void loop(void) {
  // check webserver and respond
  check8266();
  // update display information
  writetodisplay(String(k));
  checksensors();
  k++;
}

void pumpoff() {
  // PUMP SECTION, turn off all pumps
  for (i = 0 ; i < 5 ; ++i )
  {
    if (sys[i][0] >= 0)
    {
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
    if (sys[i][0] >= 0 && millis() > lastupdate[i] + checkinterval )
    {
      // loop through all sensors
      for (j = 0 ; i < 5 ; ++i ) {
        //only enabled sensors
        if (sys[i][j] >= 0)
        {
          // if sensor is above limit, we need to pump water
          if (analogRead('a' + sys[i][j]) > limit ) {
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
  // char lol[] = "0,424:GET /?pin=2&on=65 HTTP/1.";
  // receivedChars="0,424:GET /?pin=sd HTTP/1.1";
  // receivedChars="0,424:GET HTTP/1.1 xxxxxxxxxxxxxxxx";
  // get connection id
  webparsedresponse[0] = parser(receivedChars, 0);
  // parse pin if present
  if (strcmp(receivedChars, "pin") == 0) { // todo: broken
    webparsedresponse[1] = parser(receivedChars, 3);
    // parse enable if present
    if (strcmp(receivedChars, "on") == 0) { // todo: broken
      webparsedresponse[2] = parser(receivedChars, 5);
    } else {
      webparsedresponse[2] = -1;
    }

  } else {
    webparsedresponse[1] = -1;
  }
  //reset data
  for (i = 0 ; i < 3 ; ++i ) {
    Serial.println(webparsedresponse[i]);
  }
  newData = false;
}

int parser(char arr[], int i) {
  // parse int from esp8266 response
  //strcasestr
  char * strtokIndx;
  strtokIndx = strtok(arr, ", =&");
  int arr2[8];
  j = 0;
  while ((strtokIndx != NULL))
  {
    arr2[j] = atoi(strtokIndx);
    strtokIndx = strtok(NULL, ", =&");
    ++j;
  }
  return arr2[i];
}

void reply8266() {
  // parse reply from esp8266 and return a response based on reply
  if (webparsedresponse[1] >= 0 && webparsedresponse[2] == 1 ) {
    // selected pump must be turned on
    pumpwater(webparsedresponse[0]);
    webpage = F("<h1>Automatic watering system</h1><h2>");
    webpage += F("Pump : ");
    webpage += webparsedresponse[0];
    webpage += F("is enabled.");
    webpage += F("</h2>");
  }
  else if (webparsedresponse[1] >= 0 ) {
    // return info on sensors for selected pump
    webpage = F("<h2>Pump: ");
    webpage += webparsedresponse[0];
    info(webparsedresponse[1]);
    for (i = 0 ; i < 5 ; ++i ) {
      webpage += i;
      webpage += F(": ");
      webpage += arrInfo[i];
      webpage += F(", ");
    }
    webpage += F("</h2>");
  }
  else {
    // return main page
    webpage = F("<h1>Automatic watering system</h1><h2>");
    webpage += F("<form action=""/"">");
    webpage += F("Pump selection:<br>");
    webpage += F("<input type=""text"" name=""pin"" value=""><br>");
    webpage += F("<input type=""submit"" value=""Submit"">");
    webpage += F("</form>");
    webpage += F("Enable display:<br>");
    webpage += F("<input type=""text"" name=""lcd"" value=""><br>");
    webpage += F("<input type=""submit"" value=""Submit"">");
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
void info(int i) {
  // get info from all enabled sensors for pump i
  arrInfo[1] = i;
  for (j = 0 ; i < 5 ; ++i ) {
    if (sys[i][j] >= 0 ) {
      arrInfo[j] = analogRead('a' + sys[i][j]);
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
  display.firstPage();
  do {
    display.setFont(u8g2_font_ncenB08_tr);
    display.setCursor(0, 15);
    display.print(t);
  } while ( display.nextPage() );
}

void InitWifiModule() {
  // initialise esp8266
  sendData(F("AT+RST"), 2000); // reset
  delay(1000);
  sendData(F("AT+CWMODE=1"), 1000);
  delay(1000);
  sendData(F("AT+CWHOSTNAME=\""HOSTNAME"\""), 1000); // fisken\"\r\n", 1000);
  delay(1000);
  sendData(F("AT+CWJAP_CUR=\""SSID"\",\""PASSWORD"\""), 2000);
  delay(5000);
  // sendData("AT+CIFSR", 1000); // Show IP Adress
  sendData(F("AT+CIPMUX=1"), 1000); // Multiple conexions
  delay(1000);
  sendData(F("AT+CIPSERVER=1,"WEBSERVERPORT""), 1000); // start comm port 80
}

void emptyserialbuffer() {
  // empty serial buffer
  while (Serial.available())
  {
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
  while ( (time + timeout) > millis())
  {
    emptyserialbuffer();
  }
}
