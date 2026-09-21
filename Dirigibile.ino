// https://github.com/enricowar/Dirigibile

// Versioni 
//      IDE Arduino           2.3.5
//      Espressif Systems     3.2.1
//      AsyncTCP.h>           3.3.6
//      ESPAsyncWebServer.h>  3.9.3
//      ElegantOTA.h>         3.1.6
//      WebSerial.h>          1.1.0

// tratto da  https://randomnerdtutorials.com/esp32-ota-elegantota-arduino/
// per aggiornare il firmware :
// - compilare con Sketch->Esporta sketch compilato
// - aggiornare firmware con 192.168.1.61/update  prendere il file da arduino/xxxxx/build/esp32xxxxx/xxx.ini.bin
//
// tratto da  https://randomnerdtutorials.com/esp32-webserial-library/
// per seriale con web 192.168.1.61/webserial
//

#define DIAG 1

#include "esp_wifi.h"
#include <Arduino.h>
#include <AsyncTCP.h>             // v.3.3.6
#include <ESPAsyncWebServer.h>    // v.3.9.3
#include <ElegantOTA.h>           // v.3.1.6
#include <WebSerial.h>            // v.1.1.0

// parametri wifi in modalita' manuale
//const char* ssid = "BIBLIOLAB";
//const char* password = "bibliolabwifilocal";
///const int wifiChannel = 11; // di solito e' 6 quindi per non interferire con altri router 2.4 Ghz lo evito
const char* hostName = "Dirigibile";

#include <Preferences.h>
#include <IRremote.hpp> // version 4.4.1

// dati di calibrazione
typedef struct Calib_tag {
  uint32_t        speedMotDXAvanti;     // pwm motore Dx  per movimento avanti
  uint32_t        speedMotSXAvanti;     // pwm motore Sx  per movimento avanti
  uint32_t        speedMotDXIndietro;   // pwm motore Dx  per movimento indietro
  uint32_t        speedMotSXIndietro;   // pwm motore Sx  per movimento indietro

  uint32_t        speedMotSvoltaDX;     // pwm motore Sx  per svolta a DX
  uint32_t        speedMotSvoltaSX;     // pwm motore Dx  per svolta a SX
  bool            calibrated;           // true se contenuto valido 
} Calib;

// parametri letti da flash
Calib calibSetup = {0};

// parametri temporaneamente modificati prima di essere scritti in flash
Calib calibTemp = {0};

Preferences prefs;
bool saveCalib(Calib *cal) 
{
  Serial.println("========== SAVE CALIB ==========");

  Serial.println("Valori INPUT:");
  Serial.println(cal->speedMotDXAvanti);
  Serial.println(cal->speedMotSXAvanti);
  Serial.println(cal->speedMotDXIndietro);
  Serial.println(cal->speedMotSXIndietro);
  Serial.println(cal->speedMotSvoltaDX);
  Serial.println(cal->speedMotSvoltaSX);
  Serial.println(cal->calibrated);

  bool result = prefs.begin("Calibrazione", false);
  Serial.print("prefs.begin = ");
  Serial.println(result);

  if(!result) {
    Serial.println("!!! ERRORE: impossibile aprire NVS");
    prefs.end();
    return false;
  }

  size_t n;

  n = prefs.putUInt("vMotDXAvanti", cal->speedMotDXAvanti);
  Serial.print("put DX avanti = ");
  Serial.println(n);

  n = prefs.putUInt("vMotSXAvanti", cal->speedMotSXAvanti);
  Serial.print("put SX avanti = ");
  Serial.println(n);

  n = prefs.putUInt("vMotDXIndietro", cal->speedMotDXIndietro);
  Serial.print("put DX indietro = ");
  Serial.println(n);

  n = prefs.putUInt("vMotSXIndietro", cal->speedMotSXIndietro);
  Serial.print("put SX indietro = ");
  Serial.println(n);

  n = prefs.putUInt("vMotSvoltaDX", cal->speedMotSvoltaDX);
  Serial.print("put svolta DX = ");
  Serial.println(n);

  n = prefs.putUInt("vMotSvoltaSX", cal->speedMotSvoltaSX);
  Serial.print("put svolta SX = ");
  Serial.println(n);

  n = prefs.putBool("calibrated", true);
  Serial.print("put calibrated = ");
  Serial.println(n);

  Serial.println("Valori RILETTI:");

  Serial.println(prefs.getUInt("vMotDXAvanti", 999));
  Serial.println(prefs.getUInt("vMotSXAvanti", 999));
  Serial.println(prefs.getUInt("vMotDXIndietro", 999));
  Serial.println(prefs.getUInt("vMotSXIndietro", 999));
  Serial.println(prefs.getUInt("vMotSvoltaDX", 999));
  Serial.println(prefs.getUInt("vMotSvoltaSX", 999));
  Serial.println(prefs.getBool("calibrated", false));

  prefs.end();

  Serial.println("========== FINE SAVE ==========");

  return true;
}

