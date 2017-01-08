//part of aquacontrol
//captive portal

//ideas and code stolen from:
//https://github.com/esp8266/Arduino/blob/master/libraries/DNSServer/examples/CaptivePortal/CaptivePortal.ino
//https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WebServer/examples/AdvancedWebServer/AdvancedWebServer.ino
//http://www.john-lassen.de/index.php/projects/esp-8266-arduino-ide-webconfig
//http://www.esp8266.com/viewtopic.php?f=32&t=5523
//http://www.esp8266.com/viewtopic.php?f=29&t=2153
//https://gist.github.com/igrr
//http://stackoverflow.com/a/17391629/6533013

DNSServer dnsServer;
Ticker resetTicker;

const char portalHTML[] = R"=====(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<link rel="icon" href="data:;base64,iVBORw0KGgo=">  <!--prevent favicon requests-->
<title>Aquacontrol Network Setup</title>
<script>

function testNetwork(networkName){
  var networkPass= prompt("Enter the password for '" + networkName + "'","");
  console.log( networkName + " " + networkPass );
  if ( networkPass != null ) {
    networkPass.trim();
    aside = document.getElementById("testResult");
    aside.innerHTML="<p>Connecting to " + networkName + "...</p>";
    if(XMLHttpRequest) var x = new XMLHttpRequest();
    else var x = new ActiveXObject("Microsoft.XMLHTTP");
    x.timeout = 15000;  //in ms
    x.onreadystatechange = function() {
      if (this.readyState == 4 && this.status == 200) {
       aside.innerHTML = this.responseText;
      }
    };            
    x.error = function ( error ) {
      aside.innerHTML = "Error: " + error;
    };
    x.ontimeout = function (e) {
      aside.innerHTML = "TIMEOUT - Please retry...";
    };
    x.open( "GET", "/test?" + networkName +"=" + networkPass , true );        
    x.send();
  }
}

function scanWiFi(networkName){
  accesspointList = document.getElementById("accesspointList");
  accesspointList.innerHTML="<p>scanning...</p>";
  if(XMLHttpRequest) var x = new XMLHttpRequest();
  else var x = new ActiveXObject("Microsoft.XMLHTTP");
  x.timeout = 15000;  //in ms
  x.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
     accesspointList.innerHTML = this.responseText;
     document.getElementById("testResult").innerHTML = "Select a WiFi network...";
    }
  };            
  x.error = function ( error ) {
    accesspointList.innerHTML = "Error: " + error;
  };
  x.ontimeout = function (e) {
    accesspointList.innerHTML = "TIMEOUT";
  };
  x.open( "GET", "/scan" , true );        
  x.send();
}  

</script>
</head>
<body style="text-align:center;">
<h1>WiFi setup for aquacontrol.</h1>
<div id="accesspointList" style="border:solid 1px;width:300px;height:300px;margin:auto;overflow:auto;">Wait for WiFi scan to complete</div>
<p id="testResult">Wait for WiFi scan to complete...</p>
<script>
 scanWiFi();
</script>
</body>
</html>
)=====";

bool testIsRunning;
String networkSSID,
       networkPSK; 
       
  //init OLED

void startAP(){       
  char*     accessPointName     = "aquacontrol";
  char*     accessPointPassword = "pw";
  IPAddress accessPointIP     ( 192, 168, 3, 1 );
  IPAddress accessPointNetmask( 255, 255, 255, 0 );

  const byte DNS_PORT = 53;
  
  OLED.clear();
  OLED.setTextAlignment(TEXT_ALIGN_CENTER);
  OLED.setFont(ArialMT_Plain_10);
  OLED.drawString(64,0, F("No WiFi found") );
  OLED.drawString(64,8, F("Connect to AP:") );
  OLED.setFont(ArialMT_Plain_24);
  OLED.drawString(64,16,accessPointName);
  OLED.setFont(ArialMT_Plain_10);
  OLED.drawString(64,40, F("to setup this device") );
  OLED.display();

  WiFi.mode( WIFI_AP );
  WiFi.softAP( accessPointName );                                                                    // --1  dit is de goede volgorde!
  WiFi.softAPConfig( accessPointIP, accessPointIP, accessPointNetmask );                           // --2  echt waar!
                                                                                                   //http://wasietsmet.nl/esp8266/esp8266-softap-en-softapconfig-in-arduino-ide/

  Serial.print( F("No WiFi possible. Started access point '") );  Serial.print( accessPointName );  Serial.println( F("'") );

  dnsServer.setTTL(300);
  dnsServer.setErrorReplyCode(DNSReplyCode::ServerFailure);
  
  // if DNSServer is started with "*" for domain name, it will reply with
  // provided IP to all DNS request
  //everything not configured leads to the portal --- ON HTTP NOT HTTPS@!!!
  //dnsServer.start(DNS_PORT, "aquacontrol", accessPointIP);
  dnsServer.start(DNS_PORT, "aquacontrol", accessPointIP);
  Serial.println( F("DNS server started.") );

  
  server.setNoDelay( true );   
  //serverClient.setNoDelay(true);   
  server.begin();
  Serial.println( F("Webserver started.") );

  testIsRunning = false;


  ///this is the 'main' loop
  while ( true ){
    if ( testIsRunning &&  WiFi.status() == WL_CONNECTED || WiFi.status() == WL_CONNECT_FAILED ) {
      testIsRunning = false;
      OLED.clear();
      OLED.drawString( 64, 0, WiFi.status() == WL_CONNECTED ? F("Success!!") : F("failed") );
      if ( WiFi.status() == WL_CONNECTED ) {
        OLED.drawString( 64, 16, networkSSID );
        OLED.drawString( 64, 32, F("Data saved.") );
        OLED.drawString( 64, 48, F("Rebooting..") );
        File f = SPIFFS.open( wifiDataFile , "w+");
        if (!f) {
          //show error
          Serial.println( F("WiFi data file creation failed") );
          return;
        } else {
          //write the WiFi data to spiffs
          f.println( networkSSID );
          f.println( networkPSK );
          f.close();
        }                  
        resetTicker.attach( 3 , resetESP ); //plan a reboot 3 seconds in the future
      }
      OLED.display();
    }
    
    if ( !serverClient ) {
      serverClient = server.available();
    } else {
      handleWebRequest();
    }
    dnsServer.processNextRequest();
    yield();
  }
  ////end of 'main' loop

  
}

