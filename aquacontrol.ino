//part of aquacontrol
//global vars --TOO MANY!!
//setup and loop
/*         aquacontrol for

                          Wemos D1 mini ESP8266 unit

           with custom hardware as a standalone aquarium controller
           see http://wasietsmet.nl/tag/aquacontrol/ for hardware description

           by Cellie 2016

           Mogelijkheden

           -Stuur 5 LED strips via mosfets aan om een aquarium te verlichten
           -Beheer van licht timerdata via een web interface
           -Opslaan van timerdata op SPIFFS disk
           -Automatische klok via NTP


*/


extern "C" {
#include "user_interface.h"
}

#include <ESP8266WiFi.h>
#include <DNSServer.h>
//#include <ESP8266WebServer.h>
#include <WiFiUdp.h>

#include "SSD1306.h"

//https://github.com/PaulStoffregen/Time
#include <Time.h>

#include <Ticker.h>
#include <FS.h>


String mySSID                          = "networkname" ;
String myPSK                           = "networkpass" ;


//GLOBALS

String hostName                      =  "aquacontrol";

const String programName             = "Aquacontrol8266";
const String compileDate          = __DATE__;
int cpuSpeed                         = 160;                          //in MHz - either 80 or 160 for esp8266

const char* ntpServerName            = "nl.pool.ntp.org";          //http://www.pool.ntp.org/use.html

//pwm setup
const float updateFrequency          =  1;                         //in Hz -- atm frequencies over 1hz are executed, but give the same output because the current time lib has a 1sec resolution
unsigned long int PWMfrequency       =  600;                       //https://github.com/esp8266/Arduino/issues/1265
unsigned int PWMdepth                =  PWMRANGE * 10;            //PWMRANGE defaults to 1023 on ESP8266 in Arduino IDE

//channel setup
const byte numberOfChannels         =  5  ;
const byte maxTimers                =  50 ;                       // aantal mogelijke timer momenten per kanaal

const byte ledPin[numberOfChannels] =  { D1, D2, D3, D4, D5 } ;   //pin numbers of the channels !!!!! should contain [numberOfChannels] entries. D1 through D8 are the exposed pins on 'Wemos D1 mini'
//see online for pin number conversion Arduino <> Wemos D1 mini: https://github.com/esp8266/Arduino/blob/master/variants/d1_mini/pins_arduino.h#L49-L61
//I2C pin settingsz
const byte   SCL_pin                = D6;
const byte   SDA_pin                = D7;

const byte atRequestPosition        =  4;                         // Start position of actual request in for example "GET / HTTP/1.1" ---> the '/' is at position 4

//time vars
time_t       bootTime;                                                // Keep track of boot time, to check stability - this is UTC time!
time_t       lastSyncTime;
time_t       nextSyncTime;
time_t       ntpSyncDelay           = 86400;                          //in seconds
//bool         dstSet                 = false;                          // daylight savings time - set via webinterface
int          timeZone;
const String dstFile                = "/dst.txt";
const String timeZoneFile           = "/timezone.txt";
const String ntpDelayFile           = "/ntpdelay.txt";


//OLED settings

const byte OLEDaddress              = 0x3c;
//const byte OLEDpowerPin           = D0;                           //might or might not be implemented - eg switch off the power to the oled

//filenames for setting files on spiffs
const String wifiDataFile           = "/wifidata.txt";
const String hostNameFile           = "/hostname.txt";
const String minimumLevelFile       = "/nightlevel.txt";
const String pwmFreqFile            = "/pwmfreq.txt";
const String pwmDepthFile           = "/pwmdepth.txt";
const String channelNamesFile       = "/channelnames.txt";
const String channelColorsFile      = "/channelcolors.txt";

//the beef of the program is constructed here
//first define a list of timers
struct lightTimer {
  time_t        time;                                                 //time in seconds since midnight so range is 0-86400
  byte          percentage;                                           // in percentage so range is 0-100
};

