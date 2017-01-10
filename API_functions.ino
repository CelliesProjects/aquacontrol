//part of aquacontrol
//api processing
//      //         ///////   //
//     ////       //    //  //
//    /  //      //    //  //
//   //////     ///////   //
//  /    //    //        //



                    
                    
                    /*
  "Access-Control-Allow-Methods: GET, PUT, POST, DELETE, OPTIONS\r\n";/*
  "Access-Control-Allow-Headers: Origin, Content-Type, Content-Range, Content-Disposition, Content-Description, Accept\r\n"
  "Access-Control-Allow-Headers *AUTHORISED*\r\n";*/


void procesApiCall( const String request ) {

static const String apiHeaders =  F(
  "HTTP/1.1 200 \r\nContent-Type: text/plain\r\n"
  "Cache-Control: no-store, must-revalidate\r\n"
//  "Connection: keep-alive\r\n"
  "Connection: close\r\n"
  "Access-Control-Allow-Origin: *\r\n"                   //allow same origin policy OVERRIDE!
  "Access-Control-Allow-Methods: GET, POST\r\n"
  "\r\n"
);  
  
  if ( apiLogging ) {
    Serial.println( "RAW  API @ " + formattedTime( localTime() ) + " " + request );
  }

  if ( request.indexOf( F("/api/macadress") ) == atRequestPosition ) {
    serverClient.print( apiHeaders + String( WiFi.macAddress() ) );
    serverClient.stop();    
  }


  if ( request.indexOf( F("/api/oledcontrast?") ) == atRequestPosition ) {
    String contrastStr = request.substring( request.indexOf("?") + 1 , request.lastIndexOf(" ") );  //
    OLED.setContrast(contrastStr.toInt() );
    serverClient.print( apiHeaders + F("contrast set to ") + contrastStr );
    serverClient.stop();
  }

  if ( request.indexOf( F("/api/channelcolors ") ) == atRequestPosition ) {
    String channelColors = "";
    for (byte thisChannel = 0; thisChannel < numberOfChannels; thisChannel++ ) {
      channelColors += channel[thisChannel].color + "|";
    }
    serverClient.print( apiHeaders + channelColors );
    serverClient.stop();
  }

  if (request.indexOf( F("/api/ntpdelay ") ) == atRequestPosition ) {
    serverClient.print( apiHeaders + ntpSyncDelay );
    serverClient.stop();    
  }
 
  if (request.indexOf( F("/api/ntpsync ") ) == atRequestPosition ) {
    time_t result = getTimefromNTP();
    String HTMLstring;
    if (result == 0) {
      HTMLstring = F("NTP sync error.");
      //analogWrite(redLED, PWMdepth );
      ntpError = true;
    } else {
      setTime( result);
      ntpError = false;
      HTMLstring = "Clock synced at " + formattedTime( localTime() ) + " local time.";
    }
    serverClient.print( apiHeaders + HTMLstring );
    serverClient.stop();
  }//done

  if ( request.indexOf( F("/api/lastntpsync") ) == atRequestPosition ) {
    serverClient.print( apiHeaders + lastSyncTime );
    serverClient.stop();
  }

  if ( request.indexOf( F("/api/nextntpsync" ) ) == atRequestPosition ) {
    serverClient.print( apiHeaders + nextSyncTime );
    serverClient.stop();
  }
  
  if ( request.indexOf( F("/api/color?") ) == atRequestPosition ) {
    String channelStr = request.substring( request.indexOf("?") + 1 , request.indexOf(",") );  //
    byte thisChannel = channelStr.toInt();
    String colorStr = request.substring( request.indexOf(",") + 1 , request.lastIndexOf(" ") );
    channel[thisChannel].color = "#" + colorStr;
    File f = SPIFFS.open( channelColorsFile , "w+");
    if (!f) {
      //show error
      Serial.println( F("Channel color file creation failed" ) );
      serverClient.print( apiHeaders + F("ERROR saving channel colors.") );
      serverClient.stop();
      return;
    } else {
      //write the channelnames to spiffs
      for ( byte currentChannel = 0; currentChannel < numberOfChannels; currentChannel++ ) {
        f.print( channel[currentChannel].color + '\n' );  //dont forget the '\n'!!!
      }
      f.close();
    }
    serverClient.print( apiHeaders + F("Channel ") + ( thisChannel + 1 ) + F(" set to #") + colorStr );
    serverClient.stop();
  }

  if ( request.indexOf( F("/api/programname " ) ) == atRequestPosition ) {
    serverClient.print( apiHeaders + programName );
    serverClient.stop();
  }

  if ( request.indexOf( F( "/api/boottime " ) ) == atRequestPosition ) {
    serverClient.print( apiHeaders + bootTime );
    serverClient.stop();
  }

  if ( request.indexOf( F( "/api/version " ) ) == atRequestPosition ) {
    serverClient.print( apiHeaders + programVersion );
    serverClient.stop();
  }

  if ( request.indexOf( F( "/api/cpuspeed " ) ) == atRequestPosition ) {
    serverClient.print( apiHeaders + ESP.getCpuFreqMHz() );
    serverClient.stop();
  }

  if ( request.indexOf( F( "/api/cpuspeed?" ) ) == atRequestPosition ) {
    String newCPUfreq = request.substring( request.indexOf("?") + 1 , request.lastIndexOf(" ") );
    if ( newCPUfreq.toInt() == 80 ) {
      system_update_cpu_freq(80);
    } else if ( newCPUfreq.toInt() == 160 ) {
      system_update_cpu_freq(160);
    } else {
      serverClient.print( apiHeaders + F( "Invalid CPU frequency." ) );
      serverClient.stop();
      return;
    }
    cpuSpeed = ESP.getCpuFreqMHz();
    serverClient.print( apiHeaders + F( "New CPU speed: " ) + cpuSpeed + F( "Mhz" ) );
    serverClient.stop();
    //todo: save to disk and set at boot
  }

  if ( request.indexOf( F( "/api/restart " ) ) == atRequestPosition ) {
    serverClient.print( apiHeaders + F( "Rebooting..." ) );
    serverClient.stop();
    delay(100);
    ESP.reset();
  }

  if ( request.indexOf( F( "/api/channelnames " ) ) == atRequestPosition ) {
    String channelNames;
    for (byte thisChannel = 0; thisChannel < numberOfChannels; thisChannel++) {
      channelNames += channel[thisChannel].name + "|";
    }
    serverClient.print( apiHeaders + channelNames );
    serverClient.stop();
  }

  if ( request.indexOf( F( "/api/name?" ) ) == atRequestPosition ) {
    String channelStr = request.substring( request.indexOf("?") + 1 , request.indexOf(",") );
    byte thisChannel = channelStr.toInt();
    Serial.println( "channel:" + thisChannel);
    String nameStr = request.substring( request.indexOf(",") + 1 , request.lastIndexOf(" ") );
    nameStr.trim();
    channel[thisChannel].name = nameStr;
    File f = SPIFFS.open( channelNamesFile , "w+");
    if (!f) {
      //show error
      Serial.println( F( "Channel names file creation failed" ) );
      serverClient.print( apiHeaders + F("ERROR saving channel names." ) );
      serverClient.stop();
      return;
    } else {
      //write the channelnames to spiffs
      for ( byte currentChannel = 0; currentChannel < numberOfChannels; currentChannel++ ) {
        f.println( channel[currentChannel].name );
      }
      f.close();
    }
    serverClient.print( apiHeaders + "Channel " + ( thisChannel + 1 ) + " is now named '" + nameStr + "'" );
    serverClient.stop();
  }





  if ( request.indexOf( F( "/api/pwmdepth?" ) ) == atRequestPosition ) {
    String newPWMdepth = request.substring( request.indexOf("?") + 1 , request.lastIndexOf(" ") );
    PWMdepth = newPWMdepth.toInt();
    analogWriteRange(PWMdepth);
    //save the pwm depth to disk
    File f = SPIFFS.open( pwmDepthFile , "w+");
    if (!f) {
      Serial.println( F("PWM depth file creation failed" ) );
      serverClient.print( apiHeaders + F( "ERROR saving PWM depth." ) );
      serverClient.stop();
      return;
    }
    f.println(PWMdepth);
    f.close();
    serverClient.print( apiHeaders + F( "PWM depth is " ) + PWMdepth + F( " steps" ) );
    serverClient.stop();
  }

  if ( request.indexOf( F( "/api/pwmdepth " ) ) == atRequestPosition ) {
    serverClient.print( apiHeaders + PWMdepth);
    serverClient.stop();
  }

  if ( request.indexOf( F( "/api/setpercent?" ) ) == atRequestPosition ) {
    String newPercentage = request.substring( request.indexOf("?") + 1 , request.lastIndexOf(" ") );
    programOverride = true;
    Serial.print( F( "setting percentage" ) );  Serial.println( newPercentage);
    float thePercentage = newPercentage.toFloat();
    for (byte thisChannel = 0; thisChannel < numberOfChannels; thisChannel++) {
      channel[thisChannel].currentPercentage = thePercentage;
      analogWrite( channel[thisChannel].pin, mapFloat( channel[thisChannel].currentPercentage, 0, 100, 0, PWMdepth ) );
    }
    lightStatus = "All channels at " + newPercentage + "%.";
    serverClient.print( apiHeaders + lightStatus );
    serverClient.stop();
  }

  if ( request.indexOf( F( "/api/pwmfreq " ) ) == atRequestPosition ) {
    serverClient.print( apiHeaders + PWMfrequency );
    serverClient.stop();
  }


  if ( request.indexOf( F( "/api/pwmfreq?" ) ) == atRequestPosition ) {
    String newPWMfrequency = request.substring( request.indexOf("?") + 1 , request.lastIndexOf(" ") );
    PWMfrequency = newPWMfrequency.toInt();
    //check for illegal input
    if ( PWMfrequency < 1 || PWMfrequency > 1000 ) {
      serverClient.print( apiHeaders + F( "Invalid request" ) );
      serverClient.stop();
      return;
    }
    //save the pwm frequency to disk
    File f = SPIFFS.open( pwmFreqFile , "w+");
    if (!f) {
      Serial.println( F( "PWM frequency file creation failed" ) );
      serverClient.print( apiHeaders + F( "ERROR saving PWM frequency." ) );
      serverClient.stop();
      return;
    }
    f.println(PWMfrequency);
    f.close();
    analogWriteFreq(PWMfrequency);
    serverClient.print( apiHeaders + F( "PWM frequency set to " ) + PWMfrequency + F( "Hz" ) );
    serverClient.stop();
  }

  if ( request.indexOf( F( "/api/hostname " ) ) == atRequestPosition ) {
    serverClient.print( apiHeaders + hostName );
    serverClient.stop();
  }

  if ( request.indexOf( F( "/api/hostname?" ) ) == atRequestPosition ) {
    String newHostName = request.substring( request.indexOf("?") + 1 , request.lastIndexOf(" ") );

    newHostName.trim();

    //check for illegal characters --legal chars are alphanumeric
    for ( byte thisChar = 0; thisChar < newHostName.length(); thisChar++ ) {
      if ( !isAlphaNumeric( newHostName[thisChar] ) ) {
        serverClient.print( apiHeaders + "Invalid character in hostname." );
        serverClient.stop();      
        return;
      }
    }
    
    newHostName.toLowerCase();
    
    
    File f = SPIFFS.open( hostNameFile , "w+");
    if (!f) {
      Serial.println( F( "Hostname file creation failed" ) );
      serverClient.print( apiHeaders + F( "ERROR saving hostname." ) );
      serverClient.stop();
      return;
    }
    f.println(newHostName);
    f.close();

    hostName = newHostName;

    //wifi_station_set_hostname( ( char* ) hostName.c_str() );  //from espressif sdk, needs 'C' extern declaration at start

    WiFi.hostname(newHostName);
    serverClient.print( apiHeaders + F( "Hostname set to: " ) + hostName);
    serverClient.stop();
    delay(20);
    WiFi.reconnect();
    return;
  }

  if ( request.indexOf( F( "/api/nightlightlevel " ) ) == atRequestPosition ) {
    String HTTPstring;
    for ( byte thisChannel = 0; thisChannel < numberOfChannels; thisChannel++ ) {
      HTTPstring += channel[thisChannel].minimumLevel;
      HTTPstring += "-";
    }
    serverClient.print( apiHeaders + HTTPstring);
    serverClient.stop();
  }


  if ( request.indexOf( F( "/api/nightlightlevel?" ) ) == atRequestPosition ) {              ///call like:  /api/nightlightlevel?1-0.10 eg channel 1 at 0.10%
    String thisChannel = request.substring( request.indexOf("?") + 1 , request.indexOf("-") );
    String thisPercentage = request.substring( request.indexOf("-") + 1, request.lastIndexOf(" ") );
    channel[thisChannel.toInt()].minimumLevel = thisPercentage.toFloat();
    //save the nightlight level to disk
    File f = SPIFFS.open( minimumLevelFile , "w+");
    if (!f) {
      Serial.println( F("nightlight level file creation failed" ) );
      serverClient.print( apiHeaders + F( "ERROR saving nightlight level." ) );
      serverClient.stop();
      return;
    }
    for ( byte thisChannel = 0; thisChannel < numberOfChannels; thisChannel++ ) {
      f.println(channel[thisChannel].minimumLevel);
    }
    f.close();
    //setPercentage(thisChannel.toInt());
    serverClient.print( apiHeaders + F( "'" ) + channel[thisChannel.toInt()].name + F( "' minimum level " ) + thisPercentage + F( "%." ) );
    serverClient.stop();
  }


////////LOGGING

  if ( request.indexOf( F( "/api/nolog " ) ) == atRequestPosition ) {
    channelLogging = false;
    httpLogging = false;
    apiLogging = false;
    memoryLogging = false;
    serverClient.print( apiHeaders + F( "No logging - Serial port closed" ) );
    serverClient.stop();
    Serial.end();
  }

  if ( request.indexOf( F( "/api/channellog " ) ) == atRequestPosition ) {
    channelLogging = !channelLogging;
    Serial.begin(115200);
    serverClient.print( apiHeaders + F( "Channel logging " ) );
    serverClient.print( channelLogging ? "on" : "off" );
    serverClient.stop();
  }

  if ( request.indexOf( F( "/api/httplog " ) ) == atRequestPosition ) {
    httpLogging = !httpLogging;
    Serial.begin(115200);
    serverClient.print( apiHeaders + F( "http logging " ) );
    serverClient.print( httpLogging ? "on" : "off" );
    serverClient.stop();
  }

  if ( request.indexOf( F( "/api/apilog " ) ) == atRequestPosition ) {
    apiLogging = !apiLogging;
    Serial.begin(115200);
    serverClient.print( apiHeaders + F( "api logging " ) );
    serverClient.print( apiLogging  ? "on" : "off" );
    serverClient.stop();
  }

  if ( request.indexOf( F( "/api/ramlog " ) ) == atRequestPosition ) {
    memoryLogging = !memoryLogging;
    Serial.begin(115200);
    serverClient.print( apiHeaders + F( "memory logging " ) );
    serverClient.print( memoryLogging  ? "on" : "off" );
    serverClient.stop();
  }


//////FILES  
  
  if ( request.indexOf( F( "/api/files " ) ) == atRequestPosition ) {
    //show the file listing
    Dir dir = SPIFFS.openDir("/");
    serverClient.print(apiHeaders);
    while (dir.next()) {
      serverClient.print(dir.fileName());
      File f = dir.openFile("r");
      serverClient.print( F( " " ) );  serverClient.print( f.size() );  serverClient.println( F( " bytes" ) );
    }
    serverClient.stop();
  }

  if ( request.indexOf( F( "/api/removefile?" ) ) == atRequestPosition ) {
    String thisFile = request.substring( request.indexOf("?") + 1 , request.lastIndexOf(" ") );
    Serial.println(thisFile);
    if ( SPIFFS.remove(thisFile) ) {
      serverClient.print( apiHeaders + thisFile + F( " removed." ) );
    } else {
      serverClient.print( apiHeaders + thisFile + F( " not found." ) );
    }
    serverClient.stop();
  }
/*
  if (request.indexOf( F( "/api/dst" ) ) == atRequestPosition ) {
    if ( request.indexOf("?") != -1 ) {                                    //process
      String dstRequest = request.substring( request.indexOf("?") + 1 , request.lastIndexOf(" ") );
      dstRequest.toLowerCase();
      String httpStr = F( "DST is " );
      if ( dstRequest == F( "enabled" ) ) {
        dstSet = true;
        httpStr += dstRequest;
      } else if ( dstRequest == F( "disabled" ) ) {
        dstSet = false;
        httpStr += dstRequest;
      } else {
        httpStr = F( "No valid request" );
      }
      //save the setting in 'dstFile'

      File f = SPIFFS.open( dstFile , "w+");
      if (!f) {
        Serial.println( F( "timezone file creation failed" ) );
        serverClient.print( apiHeaders + F( "ERROR saving dst state." ) );
        serverClient.stop();
        return;
      }
      f.println(httpStr);
      f.close();
      serverClient.print( apiHeaders + httpStr );
    } else {    //return the current dst status
      String httpStr = F( "DST is " );
   
      
      //if ( dstSet ) {
      time_t = now();
      if ( IsDST( month(t), day(t), dayOfWeek(t)  ) ) {       
        httpStr += F( "enabled" );
      } else {
        httpStr += F( "disabled" );
      }

      serverClient.print( apiHeaders + httpStr );
    }
    serverClient.stop();
  }
*/
  if (request.indexOf( F( "/api/setclock?" ) ) == atRequestPosition ) {
    String timeData = request.substring( request.indexOf("?") + 1 , request.lastIndexOf(" ") );
    String timeDataHour = timeData.substring( timeData.indexOf("?") + 1 , timeData.indexOf(":") );
    String timeDataMinute = timeData.substring( timeData.indexOf(":") + 1 );
    unsigned int hourT = timeDataHour.toInt();
    unsigned int minuteT = timeDataMinute.toInt();
    setTime( hourT , minuteT, 0, 23, 8, 2016);
    serverClient.print( apiHeaders + F( "Clock set to " ) + timeDataHour + F( ":" ) + timeDataMinute );
    serverClient.stop();
  }










  if (request.indexOf( F( "/api/setntpsyncdelay?" ) ) == atRequestPosition ) {
    //get the time in seconds and set the NTP sync accordingly.
    String timeOffset = request.substring( request.indexOf("?") + 1 , request.lastIndexOf(" ") );
    unsigned long  newSyncDelay = timeOffset.toInt();

    if ( newSyncDelay < 300 ) {
      serverClient.print( apiHeaders + F( "ERROR: NTP sync delay too small. ( minimum = 300 sec )" ) );
      serverClient.stop();
      return;      
    }
    
    ntpSyncDelay = newSyncDelay;
    nextSyncTime = now() + newSyncDelay;
    //nextSyncTime += newSyncDelay;
    File f = SPIFFS.open( ntpDelayFile , "w+");
    if (!f) {
      Serial.println( F( "NTP delay file creation failed" ) );
      serverClient.print( apiHeaders + F( "ERROR saving NTP delay." ) );
      serverClient.stop();
      return;
    }
    f.println(ntpSyncDelay);
    f.close();
    serverClient.print( apiHeaders + nextSyncTime);
    serverClient.stop();
  }





















  if (request.indexOf( F( "/api/store " ) ) == atRequestPosition ) {
    // save the data as 'default.aqu' on SPIFFS disk
    File defaultFile = SPIFFS.open("/default.aqu", "w+");

    if ( !defaultFile ) {
      Serial.println( F( "Default file error!" ) );
      serverClient.print( apiHeaders + F( "Default file error!" ) );
      serverClient.stop();
      return;
    }

    for (byte thisChannel = 0; thisChannel < numberOfChannels; thisChannel++ ) {
      defaultFile.print("[" + (String)thisChannel + "]\r\n");
      for (byte thisTimer = 0; thisTimer < channel[thisChannel].numberOfTimers; thisTimer++ ) {
        defaultFile.print( (String)channel[thisChannel].timer[thisTimer].time + "," + (String)channel[thisChannel].timer[thisTimer].percentage + "\r\n" );
      }
    }
    defaultFile.close();
    serverClient.print(apiHeaders + F( "Current timers stored as default." ) );
    serverClient.stop();
  }//done






  if ( request.indexOf( F( "/api/timezone " ) ) == atRequestPosition ) {
    serverClient.print(apiHeaders + timeZone);
    serverClient.stop();
  }





  if ( request.indexOf( F( "/api/settimezone?" ) ) == atRequestPosition ) {
    String timeOffset = request.substring( request.indexOf("?") + 1 , request.lastIndexOf(" ") );
    timeZone = timeOffset.toInt();
    updateChannels();
    // open the file in write mode
    //SPIFFS.remove(timeZoneFile);
    File thisFile = SPIFFS.open( timeZoneFile , "w+");
    if (!thisFile) {
      Serial.println( F( "timezone file creation failed" ) );
    }
    thisFile.println(timeZone);
    thisFile.close();

    String httpStr = apiHeaders +  F( "Timezone UTC" );
    if ( timeZone > -1 ) {
      httpStr += F( "+" );
    }
    httpStr += ( String )timeZone + F( ":00 set as default." );
    serverClient.print( httpStr );
    serverClient.stop();
  }




  if (request.indexOf( F( "/api/settimers?" ) ) == atRequestPosition ) {                           //receive timerdata for one channel
    String timerData = request.substring( request.indexOf("?") + 1 , request.lastIndexOf(" ") );

    //process the timerData
    int startPos, endPos;
    byte currentTimer = 0;
    String timerStr;
    lightTimer receivedTimer[maxTimers]; //temporary storage of timer data

    //split the data on the "-" limiter
    startPos = 0;
    endPos = timerData.indexOf("-");
    String channelNumber = timerData.substring( startPos, endPos );
    byte thisChannel = channelNumber.toInt();
    startPos = endPos + 1;
    //Serial.println("channel: " + channelNumber );
    while ( startPos < timerData.length() ) {
      endPos = timerData.indexOf("-", startPos );
      timerStr = timerData.substring( startPos, endPos );

      //split this data on the "," limiter
      String temp;
      temp = timerStr.substring(0, timerStr.indexOf(",") );
      receivedTimer[currentTimer].time = temp.toInt();

      temp = timerStr.substring( timerStr.indexOf( "," ) + 1 );
      receivedTimer[currentTimer].percentage = temp.toInt();
      startPos = endPos + 1;  //skip the next "-"
      currentTimer++;
    }

    //now copy the received data to the correct channel
    for (byte thisTimer = 0; thisTimer < currentTimer; thisTimer++) {
      channel[thisChannel].timer[thisTimer].time = receivedTimer[thisTimer].time;
      channel[thisChannel].timer[thisTimer].percentage = receivedTimer[thisTimer].percentage;
    }
    //add the first timer again at 24:00
    channel[thisChannel].timer[currentTimer].time = 86400;
    channel[thisChannel].timer[currentTimer].percentage = receivedTimer[0].percentage;

    channel[thisChannel].numberOfTimers = currentTimer;

    //tell the serverClient we did good
    serverClient.print( apiHeaders + channelNumber + F( ": " ) + String(currentTimer) + F( " timers processed." ) );
    serverClient.stop();
  }//done



  //sends back:
  // chan0%,chan1%,chan2%,chan3%,chan4%,localtime,lightstatus
  if (request.indexOf( F("/api/getpercentage ") ) == atRequestPosition ) {          ///stuurt nu ook de systeemtijd mee
    String HTMLstring;
    for (byte thisChannel = 0; thisChannel < numberOfChannels; thisChannel++) {
      HTMLstring += String(channel[thisChannel].currentPercentage) + F( "," );
    }
    HTMLstring += formattedTime( localTime() ) + F( "," );
    HTMLstring += lightStatus;
    serverClient.print( apiHeaders + HTMLstring);
    serverClient.stop();
  }//done



  if (request.indexOf( F( "/api/lightson " ) ) == atRequestPosition ) {
    programOverride = true;
    for (byte thisChannel = 0; thisChannel < numberOfChannels; thisChannel++) {
      analogWrite(channel[thisChannel].pin, PWMdepth);
      channel[thisChannel].currentPercentage = 100;
    }
    lightStatus = F( "Lights are on." );
    serverClient.print( apiHeaders + lightStatus );
    serverClient.stop();
  }//done




  if (request.indexOf( F( "/api/lightsoff " ) ) == atRequestPosition ) {
    programOverride = true;
    for (byte thisChannel = 0; thisChannel < numberOfChannels; thisChannel++) {
      analogWrite(channel[thisChannel].pin, 0);
      channel[thisChannel].currentPercentage = 0;
    }
    lightStatus = F( "Lights are off." ); 
    serverClient.print( apiHeaders + lightStatus );
    serverClient.stop();
  }//done

  if (request.indexOf( F( "/api/lightsprogram " ) ) == atRequestPosition ) {
    programOverride = false;
    updateChannels();
    lightStatus = F( "Lights controlled by program." );
    serverClient.print( apiHeaders + lightStatus );
    serverClient.stop();
  }//done

  if (request.indexOf( F("/api/timers?channel=" ) ) == atRequestPosition ) {
    //get the channelname from the request
    String channelname = request.substring(request.indexOf( "=" ) + 1 , request.lastIndexOf( " " )  );
    int thisChannel = channelname.toInt();
    String httpStr;
    if ( thisChannel > numberOfChannels - 1 || thisChannel < 0 ) {
      httpStr = F( "ERROR:Invalid channel number!" );
      serverClient.print( apiHeaders + httpStr );
      serverClient.stop();
      return;
    }
    for ( byte counter = 0; counter < channel[thisChannel].numberOfTimers; counter++ ) { //find out number of timers in thisChannel
      httpStr += String(channel[thisChannel].timer[counter].time) + "," + String(channel[thisChannel].timer[counter].percentage)  + "-";
    }
    serverClient.print( apiHeaders + httpStr );
    serverClient.stop();
  }//done


  //check if client has been serviced, if not send a 404 and close client
  //if there is still a client obviously there was no api call that matched
  if ( serverClient ){
    serverClient.print( apiHeaders + F("404 not found\n") );
    serverClient.stop();    
  }
  
}
