#include <nRF24L01.h>
#include <RF24.h>  //https://maniacbug.github.io/RF24/classRF24.html
#include <RF24_config.h>
#include <SPI.h>
#include <printf.h>
#include <I2Cdev.h>
#include <MPU6050.h>
#include <Wire.h>
#include <INA226.h>

//----------------------------------Conexiones------------------------------------------
//Salidas para motores
const int LM1_PWM = 13;   //Motor propulsion 1 direccion 1 
const int RM1_PWM = 12;  //Motor propulsion 1 direccion 2
const int LM2_PWM = 15;   //Motor propulsion 2 direccion 1
const int RM2_PWM = 14;   //Motor propulsion 2 direccion 2
const int LM3_PWM = 3;   //Motor recoleccion direccion 1
//        RM3_PWM-->GND
const int R_IS1=34;      //Sensor de corriente 
const int L_IS1=35;      //Sensor de corriente
const int R_IS2=36;      //Sensor de corriente
const int L_IS2=37;      //Sensor de corriente
const int L_IS3=38;      //Sensor de corriente


//Salidas para camara
const int CAM= 11;        //Alimentacion de la camara
//Salidas de modulo RF 
const int pinCE = 2;     //pin CE en modulo NRF24L01
const int pinCSN = 4;    //pin CSN en modulo NRF24L01
//Salidas Leds
const int LedVerde = 1; 
const int LedAzul = 0;

//----------------------------------Variables globales------------------------------------
//Variables de control
char estadoact = 'P';    //Estado de los motores de propulsion
char estadoant = 'P';    //Estado de los motores de propulsion en el ciclo pasado del microcontrolador
int velocidadM1 = 0;     //Velocidad en PWM enviada a motor 1
int velocidadM2 = 0;     //Velocidad en PWM enviada a motor 2
int velocidadM3 = 0;     //Velocidad en PWM enviada a motor 3 (recoleccion)
int velocidadMax = 250;  //Velocidad maxima a la que debe llegar el motor
int dv = 10;             //Paso de cambio de velocidad de los motores de propulsion por ciclo de operacion del microcontrolador 
int dv2 = 5;             //Paso de cambio de velocidad del motor de recoleccion por ciclo de operacion del microcontrolador 
int dtemp = 50;             //Delay en cada ciclo 
bool CAMOn = false;      //Estado de la camara On/Off
bool MTROn = false;      //Estado del motor de recolección On/Off
const int VMaxRec=150;   //Velocidad del motor de recolección

int Cur_LM1=0;            //Corriente Motor 1
int Cur_RM1=0;
int Cur_LM2=0;            //Corriente Motor 2
int Cur_RM2=0;
int Cur_LM3=0;            //Corriente Motor recoleccion

//Declaracion de objetos
RF24 radio(pinCE, pinCSN);
INA226 ina;
MPU6050 mpu;

const uint64_t pipe = 0xE8E8F0F0E1LL; //Direccion "virtual" para comunicación RF (debe ser igual en emisor y receptor)
//Direcciones I2C:
//     - MPU6050 ---> 0x68
//     - INA226  ---> 0x40

 
//Varaibles de comunicacion RF
char dataTx[60]; //datos enviados
char dataRx[8]; //datos recividos
int NRFCon = 0; //NRF24L01 Conected - Variable de conexion de modulo RF
int NoCmd=0; //No Command - Variable contadora de ciclos en los que no se recivio un comando de la pc
const int MaxNoCmd=50; //Numero de ciclos sin comonado antes de parar el USV
bool DSent=false; //Data Sent - Varaible de confirmación de envio de datos

// Valores sin procesar del MPU6050
int ax, ay, az;
int gx, gy, gz;
int temp;
//Valores de cálculo y procesados del MPU6050
long tiempo_prev;
float dt;
float ang_x, ang_y;
float ang_x_prev, ang_y_prev;
float tempFinal;
int MPUCon=0; //MPU Conected - Variable de conexion de MPU

// Valores sin procesar del INA226
float BusVoltage;
float BusPower;
float ShuntVolt;
float ShuntCurr;
float Battery=0;   //Valor de carga de la bateria porcentual

