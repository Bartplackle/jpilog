/*
The MIT License (MIT) 
 
Bart PLackle 
 
Permission is hereby granted, free of charge, to any person obtaining a copy 
of this software and associated documentation files (the "Software"), to deal 
in the Software without restriction, including without limitation the rights 
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell 
copies of the Software, and to permit persons to whom the Software is 
furnished to do so, subject to the following conditions: 
  
The above copyright notice and this permission notice shall be included in all 
copies or substantial portions of the Software. 
 
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
SOFTWARE. 
*/



#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ESPMail.h>

#include <SPI.h>
#include <SD.h>

// USER SETTINGS 
const char mailserver[] = "relay.proximus.be";        // replace with your mailserver address (linked to your mobile service provider !!
const int mailport = 25;                             // SMTP port 25, 
String mailto = "xx@yy.zz";                   // email address to send the JPI log to
String mailfrom = xx@yy.zz";                 // email address from 
const char ssid[] = "MyPhoneSSID";                        // SSID of your phone access point
const char password[] = "Mypassword";                  // phone access point password
// END OF USER settings


#define NOCARD 1
#define NOFAT 2
#define ERRWRITING 3
#define OK 0

#define BUFSIZE 1000
#define simfile           //def simfile to check if sending testfile works (without JPI downloading) 
#define simefilesize 100

// set up variables using the SD utility library functions:
Sd2Card card;
SdVolume volume;
SdFile root;

const int chipSelect = D8;
const int LED = D4;
const int TONE = D3;


WiFiClient client;
File picture;
File mime;

static const char cb64[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

String vers = "OTA JMP mailer";
String infomessage = "4-4-2018 version include OTA, JMP email logger";


   
int blink=0;
int err=0;
int cycle=0;
long ontime=0;

ESP8266WebServer server(80);

const int led = 13;
int cnt=0;

void encodeblock(unsigned char in[3],unsigned char out[4],int len) {
out[0]=cb64[in[0]>>2]; out[1]=cb64[((in[0]&0x03)<<4)|((in[1]&0xF0)>>4)];
out[2]=(unsigned char) (len>1 ? cb64[((in[1]&0x0F)<<2)|((in[2]&0xC0)>>6)] : '=');
out[3]=(unsigned char) (len>2 ? cb64[in[2]&0x3F] : '=');
}


void encode() { // encodes the datalog stream ready to be attached as mime


File dataFile = SD.open("datalog.txt");

if (dataFile) Serial.println("Successfull opened datalog file");

Serial.println(dataFile.size());
Serial.println(dataFile.available());


if (SD.exists("enc.txt")) SD.remove("enc.txt");
File mime = SD.open("enc.txt",FILE_WRITE);

if (mime) Serial.println("Successfull opened encode file");
else Serial.println("Failed opening encode file");


unsigned char in[3],out[4]; int i,len,blocksout=0;
int blocks=0;
while (dataFile.available()!=0) 
{
  len=0; 
  for (i=0;i<3;i++) 
  { in[i]=(unsigned char) dataFile.read(); 
     if (dataFile.available()!=0) len++; else in[i]=0; 
  }
      if (len) { encodeblock(in,out,len); 
                 for(i=0;i<4;i++) mime.write(out[i]); 
                 blocksout++; 
                 }
      if (blocksout>=19||dataFile.available()==0) 
         { if (blocksout) mime.print("\r\n");blocksout=0;
            Serial.print("encoded block ");
            Serial.println(blocks++);
            
            }
}

mime.close();
dataFile.close();
Serial.println("encoding done");

}


void handleRoot() {

String message = "";
Serial.println("new client");
         

message ="<html>\
    <title>JPMupdater  Server </title>";
            message+="JPM updater " ;
            message+="<br />" ;  
            message+="</html> \n"; 
            
  server.send(200, "text/html", message);    
}

void handleDuty() {
String message = "";
message ="<html>\
    <title>JPMupdater Server </title>";
            message+=infomessage ;
            message+="<br />" ; 
            message+="<br />" ; 
            message+="VERSION :" ;
            message+=vers  ;
           
            message+="<br />" ; 
            message+="<br />" ; 
            message+="</html> \n";

  server.send(200, "text/html", message);
      
}

void handleInfo() {
  handleDuty();
}


void handleNotFound(){
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}


void beep (int num,int ms)
{
  
  for (int i=0;i<num;i++)
  {
  digitalWrite(TONE,LOW);
  delay(ms);
  digitalWrite(TONE,HIGH);
  delay(ms);   
  }
 
}


void setup(void){
   
  Serial.begin(19200);
  pinMode(LED, OUTPUT);
  pinMode(TONE, OUTPUT);
  
  digitalWrite(LED, LOW);
  delay(500);
  digitalWrite(LED, HIGH);
  digitalWrite(TONE, HIGH);
  
   Serial.print("Initializing SD card...");

  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
    err=NOCARD;
    return;
  }
  Serial.println("card initialized.");
  beep(2,10);


  WiFi.mode(WIFI_STA);
  //WiFi.config(ip,gateway,subnet,dns,dns);
    
  WiFi.begin(ssid, password);
  Serial.println("");

  int cnt=0;
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(cnt*20);
    cnt++;
    Serial.print(".");
    beep(1,cnt);
   if (cnt==10) cnt=1;
    //doesn't make sense to continue unless connected to wifi 
  }

  ArduinoOTA.setHostname("JPM updater");

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  
  
  });
  ArduinoOTA.onError([](ota_error_t error) {
    
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR)  {Serial.println("Auth Failed"); }
    else if (error == OTA_BEGIN_ERROR) {Serial.println("Begin Failed");}
    else if (error == OTA_CONNECT_ERROR) {Serial.println("Connect Failed");}
    else if (error == OTA_RECEIVE_ERROR) {Serial.println("Receive Failed");}
    else if (error == OTA_END_ERROR) {Serial.println("End Failed");}
  });
  ArduinoOTA.begin();
 
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  
  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS responder started");
  }

  server.on("/", handleRoot);
  server.on("/INFO", []() { handleInfo();  });


  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");
    int timeout=0;
  
    while (timeout < 100) //if no data for 250 ms assuming TX is done
    {
      int cnt=0;
      if (Serial.available()) //ensure buffer flush before starting
      {
        timeout = 0;
        Serial.read();
        cnt++;
      }
      else
      {
        timeout++;
        delay(1);
      }
    }
  beep (2,50);
  
  
  Serial.println("serial buffer clear :D ");
  if (SD.exists("datalog.txt")) SD.remove("datalog.txt");
  
  File dataFile = SD.open("datalog.txt",FILE_WRITE);
  if (dataFile) Serial.println("Successfully created new datalog.txt");


  
  
  if (dataFile) { digitalWrite(LED, HIGH);
  Serial.println("file opened");
  int timeout = 0;
  }

  Serial.println("waiting for serial ");

