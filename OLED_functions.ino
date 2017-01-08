

//not implemented because flicker
void updateOLEDbar(){
  int barWidth = DISPLAY_WIDTH / numberOfChannels;
  OLED.clear();
  for ( byte thisChannel =0; thisChannel < numberOfChannels;thisChannel++){
    //int x1= barWidth * thisChannel;
    //int y1= ( DISPLAY_HEIGHT ) - ( DISPLAY_HEIGHT * ( channel[thisChannel].currentPercentage / 100 ) );
    //int x2 = x1 + barWidth;
    //int y2 = ( DISPLAY_HEIGHT );
    //OLED.drawRect(x1,y1,x2,y2);
    OLED.drawString(64, 10 * thisChannel, String( channel[thisChannel].currentPercentage ) );    
  }
  OLED.display();
}

//not implemented because flicker
void updateOLEDstring(){
  OLED.clear();
  OLED.setTextAlignment(TEXT_ALIGN_CENTER);
  for ( byte thisChannel = 0; thisChannel < numberOfChannels; thisChannel++ ){
    OLED.drawString(64, 10 * thisChannel, String( channel[thisChannel].currentPercentage ) );    
  }
  OLED.drawString(64,50,formattedTime( localTime() ) );
  OLED.display();
}

void showHostname_IP_OLED(){
  OLED.setTextAlignment(TEXT_ALIGN_CENTER);
  OLED.clear();
  OLED.setFont(ArialMT_Plain_10);
  OLED.drawString(64,0, F("IP:") );
  OLED.setFont(ArialMT_Plain_16);
  OLED.drawString(64,12, WiFi.localIP().toString() );
  OLED.setFont(ArialMT_Plain_10);
  OLED.drawString(64,32, F("HOSTNAME:") );
  hostName.trim();
  OLED.setFont(ArialMT_Plain_16);
  OLED.drawString(64,44, hostName );
  OLED.display();      
}
