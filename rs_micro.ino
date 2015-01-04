
//
//  Angara.Net dht22-bmp085-gprs sensor
//
//  DHT-22, BMP085, Arduino Pro Micro, SIM900
//  http://github.com/maxp/rs_micro
//

// Libraries:
//  http://playground.arduino.cc/Main/WireLibraryDetailedReference
//  http://playground.arduino.cc/Main/DHTLib
//  https://github.com/adafruit/Adafruit-BMP085-Library
//
//  http://www.tinyosshop.com/index.php?route=product%2Fproduct&product_id=464


char VERSION[] = "rs_micro v0.3";

#define HOST      "rs.angara.net"
#define PORT      "80"
#define BASE_URI  "/dat?"

#define INTERVAL    300
// seconds


#define DHT_PIN 4

// !!! redefined for "mamai" hardware
#define DHT_PIN 6

#define SIM_POWER 5

// Connect VCC of the BMP085 sensor to 3.3V (NOT 5.0V!)  (one red light diode)
// Connect GND to Ground
// Connect SCL to i2c clock - on '168/'328 Arduino Uno/Duemilanove/etc thats Analog 5
// Connect SDA to i2c data - on '168/'328 Arduino Uno/Duemilanove/etc thats Analog 4
// EOC is not used, it signifies an end of conversion
// XCLR is a reset pin, also not used here


// cable: (Olha-2)
// A4 - blue   (bmp sda)
// A5 - green  (bmp scl)
// D6 - grey   (dht data)
// V3 - red/white
// V5 - red
// GD - blue/green/grey-white

#define TCP_TIMEOUT 10
#define SEND_RETRY  3

#define APN   ""
#define USER  ""
#define PASS  ""

#define CSTT     "AT+CSTT=\""APN"\",\""USER"\",\""PASS"\""
#define CIPSTART "AT+CIPSTART=\"TCP\",\""HOST"\","PORT

#define IMEI_LEN 20
#define RBUFF_LEN 80
#define UBUFF_LEN 220

char imei[IMEI_LEN];

char rbuff[RBUFF_LEN];
char ubuff[UBUFF_LEN];


#include <Wire.h>
#include <dht.h>
#include <Adafruit_BMP085.h>

#define GsmPort Serial1

int cycle;


void setup() {
  Serial.begin(9600);
  Serial.println(VERSION);
  GsmPort.begin(9600);
  cycle = 0;
  imei[0] = 0;
}


void sim_power() {
  pinMode(SIM_POWER, OUTPUT);
  digitalWrite(SIM_POWER, LOW);
  delay(1000);
  digitalWrite(SIM_POWER, HIGH);
  delay(2000);
  digitalWrite(SIM_POWER, LOW);
  delay(5000);
}


void gsm_flush() 
{
    Serial.print("drop: ");
    delay(100);
    while(GsmPort.available()){ 
        char c = GsmPort.read();
        Serial.print(c);
        delay(1);    
    }
    Serial.println();
}

void gsm_send(char* s) 
{
    GsmPort.print(s);
}

void gsm_cmd(char* s) 
{
    Serial.print("send: "); Serial.print(s); Serial.println();  
    gsm_send(s); gsm_send("\r");
}


int gsm_recv(int timeout_sec) 
{
    Serial.print("recv: ");
    int i=0;
    for(; i < RBUFF_LEN; i++ ) {
        char c = 0;
        for(int t=0; t < timeout_sec*1000; t++ ) {
            if(GsmPort.available()) { 
                c = GsmPort.read(); 
                Serial.print(c);
                break; 
            }
            else { delay(1); }
        }
        if(c == 0 || c == '\n') { break; } 
        rbuff[i] = c;     
    }
    if(i > 0 && rbuff[i-1] == '\r') { i--; }
    rbuff[i] = 0;
    Serial.println();
    return i;
}