#ifdef simfile
  // simulating file creating to test email encode and send path     
  for (int i=0; i<simefilesize;i++) 
    dataFile.println("Hello Brave new world 123456789");
  dataFile.close();
 
#else
   int c=0;            
   while (!Serial.available()) {
   c++;
   delay(1);
   if (c==2000){ 
     beep(5,20);
     c=0;
      Serial.print("X");
     }
   }
   Serial.println("serial transmission started ");
    
  // if the file is available, write to it:
  if (dataFile) { 

       while (timeout < 300) //if no data for 250 ms assuming TX is done
    {

      if (Serial.available())
      {
        timeout = 0;
       dataFile.write(Serial.read());
       digitalWrite(LED, HIGH);
      }
      else
      {
        timeout++;
        digitalWrite(LED, LOW);
        delay(5);
      }
    }
    
    dataFile.close();
    Serial.println("Serial transmission ended");
   
    err=OK;
  }
  // if the file isn't open, pop up an error:
  else {
    Serial.println("error opening ");
    err=ERRWRITING;
  }


#endif


  beep (5,20);
  sendAttach();
  beep (30,20);


Serial.println("Done sending log ");
   
}



void sendAttach()
{

char buf[BUFSIZE];

encode();
File mime = SD.open("enc.txt");
if (mime) Serial.println("Successfull opened encode file");
else Serial.println("Failed opening encode file");

if (client.connect(mailserver,mailport)==1) Serial.println("Opened to SMTP server succesfull");

String OpenStr="HELO "+ (String) mailserver + " \n";
client.print(OpenStr);
eRcv();
String MailhdrStr="MAIL FROM:<" + mailfrom + ">\nRCPT TO:<" + mailto + ">\nDATA\nFrom: \"JPMLOG\" <" + mailfrom + ">\r\nTo: "+ mailto +" \r\nSubject: Picture attached\r\n" ;
client.print(MailhdrStr);
Serial.println(MailhdrStr);


eRcv();
client.print("Content-Type:application/octet-stream ; name=\"jpmlog.jpi\"\r\nContent-Disposition: attachment; filename=\"jpmlog.jpi\"\r\nContent-Transfer-Encoding: base64\r\n\r\n");
eRcv();


Serial.println("Started sending attachment");

int i=0;
while (mime.available())
{ 
  buf[i]=mime.read();
  i++;
  if (i==BUFSIZE) 
      {client.write((const uint8_t *)buf,BUFSIZE);
       i=0;
       Serial.print(".");
        while(client.available())
        {  
          Serial.write(client.read());
         }
      }
  }
  client.write((const uint8_t *) buf,i);
  
mime.close();
Serial.println("File transmitted");

delay (500);
while(client.available())
        {  
          Serial.write(client.read());
         }

client.print("\r\n.\r\nQUIT\n");
client.stop();

Serial.println("closing SMTP");
  
}


byte eRcv()
{
  byte respCode;
  byte thisByte;
  int loopCount = 0;

  while(!client.available()) {
    delay(1);
    loopCount++;

    // if nothing received for 10 seconds, timeout
    if(loopCount > 10000) {
      client.stop();
      Serial.println(F("\r\nTimeout"));
      beep(5,500);
      return 0;
    }
  }

  respCode = client.peek();

  while(client.available())
  {  
    thisByte = client.read();    
    Serial.write(thisByte);
  }

  if(respCode >= '4')
  {
    efail();
    return 0;  
  }

  return 1;
}


void efail()
{
  byte thisByte = 0;
  int loopCount = 0;

  client.println(F("QUIT"));

  while(!client.available()) {
    delay(1);
    loopCount++;

    // if nothing received for 10 seconds, timeout
    if(loopCount > 10000) {
      client.stop();
      Serial.println(F("\r\nTimeout"));
      return;
    }
  }

  while(client.available())
  {  
    thisByte = client.read();    
    Serial.write(thisByte);
  }

  client.stop();

  Serial.println(F("disconnected"));
}


void loop(void){
   ArduinoOTA.handle();
    
    
   if (err) 
   {
   if (blink==0) digitalWrite(LED, HIGH);
    if (blink==1) digitalWrite(LED, LOW);
    blink++;
    if (blink==2) blink=0;
    if (err=NOCARD) delay (100);
    if (err=NOFAT) delay (500);
    
   }
   else 
   {
   if (blink==0) digitalWrite(LED, HIGH);  
   if (blink==4) digitalWrite(LED, LOW);
   blink++;
   if (blink==5) blink=0;
   delay (300);
   }
    
  server.handleClient();

  
  //delay (10); 

}