//then a struct for general housekeeping of a ledstrip
struct lightTable {
  lightTimer timer[maxTimers];
  String     name;                                                    //initially set to 'channel 1' 'channel 2' etc.
  String     color;                                                   //!!interface color, not light color! Can be 'red' or '#ff0000' or 'rgba(255,0,0,1)' but the setup will use hex eg '#ffffff' for white
  float      currentPercentage;                                       //what percentage is this channel set to
  byte       pin;                                                     //which pin is this mosfet -LED strip- on
  byte       numberOfTimers;                                          // daadwerkelijk gezette timers voor dit kanaal
  float      minimumLevel;                                            //never dim this channel below this percentage
};

//and make 5 instances
struct lightTable channel[numberOfChannels];                           //all channels are now memory allocated

String lightStatus;                                                   //To keep track if lights are on, off or programmed, to display this string on the webif

bool programOverride      = false;                                    // used for LIGHTS ON & LIGHTS OFF in webinterface

bool ntpError             = true;                                     //Network Time Protocol error -- must be set to true on boot

//serial logging
bool channelLogging       = false;                                     //logging of percentage % values over Serial. Useful when debugging unrelated stuff and a uncluttered screen
bool httpLogging          = false;                                     //all incoming http requests
bool apiLogging           = false;                                     //logging of api calls
bool memoryLogging        = false;                                     //shows RAM amount on change

WiFiClient serverClient;                                                   // A WiFiClient instance for the web interface

WiFiServer server(80);                                                  //and a server instance

Ticker channelUpdateTimer;                                              //for the channel updates

SSD1306  OLED( OLEDaddress, SDA_pin, SCL_pin );

///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////