int gsm_cmd_ok(char* s, int timeout)
{
    gsm_flush(); gsm_cmd(s);
    int n = gsm_recv(timeout);
    if(!n) { n = gsm_recv(timeout); } // skip empty line
    if(rbuff[0] == 'O' && rbuff[1] == 'K' && rbuff[2] == 0) { return 1; }
    else { return 0; }
}

int check_pwr() 
{
    // if(gsm_cmd_ok("AT", 3)) { return 1; }
    sim_power();
    gsm_cmd_ok("ATE0", 1);
    if(gsm_cmd_ok("AT", 3)) { 
      Serial.println("gsm on.");
      return 1; 
    }
    Serial.println("gsm off.");
    return 0;
}

int read_dht() {
  dht d;
  char c[20];

  if(d.read22(DHT_PIN) == DHTLIB_OK) {
    dtostrf(d.humidity, 3, 1, c);
    strcat(ubuff, "&h="); strcat(ubuff, c);
    dtostrf(d.temperature, 3, 1, c);
    strcat(ubuff, "&t="); strcat(ubuff, c);
    return 0;
  }
  return -1;
}

int read_bmp() {
    Adafruit_BMP085 bmp;  
    char c[20];
    
    if( !bmp.begin() ) {
      return -1;  
    }
  
    dtostrf(bmp.readTemperature(), 3, 1, c);
    strcat(ubuff, "&t1="); strcat(ubuff, c);
    dtostrf(bmp.readPressure()/100., 3, 1, c);
    strcat(ubuff, "&p="); strcat(ubuff, c);
    return 0;
}


void loop() {
  char c[20];
  strcpy(ubuff, BASE_URI);
  sprintf(c, "cycle=%d", ++cycle);
  strcat(ubuff, c);
  
  read_dht();
  read_bmp();
  
  Serial.print("data: "); Serial.println(ubuff);

  imei[0] = 0;
  
  if( check_pwr() ) {

        for( int retry=0; retry < SEND_RETRY; retry++ )
        {
            if(!imei[0]) {
              gsm_cmd("AT+GSN");
              gsm_recv(1); gsm_recv(1);
              Serial.print("imei:"); Serial.println(rbuff);
              strncpy(imei, rbuff, IMEI_LEN);
              if(imei[0]) { 
                strcat(ubuff,"&hwid="); strcat(ubuff, imei); 
              }
            }
          
            // gsm_cmd_ok("at+cipshut", 3);

            delay(10000);    // gsm network delay
            
            gsm_cmd_ok(CSTT, 3); 
            gsm_cmd_ok("at+ciicr", 3);
            gsm_cmd_ok("at+cifsr", 3);
            gsm_cmd_ok(CIPSTART, 3);
            gsm_recv(TCP_TIMEOUT);
        
            if( strcmp(rbuff, "CONNECT OK") ) { Serial.println("connected"); }

            gsm_cmd_ok("at+cipsend", 3);
        
            Serial.print("ubuff: "); Serial.println(ubuff);
            
            gsm_send("GET "); gsm_send(BASE_URI); gsm_send(ubuff); gsm_send(" HTTP/1.0\r\n");
            gsm_send("Host: "); gsm_send(HOST); gsm_send("\r\n\r\n");
            gsm_send("\x1a"); // Ctrl-Z
        
            gsm_recv(TCP_TIMEOUT);
            gsm_recv(TCP_TIMEOUT);
            Serial.print("resp: "); Serial.println(rbuff);
            
            boolean send_ok = (strcmp(rbuff, "SEND OK") == 0);
            gsm_cmd_ok("at+cipshut", 3);
            
            if(send_ok) { 
              Serial.println("send_ok."); 
              break; 
            }
        }        
        sim_power();
    }
  
    Serial.print("\nsleep "); Serial.print(INTERVAL); Serial.println(" sec.");
    for(int i=0; i < INTERVAL; i++ ) { delay(1000); }
    Serial.println();
}

//.