bool loadCalib(Calib *cal) 
{
  prefs.begin("Calibrazione", true);  // Read
  cal->calibrated = prefs.getBool("calibrated", false);

  if(!cal->calibrated) {
    prefs.end();
    return false;
  }

  cal->speedMotDXAvanti = prefs.getUInt("vMotDXAvanti", 0);
  cal->speedMotSXAvanti = prefs.getUInt("vMotSXAvanti", 0);

  cal->speedMotDXIndietro = prefs.getUInt("vMotDXIndietro", 0);
  cal->speedMotSXIndietro = prefs.getUInt("vMotSXIndietro", 0);

  cal->speedMotSvoltaDX = prefs.getUInt("vMotSvoltaDX", 0);
  cal->speedMotSvoltaSX = prefs.getUInt("vMotSvoltaSX", 0);
 
  prefs.end();

  return true;
}

void resetCalib() 
{
  prefs.begin("Calibrazione", false);  // Read/Write
  prefs.getUInt("vMotDXAvanti", 0);
  prefs.getUInt("vMotSXAvanti", 0);
  prefs.getUInt("vMotDXIndietro", 0);
  prefs.getUInt("vMotSXIndietro", 0);
  prefs.getUInt("vMotSvoltaDX", 0);
  prefs.getUInt("vMotSvoltaSX", 0);
  prefs.putBool("calibrated", false);
  prefs.end();
  return;
}



int parseInt(char *input) {
  return atoi(input);
}

// codici telecomando
#define NONE      0     // nessun tasto
#define ON_OFF    69    // spegne i motori
#define MENU      71    // non usato
#define TEST      68    // non usato
#define PLUS      64    // avanti 
#define BACK      67    // non usato
#define LEFT      7     // ruota a sinistra
#define PLAY      21    // non usato
#define RIGHT     9     // ruota a destra
#define ZERO      22    // non usato
#define MINUS     25    // indietro
#define CLEAR     13    // non usato
#define ONE       12    // non usato
#define TWO       24    // non usato
#define THREE     94    // non usato
#define FOUR      8     // non usato
#define FIVE      28    // non usato
#define SIX       90    // non usato
#define SEVEN     66    // non usato
#define EIGHT     82    // non usato
#define NINE      74    // non usato


#define IR_PIN GPIO_NUM_5     // sensore per telecomando IR

// osservando da dietro elica SX = M1   elica DX = M2
#define M1A GPIO_NUM_3        
#define M1B GPIO_NUM_2
#define M2A GPIO_NUM_1
#define M2B GPIO_NUM_0

#define FERMO     0
#define AVANTI    1
#define INDIETRO  2
#define SINISTRA  3
#define DESTRA    4

// modo operazioni : navigazione/calibrazione
#define MODE_NAV   0
#define MODE_CAL   1
int mode;

void setup() 
{
  Serial.begin(115200);
  delay(200);

  // dati calibrazione da flash : 
  //    se non ci sono dati salva in falsh i dati standard
  //    per calibrare le velocita' si usa webserial utilizzando il tasto Menu del telecomando
  if(loadCalib(&calibSetup) == false) {
    Serial.println("!!! manca calibrazione");
    calibSetup.speedMotDXAvanti = 100;    
    calibSetup.speedMotSXAvanti = 100;    
    calibSetup.speedMotDXIndietro = 100;    
    calibSetup.speedMotSXIndietro = 100;    
    calibSetup.speedMotSvoltaDX = 50;
    calibSetup.speedMotSvoltaSX = 50;
    memcpy(&calibTemp, &calibSetup, sizeof(calibTemp));
    saveCalib(&calibTemp); 
  }

  IrReceiver.begin(IR_PIN);

  pinMode(M1A, OUTPUT);
  pinMode(M1B, OUTPUT);
  pinMode(M2A, OUTPUT);
  pinMode(M2B, OUTPUT);

  stop();

  mode = MODE_NAV;
  return;

}