void setup() {
  pinMode(LM1_PWM, OUTPUT);
  pinMode(RM1_PWM, OUTPUT);
  pinMode(LM2_PWM, OUTPUT);
  pinMode(RM2_PWM, OUTPUT);
  pinMode(LedVerde, OUTPUT);
  pinMode(LedAzul, OUTPUT);
  
  Serial.begin(19200);
  Wire.begin();   //Iniciando I2C 
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

//-------------Configuracion Modulo RF---------------------------------------------------
  printf_begin();
  radio.begin(); //Inicia el objeto de modulo RF
  radio.setAutoAck(true); //Enciende el protocolo de confirmación de lectura de NRF24L01
  radio.setDataRate(RF24_250KBPS); //Velocidad de trasmisión RF
  radio.setChannel(02); //Canal de comunicación RF (determina frecuencia exacta en el rango de 2.400GHz a 2.525GHz). Puede ser de 0 a 124
  radio.setPALevel(RF24_PA_MIN); //Potencia de modulo RF
  radio.setCRCLength( RF24_CRC_8); //Protolo CRC 

  radio.printDetails();
  NRFCon= radio.isChipConnected(); //1 si modulo RF esta conectado 0 en caso contrario
  Serial.println(NRFCon);
  
  radio.openWritingPipe(pipe);   //Abre linea virtual para enviar datos RF 
  radio.openReadingPipe(1,pipe);  //Abre linea virtaul para recibir datos RF
  radio.startListening(); //Pone modulo RF en estado de receptor

//---------------------Configuracion de MPU------------------------------------------
  mpu.initialize();    //Iniciando el sensor MPU
  MPUCon=mpu.testConnection();

//----------------------Configuracion de sensor INA-----------------------------------
  ina.begin();  //Inicialización del sensor INA
  ina.configure(INA226_AVERAGES_16, INA226_BUS_CONV_TIME_1100US, INA226_SHUNT_CONV_TIME_1100US, INA226_MODE_BUS_CONT);
  ina.calibrate(0.1, 4);

//---------------------Configuracion de los leds---------------------------------------
  digitalWrite(LedVerde, HIGH);
  digitalWrite(LedAzul, HIGH);
  
  
}



void loop() {

  //--------------------------------USV en modo RX-----------------------------
  radio.startListening();   //Pone al NRF24L01 en modo de receptor 
  //while(!radio.available()){}
  if (radio.available()){cmdDecode();}
  else 
  {
    if (NoCmd<MaxNoCmd){NoCmd++;} //Si no se recibe un mensaje RF en un ciclo de operación del microcontrolador el contador NoCmd incrementa
    else
    {
      NoCmd=MaxNoCmd;    //Si el contador NoCmd llega al valor MaxNoCmd el USV se para, significa error de comunicación
      estadoact='P';
    }
  }
  //:::::::::::::::::::::Comando desde Serial:::::::::::::::::::::::::::::::::::::::::::::
  /*if (Serial.available() > 0){
    dataRx[0] = Serial.read(); 
    if(dataRx[0]=='V')
    {
      int i=1;
      while(Serial.available() && i<8)
      {
        dataRx[i]=Serial.read();
        i++;
      }
      velocidadMax=atoi(&dataRx[1]);
    }
    else {estadoact=dataRx[0];}
  }*/
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

  //-------------------Control de motores y lectura de sensores---------------
  controlMotores();
  
  if(CAMOn){
    digitalWrite(CAM,HIGH);
    digitalWrite(LedAzul,HIGH);
  } //Prende o apaga la camara
  else{
    digitalWrite(CAM,LOW);  
    digitalWrite(LedAzul,LOW);
    }

  if(MTROn){giroM3(VMaxRec);}
  else{giroM3(0);}
  
  leer_MPU6050();
  leer_INA226();
  Cur_LM1=analogRead(L_IS1);
  Cur_RM1=analogRead(R_IS1);
  Cur_LM2=analogRead(L_IS2);
  Cur_RM2=analogRead(R_IS2);
  Cur_LM3=analogRead(L_IS3);
  
  //:::::::::::::::::::::Mostrar estado en monitor serie:::::::::::::::::::::::::
  
  Serial.print("Est: ");
  Serial.print(estadoact);
  Serial.print("  VMax= ");
  Serial.print(velocidadMax);
  Serial.print("  VM1= ");
  Serial.print(velocidadM1); 
  Serial.print("  VM2= ");
  Serial.print(velocidadM2);
  Serial.print("  VM3= ");
  Serial.print(velocidadM3);
  Serial.print("  B_REC= ");
  Serial.print(MTROn);
  Serial.print("  CAM= ");
  Serial.print(CAMOn);
  Serial.print("  dataRx: ");
  Serial.print(dataRx);
  Serial.print("  NoCmd: ");
  Serial.print(NoCmd);
  Serial.print("  IncX: ");
  Serial.print(ang_x);
  Serial.print("  IncY: ");
  Serial.print(ang_y);
  Serial.print("  T: ");
  Serial.print(tempFinal);
  Serial.print("  V Bat: ");
  Serial.println(Battery);
  //delay(dtemp);
  //::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
  
  
  //-------------------------------USV en modo TX-------------------------------
  radio.stopListening(); //Pone al NRF24L01 en modo de emisor
  String str=String(Battery)+","+String(tempFinal)+","+String(tempFinal)+","+String(ang_x)+","+String(ang_y);//+","+String(Cur_LM1)+","+String(Cur_LM2)+","+String(velocidadM1)+","+String(velocidadM2);
  //String str=String(Cur_LM1)+","+String(Cur_LM2)+","+String(velocidadM1)+","+String(velocidadM2)+","+String(Cur_LM3)+","+String(velocidadM3);
  str.toCharArray(dataTx,60);
  //Serial.println(dataTx);
  DSent= radio.write(dataTx, sizeof(dataTx)); //Envia los datos y si la comunicacion el exitosa DSent=true 
}


