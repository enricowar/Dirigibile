#include <IRremote.hpp> // version 4.4.1

// codici telecomando
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


#define IR_PIN 4

#define M1A 3
#define M1B 2
#define M2A 1
#define M2B 0

#define FERMO     0
#define AVANTI    1
#define INDIETRO  2
#define SINISTRA  3
#define DESTRA    4

char *tbDisplay[6] = {"FERMO", "AVANTI", "INDIETRO", "SINISTRA", "DESTRA", "????"};
int displayMsg;
void display(int msg)
{
  if(msg > DESTRA) msg = 5;
  Serial.println(tbDisplay[msg]);
  return;
}

int vel = 130;   // 0-255   (da regolare quando lo proviamo!)
uint32_t tasto;


void setSpeed(int a, int b, int v) 
{
  if(v > 0) {
    analogWrite(a, v);
    digitalWrite(b, LOW);
    return;
  } 
  
  if(v < 0) {
    digitalWrite(a, LOW);
    analogWrite(b, -v);
    return;
  } 

  digitalWrite(a, LOW);
  digitalWrite(b, LOW);
  return;
}


void avanti() 
{
  setSpeed(M1A, M1B, vel);
  setSpeed(M2A, M2B, vel);
  display(AVANTI);
}

void indietro() 
{
  setSpeed(M1A, M1B, -vel);
  setSpeed(M2A, M2B, -vel);
  display(INDIETRO);
}

void sinistra() 
{
  setSpeed(M1A, M1B, 0);
  setSpeed(M2A, M2B, vel);
  display(SINISTRA);
}

void destra() 
{
  setSpeed(M1A, M1B, vel);
  setSpeed(M2A, M2B, 0);
  display(DESTRA);
}

void stop() 
{
  setSpeed(M1A, M1B, 0);
  setSpeed(M2A, M2B, 0);
  display(FERMO);
}

void setup() 
{
  Serial.begin(115200);

  IrReceiver.begin(IR_PIN);

  pinMode(M1A, OUTPUT);
  pinMode(M1B, OUTPUT);
  pinMode(M2A, OUTPUT);
  pinMode(M2B, OUTPUT);

  stop();
}

uint32_t lastCode = 123456;
void loop() 
{
  if(!IrReceiver.decode()) 
    return;  
  
  if(IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT) {
    IrReceiver.resume();
    return;  
  }  

  tasto = IrReceiver.decodedIRData.command;

  Serial.println(tasto);

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

  IrReceiver.resume();

  return;
}

