#include <nRF24L01.h>
#include <RF24.h>
#include <RF24_config.h>
#include <SPI.h>
#include <printf.h>

const int pinCE = 9;
const int pinCSN = 10;
RF24 radio(pinCE, pinCSN);

// Single radio pipe address for the 2 nodes to communicate.
const uint64_t pipe = 0xE8E8F0F0E1LL;

char dataTx[8];
char dataRx[60];

void setup(void)
{
  Serial.begin(19200);
  printf_begin();
  radio.begin();
  radio.setAutoAck(true);
  radio.setDataRate(RF24_250KBPS);
  radio.setRetries(15,15);
  radio.setChannel(02);
  radio.setPALevel(RF24_PA_MIN);
  radio.setCRCLength( RF24_CRC_8);

  radio.printDetails();
  int con= radio.isChipConnected() ;
  Serial.println(con);
   
  radio.openWritingPipe(pipe);
  radio.openReadingPipe(1,pipe);
  
}

void loop(void)
{
//----------------------------TX part------------------------------
  radio.stopListening();
  if (Serial.available())
  {
    //String str=Serial.readString();
    //str.toCharArray(dataTx,8);
    int i=0;
    while (Serial.available() && i<8)
    {
      dataTx[i]=Serial.read();
      i++;
    }
    while(i<7)
    {
      dataTx[i]=' ';
      i++;
    }
  }
  else
  {
    String str2="NC";
    str2.toCharArray(dataTx,8);
  }
  
  bool ok= radio.write(dataTx, sizeof(dataTx));
  /*if(ok)
  {
    Serial.print("Cmd sent ok: ");
    Serial.println(dataTx);
   }
  else {Serial.println("CMD NOT SENT!!!!... Fuck!");}*/
//-----------------------------RX part-----------------------------
  
  radio.startListening();
  bool timeout=false;
  long t=millis();
  while(!radio.available() && !timeout)
  {
    if( (millis()-t)>200){timeout=true;}
  }
  if (!timeout)
  {
    radio.read(dataRx, sizeof(dataRx));
    //Serial.print("Responce:");
    Serial.println(dataRx);
  }
  else
  {
    //Serial.println("No responce!!!!.... Fuck!");
  }
  
  //Serial.println();
  //delay(200);
}