//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
//::::::::::::::::::::::::::::::::::::::::::Funciones::::::::::::::::::::::::::::::::::::::::::::::::::::
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

//--------------------------------------Comunicacion RF-------------------------------------

void cmdDecode(){
  radio.read(dataRx, sizeof(dataRx));    //Si el NRF24L01 recibio un mensaje lo lee
  Serial.println(dataRx);
  if(dataRx[0]=='V')
  {
    velocidadMax=atoi(&dataRx[1]);
  }
  else if (dataRx[0]=='C')               //El comando Cx prede y apaga la camara
  {
    if (dataRx[1]=='1'){CAMOn=true;}
    else if(dataRx[1]=='0'){CAMOn=false;}
  }
  else if (dataRx[0]=='B'){MTROn=!MTROn;}
  else if (dataRx[0]!='N'){estadoact=dataRx[0];}   //Si el comando es N (No Command) no actualiza el estado  de control, de lo contario el estado es igual al primer caracter del mensaje recibido.
  else {NoCmd++;}
  NoCmd=0; // Al obtner un comando, aunque sea N, el contador NoCmd se resetea
}

//-----------------------------------------Motores-------------------------------------------
void controlMotores(){
  switch (estadoact){
    case 'A':
      giroM1(velocidadMax);
      giroM2(velocidadMax);
      break;
    case 'I':
      giroM1(velocidadMax);
      giroM2(0);
      break;
    case 'D':
      giroM1(0);
      giroM2(velocidadMax);
      break;
    case 'R':
      giroM1(-velocidadMax);
      giroM2(-velocidadMax);
      break;
    case 'P':
      giroM1(0);
      giroM2(0);
      break;
    default:
      estadoact= 'P';    
    }
}


void giroM1(int velocidadMAX){
   if (velocidadM1<velocidadMAX-dv) {velocidadM1=velocidadM1+dv;}
   else if (velocidadM1>velocidadMAX+dv) {velocidadM1=velocidadM1-dv;}
   else {velocidadM1=velocidadMAX;}

   if(velocidadM1>=0)
   {
    analogWrite(LM1_PWM,velocidadM1);
    analogWrite(RM1_PWM,0);
   }
   else
   {
    analogWrite(RM1_PWM,-velocidadM1);
    analogWrite(LM1_PWM,0);
   }
}

void giroM2(int velocidadMAX){
   if (velocidadM2<velocidadMAX-dv) {velocidadM2=velocidadM2+dv;}
   else if (velocidadM2>velocidadMAX+dv) {velocidadM2=velocidadM2-dv;}
   else {velocidadM2=velocidadMAX;}

   if(velocidadM2>=0)
   {
    analogWrite(LM2_PWM,velocidadM2);
    analogWrite(RM2_PWM,0);
   }
   else
   {
    analogWrite(RM2_PWM,-velocidadM2);
    analogWrite(LM2_PWM,0);
   }
}


void giroM3(int velocidadMAX){
   if (velocidadM3<velocidadMAX-dv2) {velocidadM3=velocidadM3+dv2;}
   else if (velocidadM3>velocidadMAX+dv2) {velocidadM3=velocidadM3-dv2;}
   else {velocidadM3=velocidadMAX;}
   
   if(velocidadM3>=0)
   {
    analogWrite(LM3_PWM,velocidadM3);
   }
}


//-----------------------------Sensores---------------------------------------------
void leer_MPU6050(){
   // Leer las aceleraciones y velocidades angulares 
  mpu.getAcceleration(&ax, &ay, &az);
  mpu.getRotation(&gx, &gy, &gz);
  temp = mpu.getTemperature();
  
  dt = (millis()-tiempo_prev)/1000.0;
  tiempo_prev=millis();
  
  //Calcular los ángulos con acelerometro
  float accel_ang_x=atan(ay/sqrt(pow(ax,2) + pow(az,2)))*(180.0/3.14);
  float accel_ang_y=atan(-ax/sqrt(pow(ay,2) + pow(az,2)))*(180.0/3.14);
  
  //Calcular angulo de rotación con giroscopio y filtro complemento  
  ang_x = 0.98*(ang_x_prev+(gx/131)*dt) + 0.02*accel_ang_x;
  ang_y = 0.98*(ang_y_prev+(gy/131)*dt) + 0.02*accel_ang_y;
  ang_x_prev=ang_x;
  ang_y_prev=ang_y;
  ang_x = ang_x-25;
  ang_y = ang_y-7;
  tempFinal = (float)temp/340+36.53;
}

void leer_INA226(){
  const float BatVMax=25.3;    //Voltaje de bateria cargada al 100%
  const float BatVMin=22.2;    //Voltaje de bateria cargada al 0%
  BusVoltage = ina.readBusVoltage();
  BusPower = ina.readBusPower();
  ShuntVolt = ina.readShuntVoltage();
  ShuntCurr = ina.readShuntCurrent();
  Battery = BusVoltage;
  //Battery=(BusVoltage-BatVMin)/(BatVMax-BatVMin)*100;
  //if (Battery>100){Battery=100;}
  //else if(Battery<0){Battery=0;}
}