void setup() {
  system_update_cpu_freq(cpuSpeed);  //todo: make adjustable in web interface AND SAVEable!!!!

  //setup channel names and set OUTPUT pinModes just in case...
  for (byte thisChannel = 0; thisChannel < numberOfChannels; thisChannel++ ) {
    channel[ thisChannel ].name = "CHANNEL" + String( thisChannel + 1 );
    channel[ thisChannel ].color = "white";
    channel[ thisChannel ].pin = ledPin[ thisChannel ];
    channel[ thisChannel ].minimumLevel = 0;
    pinMode( channel[ thisChannel ].pin, OUTPUT );
    digitalWrite( channel[ thisChannel ].pin, LOW );
  }
  OLED.init();
  OLED.clear();
  OLED.flipScreenVertically();

  Serial.begin(115200);  // Used to type in characters & debug
  Serial.println();
  Serial.println();

  //program info
  Serial.println( programName + F(" ") + compileDate );
  Serial.println();
  Serial.print( F( "Timers memory requirement: " ) );  Serial.print( sizeof( channel ) ) ;  Serial.println( F( " bytes" ) );
  Serial.print( ESP.getFreeHeap() );  Serial.println( F(" bytes of free RAM memory.") );
  Serial.println();

  //check if SPIFFS fs is present
  //  FSInfo fs_info;
  if ( SPIFFS.begin() ) {
    Serial.println( F("SPIFFS filesystem found." ) );
  } else {
    Serial.println( F("No SPIFFS filesystem found. Raising alert. Aborting mission..." ) );
    analogWriteFreq(3);
    analogWrite( BUILTIN_LED, PWMdepth / 2 );  /*blink the onboard LED to get some attention */
    OLED.clear();
    OLED.setTextAlignment( TEXT_ALIGN_CENTER );
    OLED.setFont( ArialMT_Plain_16 );
    OLED.drawString( 64, 30, F("NO SPIFFS!" ) );
    OLED.display();
    while (true) {
      delay(100); /* wait forever */
      yield();
    };
  }

  //check if index.htm is present and stop if it is not
  if ( !SPIFFS.exists( "/index.htm" ) ) {
    OLED.clear();
    OLED.setTextAlignment( TEXT_ALIGN_CENTER );
    OLED.setFont( ArialMT_Plain_16 );
    OLED.drawString( 64, 30, F("NO INDEX!" ) );
    OLED.display();
    while (true) {
      delay(100); /* wait forever */
      yield();
    };    
  }

  //now we have a spiffs disk and can setup WiFi

  //thisFile will be re-used throughout the whole setup() routine
  File thisFile;

  //check if we have WiFi credentials on disk 
  thisFile = SPIFFS.open( wifiDataFile , "r");
  if ( thisFile ) {
    while (thisFile.available()) {
      mySSID = thisFile.readStringUntil('\n');
      mySSID.trim();
      Serial.print( F("Network SSID " ) );  Serial.println( mySSID );
      myPSK = thisFile.readStringUntil('\n');
      myPSK.trim();
      Serial.print( "Trying password: " );  Serial.println( myPSK );
    }
    thisFile.close();
  } else {
    //no WiFi credentials found, start an accesspoint to setup WiFi
    startAP();
  }

  //check for 'hostname.txt' and set hostname accordingly
  thisFile = SPIFFS.open( hostNameFile , "r");
  if ( thisFile ) {
    Serial.println( F("Hostname found on disk" ) );
    while (thisFile.available()) {
      hostName = thisFile.readStringUntil('\n');
      hostName.trim();
    }
    thisFile.close();
  }
  WiFi.hostname( hostName );
  //Serial.print( F("Hostname set to: " ) );  Serial.println( hostName );

  //now actually connect to WiFi

  Serial.print( F("Connecting to WiFi network: " ) );  Serial.println( mySSID );

  WiFi.mode( WIFI_STA );

  WiFi.begin( mySSID.c_str(), myPSK.c_str() );

  OLED.clear();
  OLED.setTextAlignment( TEXT_ALIGN_CENTER );
  OLED.setFont( ArialMT_Plain_16 );
  OLED.drawString( 64, 0, F("Connecting to" ) );
  OLED.drawString( 64, 20, F("WiFi access point" ) );
  OLED.drawString( 64, 40, mySSID );
  OLED.display();

  //try to connect for x seconds
  int endTime = now() + 30;
  while ( now() < endTime && WiFi.status() != WL_CONNECTED ) {
    delay(100);
    Serial.print(".");
    yield();
  }
  Serial.println();

  if ( WiFi.status() != WL_CONNECTED ) {
    Serial.print( F("Could not connect to '") );  Serial.print( mySSID );  Serial.println( F("', starting WiFi setup access point.") );
    startAP();
  }

  OLED.clear();
  OLED.setTextAlignment(TEXT_ALIGN_CENTER);
  OLED.setFont(ArialMT_Plain_16);
  OLED.drawString(64, 20, F("Connected") );
  OLED.display();

  Serial.println( F("WiFi connected.") );

  //check for ntp delay file and setup accordingly
  thisFile = SPIFFS.open( ntpDelayFile , "r");
  if ( thisFile ) {
    while (thisFile.available()) {
      String ntpDelayStr = thisFile.readStringUntil( '\n' );
      ntpSyncDelay = ntpDelayStr.toInt();
      Serial.print( F("NTP delay file found. Delay set to: ") );  Serial.println( ntpSyncDelay );
    }
    thisFile.close();
  } else {
    Serial.print( F("Default NTP sync delay set to: ") );  Serial.println( ntpSyncDelay );
  }

  //check if hostname is correct
  if ( hostName != WiFi.hostname() ) {
    Serial.println( F("Difference between hostnames. Adjusting and reconnecting...") );
    WiFi.hostname(hostName);
    WiFi.reconnect();
  }
  
  //check for channelnames on disk and setup accordingly
  thisFile = SPIFFS.open( channelNamesFile , "r");
  byte thisChannel = 0;
  if ( thisFile ) {
    Serial.println( F("Channel name file found" ) );
    while ( thisFile.available() ) {
      channel[thisChannel].name = thisFile.readStringUntil('\n');
      thisChannel++;
    }
    thisFile.close();
  }

  //check for channelcolors on disk and setup accordingly
  thisFile = SPIFFS.open( channelColorsFile , "r");
  thisChannel = 0;
  if ( thisFile ) {
    Serial.println( F("Channel color file found") );
    while ( thisFile.available() ) {
      channel[thisChannel].color = thisFile.readStringUntil('\n');
      thisChannel++;
    }
    thisFile.close();
  }

  //check for timers on disk and setup accordingly
  if ( defaultTimers() ) {
    Serial.println( F("Default timers found on SPIFFS disk.") );
  } else {
    Serial.println( F("No timers on disk. Initializing channels with single timer.") );
    for ( byte thisChannel = 0; thisChannel < numberOfChannels; thisChannel++ ) {
      channel[thisChannel].timer[0].time = 0;
      channel[thisChannel].timer[0].percentage = 0;
      //and add the 24:00 timers
      channel[thisChannel].timer[1].time = 0;
      channel[thisChannel].timer[1].percentage = 0;
      channel[thisChannel].numberOfTimers = 1;                  //because zero based, there are actually 2 timers
    }
  }

  //get the minimum level info from spiffs disk
  thisFile = SPIFFS.open( minimumLevelFile , "r");
  if ( thisFile ) {
    Serial.println( F("Minimum levels file found") );
    while (thisFile.available()) {
      for (byte thisChannel = 0; thisChannel < numberOfChannels; thisChannel++ ) {
        String line = thisFile.readStringUntil('\n');
        channel[thisChannel].minimumLevel = line.toFloat();
      }
    }
    thisFile.close();
  }

  //get the PWM frequency from spiffs disk
  thisFile = SPIFFS.open( pwmFreqFile , "r");
  if ( thisFile ) {
    while (thisFile.available()) {
      String line = thisFile.readStringUntil('\n');
      PWMfrequency = line.toInt();
      Serial.println( F("PWM frequency file found.") );
    }
    thisFile.close();
  }
  analogWriteFreq(PWMfrequency);
  Serial.print( F("PWM frequency ") );  Serial.print( PWMfrequency );  Serial.println( F(" Hz") );

  //get the PWM depth from spiffs disk
  thisFile = SPIFFS.open( pwmDepthFile , "r");
  if ( thisFile ) {
    while (thisFile.available()) {
      String line = thisFile.readStringUntil('\n');
      PWMdepth = line.toInt();
      Serial.println( F("PWM depth file found.") );
    }
    thisFile.close();
  }
  analogWriteRange(PWMdepth);
  Serial.print( F("PWM depth ") );  Serial.print( PWMdepth );  Serial.println( F(" steps.") );

  //get the timezone info from spiffs disk
  thisFile = SPIFFS.open( timeZoneFile , "r");
  if ( thisFile ) {
    while (thisFile.available()) {
      String line = thisFile.readStringUntil('\n');
      timeZone = line.toInt();
      Serial.print( F("Timezone set to: ") );  Serial.println( line );
    }
    thisFile.close();
  } else {
    timeZone = 0;
    Serial.println( F("No timezone file found. Timezone set to UTC.") );
  }
/*
  //get the dst state from spiffs disk
  thisFile = SPIFFS.open( dstFile , "r");
  if ( thisFile ) {
    while (thisFile.available()) {
      String line = thisFile.readStringUntil('\n');
      if ( line.indexOf( "enabled") > -1 ) {
        dstSet = true;
        Serial.println( F("DST enabled.") );
      } else if (line.indexOf("disabled") > -1 ) {
        dstSet = false;
        Serial.println( F("DST disabled.") );
      }
    }
    thisFile.close();
  } else {
    dstSet = false;
    Serial.println( F("No dst settings found. DST disabled.") );
  }
*/
  
  Serial.println( F("Synching clock by NTP.") );
  byte numberOfTries = 3;
  byte counter = 1;
  while ( ntpError && counter <= numberOfTries ) {
    Serial.print( F("NTP sync try number ") );  Serial.println( counter );
    time_t result = getTimefromNTP();
    if ( result != 0 ) {
      setTime( result );
      bootTime = result;
      ntpError = false;
    } else {
      counter++;
    }
  }
  if ( ntpError ) {
    Serial.print( F("No NTP sync. System clock is reading: ") );
  } else {
    Serial.print( F("Local time set to ") );
  }
  Serial.println( formattedTime( localTime() ) );

  // Start the web server
  server.begin();
  //server.setNoDelay(false);

  Serial.print( F("Web server started at IP: ") );
  Serial.println( WiFi.localIP() );
  Serial.print( F(" Hostname: ") );
  Serial.println( WiFi.hostname() );

  lightStatus = F("Lights are controlled by program.");

  Serial.print( F("LED update frequency ") );  Serial.print( updateFrequency );  Serial.println( F(" Hz.") );
  Serial.println( F("Serial shutoff. (enable via webif)\nEntering main loop.") );
  delay(20);
  Serial.end();

  //set all channels
  channelUpdateTimer.attach_ms( 1000 / updateFrequency, updateChannels );         // Finally set the timer routine to update the leds
  updateChannels();

  nextSyncTime = now() + ntpSyncDelay;
  showHostname_IP_OLED();
  OLED.setContrast( 0 );

}  ////end setup and enter loop below

