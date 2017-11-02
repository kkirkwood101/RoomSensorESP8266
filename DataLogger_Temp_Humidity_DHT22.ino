/*
    This sketch sends data via HTTP GET requests to www.HL14.com//DataLogger/DataLog_01.php service.
    it encodes the data values such as Temp, HUidity etc into the URL

  Revison Log
  20160901 K.Kirkwood
  - Original Version
  20161001 K.Kirkwood
  - Added WifiMulti Functionality to scan a list of known Access points and connect to the best signal
  - added signal strength to the output data
  20161120 K.Kirkwood
  - Added logic to scan PIR motion sensor (Pin 2 on ESP8266 = Pin 4 on Arduino Uno)
  - Added logic to wait upto 60000 milliseconds for the PIR to Stabilize
  20161201 KKIRKWOOD
  - Added DHT.h Lirary and added DHT22 type
  20170214 K Kirkwood
  - changed password for TGM access point
  20171018 K Kirkwood
  - Added logic for reed switch to sense door Open / Close
  20171019 k Kirkwood
  - Added call pinMode(DOORMONITOR, INPUT_PULLUP) to make PIR more reliable
*/

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <EEPROM.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
//#include <dht11.h>
#include <DHT.h>
#define TZ_ADJUST -8
#define DUTYCYCLELIGHT 20000
#define DUTYCYCLEMOTION 500
#define DUTYCYCLETEMP 40000
#define DUTYCYCLEOUTPUT 1800000
#define MINSTATECHANGE 20000
#define LEDPIN 13 // our output signal pin for a LED is pin 13 if needed
#define MOTIONPIN 4 // Input from motion sensor = Pin 2 ESP8288-12E
#define DOORMONITOR 12 // Input from Door Sensor (Reed Switch) Pin 6 on ESP8266-12
#define DHT11PIN 2 // DTH11 Pin for Humidity and Temp = Pin 4 ESP8266-12E
#define DHTYPE DHT22 // DHT 22 (AM2302)

String ssid;
boolean wificonnected;
unsigned int localPort = 2390;      // local port to listen for UDP packets

/* Don't hardwire the IP address or we won't get the benefits of the pool.
    Lookup the IP address for the host name instead */
//IPAddress timeServer(129, 6, 15, 28); // time.nist.gov NTP server

IPAddress timeServerIP; // time.nist.gov NTP server address
const char* ntpServerName = "time.nist.gov";

const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message

byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udp;

//Create object to access the Wifi mult access point scan and select
ESP8266WiFiMulti wifiMulti;

// create an object to access the Dth11 chip
DHT dht(DHT11PIN,DHTYPE,30);

int lightPin = 0;  //define a pin for Photo resistor
unsigned long LastTimeLight = millis() - DUTYCYCLELIGHT; // Used to figure out last time we sampled light
unsigned long LastTimeTemp = millis() - DUTYCYCLETEMP; // Used to figure out last time we sampled Temp
unsigned long LastTimeMotion = millis() - DUTYCYCLEMOTION; // Used to figure out last time we sampled motion
unsigned long LastTimeOutput = millis(); // Used to figure out last time we output our readings
unsigned long LastTimeStateChange = millis() - MINSTATECHANGE; // last time we recorded a state change
unsigned long StartTime = millis(); // Record start time since we have to allow the PIR 1 minute to Settle
boolean OutputNow = true;
String LightStatus = "UNK";
String MotionStatus = "UNK";
String LastMotion = "";
String SystemStatus = "Reboot";
char TimeNow[18];
char StationID[7];
float CurrentTemp = 0;
float Humidity = 0;
float DewPoint = 0;
String DoorStatus ="Unknown";
String LastDoorStatus="";
int cb;


String LocalIP;

const char* host = "WWW.HL14.COM";

unsigned long LocalUnixTime;