char *tbDisplay[6] = {"FERMO", "AVANTI", "INDIETRO", "SINISTRA", "DESTRA", "????"};
int displayMsg;
void display(int msg)
{
  if(msg > DESTRA) msg = 5;
  Serial.println(tbDisplay[msg]);
  return;
}

int vel = 255;   // 0-255   
uint32_t tasto;


void setSpeed(int a, int b, int v) 
{
  if(v > 0) {
    analogWrite(a, v);
    analogWrite(b, 0);
    return;
  } 
  
  if(v < 0) {
    analogWrite(b, -v);
    analogWrite(a, 0);
    return;
  } 

  analogWrite(a, 0);
  analogWrite(b, 0);
  return;
}


void avanti() 
{
  setSpeed(M1A, M1B, -calibSetup.speedMotSXAvanti); // motore SX
  setSpeed(M2A, M2B, calibSetup.speedMotDXAvanti);  // motore DX
  display(AVANTI);
}

void indietro() 
{
  setSpeed(M1A, M1B, calibSetup.speedMotSXAvanti);  // motore SX
  setSpeed(M2A, M2B, -calibSetup.speedMotDXAvanti); // motore DX
  display(INDIETRO);
}

void sinistra() 
{
  setSpeed(M1A, M1B, 0);  // motore SX
  setSpeed(M2A, M2B, calibSetup.speedMotSvoltaDX);   // motore DX
  display(SINISTRA);
}

void destra() 
{
  setSpeed(M1A, M1B, -calibSetup.speedMotSvoltaSX);    // motore SX
  setSpeed(M2A, M2B, 0);   // motore DX
  display(DESTRA);
}

void stop() 
{
  setSpeed(M1A, M1B, 0);
  setSpeed(M2A, M2B, 0);
  display(FERMO);
}

// lettura tasto telecomando
uint32_t readIRCode()
{
  tasto = NONE;
  if(!IrReceiver.decode()) 
    return tasto;  
  
  if(IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT) {
    IrReceiver.resume();
    return tasto;  
  }  

  tasto = IrReceiver.decodedIRData.command;
  Serial.println(tasto);

  IrReceiver.resume();
  return tasto;  
}

// inizializza modalita' calibrazione
AsyncWebServer server(80);

const char* ssid = "BIBLIOLAB";
const char* password = "bibliolabwifilocal";
const int wifiChannel = 11;
IPAddress manualIP(192, 168, 1, 61); 	// per il modo manuale del client 
IPAddress serverIP(192, 168, 1, 220); 
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
unsigned long lastReconnect;
unsigned long msPrev;

// connessione al monitor
bool doConnect(IPAddress &IP, uint32_t timeout)
{
  // Configurazione IP statico (evita DHCP e velocizza connessione)
  if(!WiFi.config(IP, gateway, subnet)) {
    ;
    #ifdef DIAG
    Serial.println("doTx:Errore nella configurazione IP");
    #endif
  }

  // Avvio connessione WiFi
  WiFi.begin(ssid, password);

  #ifdef DIAG
  Serial.print("Connessione a WiFi");
  #endif
  unsigned long t0 = millis();
  while(WiFi.status() != WL_CONNECTED && millis() - t0 < timeout) {
    #ifdef DIAG
    Serial.print(".");
    #endif
    delay(500);
  }
  #ifdef DIAG
  Serial.println();
  #endif

  if(WiFi.status() == WL_CONNECTED) {
    #ifdef DIAG
    Serial.println("Connesso al WiFi!");
    Serial.print("IP locale: ");
    Serial.println(WiFi.localIP());
    #endif
    return true;
  }

  #ifdef DIAG
  Serial.println("Impossibile connettersi al WiFi.");
  #endif
  return false;
}

