//part of aquacontrol
//routines for controller

String hexMac() {
  uint8_t MAC_array[6];
  char MAC_char[18];
  WiFi.macAddress(MAC_array);
  for (int i = 0; i < sizeof(MAC_array); ++i) {
    sprintf(MAC_char, "%s%02x:", MAC_char, MAC_array[i]);
  }
}

float mapFloat( double x, const double in_min, const double in_max, const double out_min, const double out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

String formattedTime( const time_t t) {  ///output a time with if necessary leading zeros
  String s = String(hour(t)) + ":";
  if ( (byte)minute(t) < 10 ) {
    s += "0";
  }
  s += String(minute(t)) + ":";

  if ( (byte)second(t) < 10 ) {
    s += "0";
  }
  s += second(t);
  return s;
}

time_t localTime() {
  time_t t = now() + ( timeZone * 3600 );
  //if ( dstSet ) {
  if ( IsDST( month(t), day(t), dayOfWeek(t) ) ) {
    t += 3600;
  }
  return t;
}

// IsDST(): returns true if during DST, false otherwise
boolean IsDST(int mo, int dy, int dw) {
  if (mo < 3 || mo > 11) { return false; } // January, February, and December are out.
  if (mo > 3 && mo < 11) { return true; } // April to October are in
  int previousSunday = dy - dw; 
  if (mo == 3) { return previousSunday >= 8; } // In March, we are DST if our previous Sunday was on or after the 8th.
  return previousSunday <= 0; // In November we must be before the first Sunday to be DST. That means the previous Sunday must be before the 1st.
}

void setPercentage( const byte thisChannel, const time_t secondsToday ) {
  if ( !programOverride && ( secondsToday != 0 ) ) {     ///to solve flashing at midnight due to secondsToday which cant be smaller than 0 -- so at midnight there is no adjusting
    byte thisTimer = 0;
    while ( channel[thisChannel].timer[thisTimer].time < secondsToday ) {
      thisTimer++;
    }
    float dimPercentage = channel[thisChannel].timer[thisTimer].percentage - channel[thisChannel].timer[thisTimer - 1].percentage;
    time_t numberOfSecondsBetween = channel[thisChannel].timer[thisTimer].time - channel[thisChannel].timer[thisTimer - 1].time;
    time_t secondsSinceLastTimer = secondsToday - channel[thisChannel].timer[thisTimer - 1].time;
    float changePerSecond  = dimPercentage / numberOfSecondsBetween;
    channel[thisChannel].currentPercentage = channel[thisChannel].timer[thisTimer - 1].percentage + (secondsSinceLastTimer * changePerSecond);

    //check if channel is nightlight and set accordingly
    if ( channel[thisChannel].currentPercentage < channel[thisChannel].minimumLevel ) {
      channel[thisChannel].currentPercentage = channel[thisChannel].minimumLevel;
    }

    analogWrite( channel[thisChannel].pin, (int)mapFloat( channel[thisChannel].currentPercentage, 0, 100, 0, PWMdepth ) );

    if (channelLogging) {
      Serial.print( F("Channel: ") );
      Serial.print( channel[thisChannel].name );
      Serial.print( F(" ") );
      Serial.print( String( channel[ thisChannel ].currentPercentage ) );
      String rawValue = String( (int)mapFloat( channel[thisChannel].currentPercentage, 0, 100, 0, PWMdepth ) );
      Serial.print( F("% RAW: ") );
      Serial.println(  rawValue );
    }
  }
}

void updateChannels() {
  if (channelLogging) {
    Serial.print( F("Updating all channels at ") );  Serial.print( formattedTime( localTime() ) );  Serial.println( F(" local time.") );
  }
  //get the current timeInSeconds and use that as time argument for setPercentage
  time_t secondsToday = elapsedSecsToday( localTime() );      //elapsedSecsToday lives in TimeLib.h  
  
  for ( byte thisChannel = 0; thisChannel < numberOfChannels; thisChannel++ ) {
    setPercentage(thisChannel, secondsToday );
  }
}

bool defaultTimers() {                                                      //this function loads the timers or returns FALSE
  //find 'default.aqu' on SPIFFS disk and if present load the timerdata from this file
  //return false on error
  File f = SPIFFS.open( "/default.aqu", "r" );
  if (!f) {
    Serial.println( F("ERROR: No SPIFFS file!") );
    return false;
  }
  String lineBuf;
  byte currentTimer = 0;
  byte thisChannel;
  while ( f.position() < f.size() ) {
    lineBuf =    f.readStringUntil('\r');
    if ( lineBuf.indexOf( "[" ) > -1 ) {
      String thisChannelStr;
      thisChannelStr = lineBuf.substring( lineBuf.indexOf("[") + 1, lineBuf.indexOf("]") );
      thisChannel = thisChannelStr.toInt();
      currentTimer = 0;
    } else {
      if (lineBuf.length() > 3 ) {                                        //weird leftover buffer thing - todo: what was that?
        String thisPercentage;
        String thisTime;
        thisTime = lineBuf.substring( 0, lineBuf.indexOf(",") );
        thisPercentage = lineBuf.substring( lineBuf.indexOf(",") + 1 );
        channel[thisChannel].timer[currentTimer].time = thisTime.toInt();
        channel[thisChannel].timer[currentTimer].percentage = thisPercentage.toInt();
        currentTimer++;
        channel[thisChannel].numberOfTimers = currentTimer;
      }
    }
  }
    f.close();
  //add the 24:00 timers ( copy of timer percentage no: 0 )
  for (thisChannel = 0; thisChannel < numberOfChannels; thisChannel++ ) {
    channel[thisChannel].timer[channel[thisChannel].numberOfTimers].time = 86400;
    channel[thisChannel].timer[channel[thisChannel].numberOfTimers].percentage = channel[thisChannel].timer[0].percentage;
  }
  return true;
}

void fileSend( const String fileName) {

  File thisFile = SPIFFS.open(fileName, "r");
  if (!thisFile) {
    serverClient.print( F("HTTP/1.1 404 Not found\r\n\r\nERROR 404 - '") );
    serverClient.print( fileName );
    serverClient.print( F("' was not found.") );
    serverClient.stop();
    return;
  }
  //check the file's extention and tailor the header for that
  String httpHeader = F("HTTP/1.1 200 \r\nContent-Type: text/plain\r\n");

  if ( fileName.substring(  fileName.length() - 4 ) == ".css" ) {
    httpHeader = F("HTTP/1.1 200 \r\nContent-Type: text/css\r\n");
  }

  if ( fileName.substring(  fileName.length() - 4 ) == ".htm" ) {
    httpHeader = F("HTTP/1.1 200 \r\nContent-Type: text/html\r\n");
  }
  httpHeader += "Content-Length: " + (String)thisFile.size() + "\r\n\r\n" ;
  serverClient.print( httpHeader );

  int bufferSize = 1460 * 2;
  char rwBuffer[bufferSize];
  while ( thisFile.available() > bufferSize ) {
    thisFile.readBytes(rwBuffer, bufferSize);
    serverClient.write(rwBuffer, bufferSize);
    yield();
  }
  //send last chunk
  bufferSize = thisFile.available();
  thisFile.readBytes(rwBuffer, bufferSize);
  serverClient.write(rwBuffer, bufferSize);
  serverClient.stop();
  thisFile.close();
}//done