void setup() {
  // Sewtup for the reed switch for the door
   // Since the other end of the reed switch is connected to ground, we need
  // to pull-up the reed switch pin internally.
  pinMode(DOORMONITOR, INPUT_PULLUP);
 dht.begin();
  // Create a list of access points we know about we will scan for them later
  wifiMulti.addAP("mobilegoat", "t^^^^^^");
  wifiMulti.addAP("TELUS0504", "*******");
  wifiMulti.addAP("tgm", "********");

  Serial.println("Starting UDP");
  udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(udp.localPort());

  Serial.begin(9600);
  EEPROM.begin(512);
  delay(10);

  // We start by connecting to a WiFi network

  Serial.println();

  // Use the WifiMulti Library to connect to the best signal in out list

  do {
    if (wifiMulti.run() != WL_CONNECTED) {
      Serial.println("WiFi not connected!");
    } else {
      Serial.println("");
      Serial.println("WiFi connected");
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());
      Serial.println("Access Point");
      Serial.println(WiFi.SSID());
      wificonnected = 1;

    }
    delay(800);
  } while (!wificonnected);

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  LocalIP = WiFi.localIP().toString();
  Serial.println(LocalIP);

  // Get the Station ID burned into EEPROM
  for (int i = 0; i < 6; i++) {
    StationID[i] = EEPROM.read(i);
  }

  // Get an NTP time Stamp
  //get a random server from the pool
  WiFi.hostByName(ntpServerName, timeServerIP);
  do {
    sendNTPpacket(timeServerIP); // send an NTP packet to a time server
    // wait to see if a reply is available
    delay(1500);

    cb = udp.parsePacket();
    if (!cb) {
      Serial.println("no packet yet");
    }
    else {
      Serial.print("packet received, length=");
      Serial.println(cb);
      // We've received a packet, read the data from it
      udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

      //the timestamp starts at byte 40 of the received packet and is four bytes,
      // or two words, long. First, esxtract the two words:

      unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
      unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
      // combine the four bytes (two words) into a long integer
      // this is NTP time (seconds since Jan 1 1900):
      unsigned long secsSince1900 = highWord << 16 | lowWord;
      Serial.print("Seconds since Jan 1 1900 = " );
      Serial.println(secsSince1900);
      // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
      const unsigned long seventyYears = 2208988800UL;
      // subtract seventy years:
      unsigned long epoch = secsSince1900 - seventyYears;
      // print Unix time: after adjustment for local tiome zone
      LocalUnixTime = epoch + (3600 * TZ_ADJUST);
      setTime(LocalUnixTime);
      Serial.println("System Time Has been Set To ");
      Serial.print("date : ");
      Serial.print(year());
      Serial.print("-");
      Serial.print(month());
      Serial.print("-");
      Serial.print(day());
      Serial.print("  Time: ");
      Serial.print(hour());
      Serial.print(":");
      Serial.println(minute());
    }
    if (!cb) {
      delay(10000);  // wait 10 seconds before we make another request
    }
  } while (!cb); // Exit once we have set the time

  // check to see we have waited 1 minute 60000 millisecs so that the PIR will be settled
  // before we start to used it

  if ((millis() - StartTime) < 60000) {
    long int startdelay=(60000 -(millis()-StartTime));
    Serial.print("waiting ");
    Serial.print(startdelay/1000);
    Serial.println(" seconds for the PIR to stabilize");
    delay(startdelay);
  }
 
}


void loop() {

  if (HasBeen(DUTYCYCLETEMP, LastTimeTemp)) { // Wait between scans  for both Temp and Humidity
    CurrentTemp = ScanForTemp();
    Humidity = ScanForHumidity();
    DewPoint = dewPoint(CurrentTemp, Humidity);
    LastTimeTemp = millis();
  }

  if (HasBeen(DUTYCYCLEMOTION, LastTimeMotion)) { // wait between scans for motion
     MotionStatus = ScanForMotion();
     LastTimeMotion = millis();
     DoorStatus=ScanDoor();
  }

  if (HasBeen(DUTYCYCLEOUTPUT, LastTimeOutput)) {
    OutputNow = true;
    LastTimeOutput = millis();
    //Serial.println("time to print");
  }

  if (OutputNow) { // Wait between Output

    Serial.print("connecting to ");
    Serial.println(host);

    // Use WiFiClient class to create TCP connections
    WiFiClient client;
    const int httpPort = 80;
    if (!client.connect(host, httpPort)) {
      Serial.println("connection failed");
      return;
    }


    // We now create a URI for the request
    String url = "/DataLogger/DataLog_01.php";
    url += "?Obs_Date=";
    url += year();
    url += padZero(month());
    url += padZero(day());
    url += "&Obs_Time=";
    url += padZero(hour());
    url += ":";
    url += padZero(minute());
    url += ":";
    url += padZero(second());
    url += "&Station_ID=";
    url += StationID;
    url += "&Station_IP=";
    url +=  LocalIP;
    url += "&Station_Status=";
    url += SystemStatus;
    url += "&Access_Point=";
    url += WiFi.SSID();
    url += "&Humidity=";
    url += Humidity;
    url += "&Temperature=";
    url += CurrentTemp;
    url += "&Motion=";
    url += MotionStatus;
    url += "&DewPoint=";
    url += DewPoint;
    url += "&DoorStatus=";
    url += DoorStatus;
    url += "&Signal_Strength=";
    url +=  WiFi.RSSI();

    Serial.print("Requesting URL: ");
    Serial.println(url);

    // This will send the request to the server
    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" +
                 "Connection: close\r\n\r\n");
    unsigned long timeout = millis();
    while (client.available() == 0) {
      if (millis() - timeout > 5000) {
        Serial.println(">>> Client Timeout !");
        client.stop();
        return;
      }
    }

    // Read all the lines of the reply from server and print them to Serial
    while (client.available()) {
      String line = client.readStringUntil('\r');
      Serial.print(line);
      SystemStatus = "Normal";
    }

    Serial.println();
    Serial.println("closing connection");
    OutputNow = false;
    //Serial.println(LastTimeOutput);
    //Serial.println(OutputNow);

  }
}


// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(IPAddress& address)
{
  Serial.println("sending NTP packet...");
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udp.beginPacket(address, 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}

// Simple function that determines if we have waited TimeToWait Seconds or not
boolean HasBeen(unsigned long TimeToWait, unsigned long LastTime)
{
  boolean LongEnough = false;
  if (((millis()) - LastTime) > TimeToWait) {
    LongEnough = true;
  }
  return LongEnough;
}

String ScanForLight()
{
  return "UNK";
  String Intensity = "Dark";
  int LightValue = analogRead(lightPin); // Get the value from the Light resister
  //Serial.println(LightValue);
  // Convert the reading to a subjective range Dark, Dim, Bright, Very Bright
  if (LightValue > 900) {
    Intensity = "VERY BRIGHT";
  } else if (LightValue > 750) {
    Intensity = "BRIGHT";
  } else if (LightValue > 400) {
    Intensity = "DIM";
  } else if (LightValue > -1) {
    Intensity = "DARK";
  }

  return Intensity;
}
String ScanDoor()
{
 
  String Status="Unknown";
  
     int proximity = digitalRead(DOORMONITOR); // Read the state of the switch
     
     if (proximity == LOW) // If the pin reads low, the switch is closed door is closed
      {
       Status="Open";
      }
     else
      {
       Status="Closed";
      }
  if (LastDoorStatus != Status) {
    OutputNow=1;
    LastDoorStatus=Status;
  }
  return Status;
}

String ScanForMotion()
{
  int sensor_1 = digitalRead(MOTIONPIN);
  if (sensor_1 == HIGH) {
    MotionStatus = "YES";
  }
  else {
    MotionStatus = "No";
  }
  // if there is any chnage over the last time then output now
  // but we only report it if the min time between state changes has expired
  
  if (LastMotion != MotionStatus)  {
    if (HasBeen(MINSTATECHANGE,LastTimeStateChange)) {
    LastMotion = MotionStatus;
    OutputNow = true; // we want to trigger output any time we see a change in state
    LastTimeStateChange= millis();
    }
  }
  return MotionStatus;
}

float ScanForTemp()
{
  float TempReading;
  TempReading = dht.readTemperature();
  return TempReading;
}

float ScanForHumidity()
{
 
  float HumidityReading;
  HumidityReading = dht.readHumidity();
  return HumidityReading;
}

String padZero(int i)
{
  String padded;
  i = i + 100;
  padded = String(i);
  padded = padded.substring(1);
  return padded;
}

// dewPoint function NOAA
// reference (1) : http://wahiduddin.net/calc/density_algorithms.htm
// reference (2) : http://www.colorado.edu/geography/weather_station/Geog_site/about.htm
//
double dewPoint(double celsius, double humidity)
{
  // (1) Saturation Vapor Pressure = ESGG(T)
  double RATIO = 373.15 / (273.15 + celsius);
  double RHS = -7.90298 * (RATIO - 1);
  RHS += 5.02808 * log10(RATIO);
  RHS += -1.3816e-7 * (pow(10, (11.344 * (1 - 1 / RATIO ))) - 1) ;
  RHS += 8.1328e-3 * (pow(10, (-3.49149 * (RATIO - 1))) - 1) ;
  RHS += log10(1013.246);

  // factor -3 is to adjust units - Vapor Pressure SVP * humidity
  double VP = pow(10, RHS - 3) * humidity;

  // (2) DEWPOINT = F(Vapor Pressure)
  double T = log(VP / 0.61078); // temp var
  return (241.88 * T) / (17.558 - T);
}