// Modalita' di calibrazione
void modeCalStart()
{
  Serial.println("Attivata Modalita' Calibrazione");

  // in modalita' manuale arriva quando si preme il tasto menu del telecomando
  // in ogni caso i parametri in flash sono quelli che verranno usati, calibTemp verra' modificato interagento l'operatore.
  memcpy(&calibTemp, &calibSetup, sizeof(calibTemp));

  WiFi.mode(WIFI_STA);

  esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B);

  // potenza trasmissione alta (84 = 20db)
  esp_wifi_set_max_tx_power(84);

  if(!doConnect(manualIP, 5000))
    Serial.println("Impossibile connettersi al WiFi.");

  server.begin();
  Serial.println("HTTP server started");

  WebSerial.begin(&server);
  WebSerial.msgCallback(recvMsg);

  ElegantOTA.begin(&server);  

  lastReconnect = millis();
  msPrev  = millis();

  return;
}

// Modalita' navigazione
void modeCalStop()
{
  Serial.println("Disattivata Modalita' Calibrazione");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  return;
}




#define TICK_MANU     1000    // 1s  
#define TIMEOUT_MANU  3600    // 1h 

unsigned long counter = 0;

// ricezione messaggio webserial
#define MAX_MSG_LEN 50
char msg[51];
void recvMsg(uint8_t *data, size_t len)
{

  if(len == 0)
    return;

  // dimensione del messaggio compatibile
  if(len > MAX_MSG_LEN) {
    WebSerial.print((int)len);
    WebSerial.print(":");
    WebSerial.println("Messaggio troppo lungo, massimo consentito 41 caratteri");
    return;
  }

  //WebSerial.println("Received Data...");
  for(int ii = 0; ii < len; ii++){
    msg[ii] = data[ii];
  }
  msg[len] = '\0';

  // interpretazione dei comandi
  switch(msg[0]) {
    case 'A' : // navigare avanti
      if(msg[1] == 'S') {
        WebSerial.println("Navigare Avanti mot SX");
        calibTemp.speedMotSXAvanti = parseInt((char *)&msg[2]);
      } else 
      if(msg[1] == 'D') {
        WebSerial.println("Navgare Avanti mot DX");
        calibTemp.speedMotDXAvanti = parseInt((char *)&msg[2]);
      }  
    break;

    case 'I' : // navigare indietro
      if(msg[1] == 'S') {
        WebSerial.println("Navigare Indietro mot SX");
        calibTemp.speedMotSXIndietro = parseInt((char *)&msg[2]);
      } else  
      if(msg[1] == 'D') {
        WebSerial.println("Navigare Indietro mot DX");
        calibTemp.speedMotDXIndietro = parseInt((char *)&msg[2]);
      }  
    break;

    case 'C' : // Curvare
      if(msg[1] == 'S') {
        WebSerial.println("Curvare a sinistra mot DX");
        calibTemp.speedMotSvoltaSX = parseInt((char *)&msg[2]);
      } else  
      if(msg[1] == 'D') {
        WebSerial.println("Curvare a destra mot SX");
        calibTemp.speedMotSvoltaDX = parseInt((char *)&msg[2]);
      }  
    break;

    case 'O' : // stampa i parametri da flash
      loadCalib(&calibSetup);
      WebSerial.println("O ********************");
      WebSerial.print("speedMotDXAvanti:"); WebSerial.println(calibSetup.speedMotDXAvanti);    
      WebSerial.print("speedMotSXAvanti:"); WebSerial.println(calibSetup.speedMotSXAvanti);    
      WebSerial.print("speedMotDXIndietro:"); WebSerial.println(calibSetup.speedMotDXIndietro);    
      WebSerial.print("speedMotSXIndietro:"); WebSerial.println(calibSetup.speedMotSXIndietro);    
      WebSerial.print("speedMotSvoltaDX:"); WebSerial.println(calibSetup.speedMotSvoltaDX);
      WebSerial.print("speedMotSvoltaSX:"); WebSerial.println(calibSetup.speedMotSvoltaSX);
      WebSerial.print("speedMotSvoltaSX:"); WebSerial.println(calibSetup.speedMotSvoltaSX);
      WebSerial.print("calibrated:"); WebSerial.println(calibSetup.calibrated);
    break;

    case 'P' : // stampa parametri attuali in ram
      WebSerial.println("P ********************");
      WebSerial.print("speedMotDXAvanti:"); WebSerial.println(calibTemp.speedMotDXAvanti);    
      WebSerial.print("speedMotSXAvanti:"); WebSerial.println(calibTemp.speedMotSXAvanti);    
      WebSerial.print("speedMotDXIndietro:"); WebSerial.println(calibTemp.speedMotDXIndietro);    
      WebSerial.print("speedMotSXIndietro:"); WebSerial.println(calibTemp.speedMotSXIndietro);    
      WebSerial.print("speedMotSvoltaDX:"); WebSerial.println(calibTemp.speedMotSvoltaDX);
      WebSerial.print("speedMotSvoltaSX:"); WebSerial.println(calibTemp.speedMotSvoltaSX);
      WebSerial.print("calibrated:"); WebSerial.println(calibTemp.calibrated);
    break;

    case 'B' : // riavvia esp32
      WebSerial.println("B ********************");
      ESP.restart();
    break;

    case 'E' : // azzera la calibrazione
      WebSerial.println("E ********************");
      resetCalib();
    break;

    case 'S' : // salva i parametri in memoria persistente 
      WebSerial.println("S ********************");
      saveCalib(&calibTemp);
    break;

    default:
      WebSerial.print("Non Interpretabile : "); WebSerial.println(msg);
      WebSerial.println("**** Comandi Consentiti ****");
      WebSerial.println("  ASXX vel motore SX navigazione avanti");
      WebSerial.println("  ADXX vel motore DX navigazione avanti");
      WebSerial.println("  ISXX vel motore SX navigazione indietro");
      WebSerial.println("  CSXX vel motore DX per curvare a SX");
      WebSerial.println("  CDXX vel motore SX per curvare a DX");
      WebSerial.println("  P stampa i parametri RAM");
      WebSerial.println("  O stampa i parametri FLASH");
      WebSerial.println("  S salva i parametri RAM->FLASH");
      WebSerial.println("  E erase: inizializza a 0 dati FLASH");
      WebSerial.println("  B boot: riavvia esp32");
    break;

    }

  return;
}