void handleWebRequest(){
  String request = serverClient.readStringUntil('\r');
  if ( request.indexOf("/ ") == atRequestPosition ){
    handleRoot();
  } else if ( request.indexOf("/scan") == 4 ){
    handleScan();
  } else if ( request.indexOf("/test") == 4 ){
    handleTest( request );
  } else {            //send a 404
    serverClient.print( request + " not found.");
    serverClient.stop();  
  }
}

void handleTest( const String request ) {
  const String testHeaders = F(
    "HTTP/1.1 200 \r\n"
    "Content-Type: text/html\r\n"
    "Cache-Control: no-store, must-revalidate\r\n"
    "Connection:Keep-Alive\r\n"
    "Keep-Alive: timeout=30, max=1\r\n"
    "Access-Control-Allow-Origin: *\r\n"                   //allow same origin policy OVERRIDE!
    "Access-Control-Allow-Methods: GET, POST\r\n"
    "\r\n"
  );
  
  serverClient.print( testHeaders );  
  
  networkSSID = request.substring( request.indexOf("?") + 1 , request.indexOf("=") );
  networkPSK  = request.substring( request.indexOf( "=" ) + 1 , request.lastIndexOf(" ") );
  networkSSID.trim();
  networkPSK.trim();
  Serial.print( F("Starting connection test for ") );  Serial.println( networkSSID );  Serial.println( F(" with password ") );  Serial.println( networkPSK );

  OLED.clear();
  OLED.setTextAlignment(TEXT_ALIGN_CENTER);
  OLED.setFont(ArialMT_Plain_16);
  OLED.drawString( 64 , 0 , F("Probing WiFi AP:") );   
  OLED.drawString( 64 , 16 , networkSSID );   
  OLED.drawString( 64 , 32 , F("please wait.") );   
  OLED.display();

  WiFi.begin( networkSSID.c_str(), networkPSK.c_str() );
  testIsRunning = true;
  serverClient.print(  F("Probing ") );  serverClient.print( networkSSID );  
  serverClient.stop();
}

void resetESP(){
  ESP.restart();   //only works OK after a power cycle
}

void handleRoot(){
  const String rootHeaders = F(
    "HTTP/1.1 200 \r\n"
    "Content-Type: text/html\r\n"
    "Cache-Control: no-store, must-revalidate\r\n"
    "Connection: Keep-Alive\r\n"
    "Keep-Alive: timeout=30, max=1\r\n"
    "Access-Control-Allow-Origin: *\r\n"                   //allow same origin policy OVERRIDE!
    "Access-Control-Allow-Methods: GET, POST\r\n"
    "\r\n"
  );
  
  String responseText = portalHTML;
  serverClient.print( rootHeaders + responseText );
  serverClient.stop();
}





void handleScan( ){
  const String scanHeaders = F(
    "HTTP/1.1 200 \r\n"
    "Content-Type: text/html\r\n"
    "Cache-Control: no-store, must-revalidate\r\n"
    "Connection: Keep-Alive\r\n"
    "Keep-Alive: timeout=30, max=1\r\n"
    "Access-Control-Allow-Origin: *\r\n"                   //allow same origin policy OVERRIDE!
    "Access-Control-Allow-Methods: GET, POST\r\n"
    "\r\n"
  );  

  serverClient.print( scanHeaders );


  OLED.clear();
  OLED.setTextAlignment(TEXT_ALIGN_CENTER);
  OLED.setFont(ArialMT_Plain_16);
  OLED.drawString( 64 , 0 , F("Client connected") );   
  OLED.drawString( 64 , 20 , F("Scanning WiFi") );   
  OLED.drawString( 64 , 40 , F("Please wait.") );   
  OLED.display();
  
  int networks = WiFi.scanNetworks();
  String responseText;
  if ( networks > 0 ) {
    for ( int thisNetwork= 0; thisNetwork < networks; thisNetwork++ ) {
      responseText += "<p onclick=\"window.testNetwork(this.innerHTML);\">" + WiFi.SSID( thisNetwork ) + "</p>";
    }    
  } else {
    responseText += "<p>No WiFi networks in range.</p>";
  }
  serverClient.print( responseText );
  serverClient.stop();


  OLED.clear();
  OLED.drawString( 64 , 0 , F("Scan done") );   
  OLED.drawString( 64 , 20 , F("Select a")  );   
  OLED.drawString( 64 , 40 , F("WiFi network.") );   
  OLED.display();

  
}