int previousFreeRAM; //for memory logging usage, see last lines of loop()

//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////


void loop() {
  //check if still WiFi connected.
  if ( WiFi.status() != WL_CONNECTED ) {
    OLED.clear();
    OLED.drawString(64, 0, F("WiFi reconnect.") );
    OLED.drawString(64, 20, F("Please hold on.") );
    OLED.display();
    while (WiFi.status() != WL_CONNECTED) {
      delay(20);
      yield();
    }
    showHostname_IP_OLED();
  }

  // fetch a server request if not busy with the previous one
  if ( !serverClient ) {
    serverClient = server.available();
  }

  //ntp sync
  //check if nextSyncTime has passed and if so, try to sync NTP --- but only if no server requests in queue
  if ( !serverClient && nextSyncTime <= now() ) {
    Serial.println( F("Automatic NTP update.") );
    time_t result = getTimefromNTP();
    if ( result != 0 ) {
      setTime( result );
      ntpError = false;
    }
    nextSyncTime += ntpSyncDelay;
    Serial.print( F("Next NTP auto sync in ") );  Serial.print( ntpSyncDelay );  Serial.println( F(" seconds.") );
  }


  // Process the web request
  if ( serverClient ) {  //
    String request = serverClient.readStringUntil('\r');
    if ( httpLogging && request.indexOf( "/api/" ) == -1 ) {
      Serial.print( F("RAW HTTP @ ") );  Serial.print( formattedTime( localTime() ) ); Serial.print( F(" ") ); Serial.println( request );
    }

    bool processed = false;

    //Is it a request for the frontpage? "/ " !!!with space!
    if ( request.indexOf( "/ " ) == atRequestPosition ) {
      fileSend( "/index.htm" );
      processed = true;
    }

    /// Is it an api call?
    if ( request.indexOf("/api/") == atRequestPosition ) {                                 /// Api calls first
      procesApiCall( request );
      processed = true;
    }

    if ( !processed ) {                                                                  //this flag is set to true on valid '/' or '/api/,,,' calls
      fileSend( request.substring( 4, request.lastIndexOf( " " ) ) );
    }
  }

  if (programOverride) {
    OLED.invertDisplay();
  }
  else {
    OLED.normalDisplay();
  }
  updateOLEDstring();
  
  if ( memoryLogging ) {
    //show mem usage
    int nowFreeRAM = ESP.getFreeHeap();
    if ( previousFreeRAM != nowFreeRAM) {
      Serial.print( nowFreeRAM );  Serial.println( F(" bytes RAM.") );
      previousFreeRAM = nowFreeRAM;
    }
  }
}