void loopNAV() 
{
  switch(tasto) {

    case PLUS:      // avanza sollevandosi
      avanti();
    break;

    case MINUS:     // indietreggia abbassandosi 
      indietro();
    break;

    case LEFT:      // spegne motore sinistro 
      sinistra();
    break;

    case RIGHT:     // spegne motore destro
      destra();
    break;

    case ON_OFF:    // ferma i motori : per gravita' perde quota, 
      stop();
    break;
  }

  return;
}

void loopCAL() 
{
  // static unsigned long lastReconnect = 0;
  if(WiFi.status() != WL_CONNECTED && millis() - lastReconnect > 15000) {
    lastReconnect = millis();
    WiFi.reconnect();
  }    

  if(WiFi.status() != WL_CONNECTED && millis() - lastReconnect >= 15000) {
    Serial.println("!!! Timeout connessione WiFi : restart esp32 !!!");
    ESP.restart();
  }    

  if(WiFi.status() == WL_CONNECTED) {
    ElegantOTA.loop();
  }

  // uscita forzata dalla modalita manuale dopo 1h
  if((millis() - msPrev) > TICK_MANU) {
    msPrev = millis();
    counter++;
    if(counter > TIMEOUT_MANU)
      ESP.restart();  
  }

  return;
}


void loop() 
{
  readIRCode();

  // il tasto MENU del telecomando scambia modo NAVIGAZIONE<->CALIBRAZIONE (all'accensione modo NAVIGAZIONE)
  switch(mode) {
    case MODE_NAV:      // modalita' navigazione
      loopNAV();
      if(tasto != MENU)
        break;

      // spegne i motori 
      stop(); 

      // prepara wifi per calibrazione e aggiornamento firmware
      modeCalStart(); 
      delay(100);
      mode = MODE_CAL;
    break;

    case MODE_CAL:      // modalita calibrazione 
      loopCAL();
      if(tasto != MENU)
        break; 
      modeCalStop();
      delay(100);
      mode = MODE_NAV;
    break;
  }

  return;
}

