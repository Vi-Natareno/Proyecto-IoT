#include <WiFi.h>
#include <ThingSpeak.h>
//Wokwi-GUEST
//Buffer circular o de anillo
typedef struct {
  int carros_izquierda;
  int carros_derecha;
  int carros_espera;
} valores;
valores bufferCircular[10];
int head = 0, tail = 0, elementos = 0;

//thingspeak
long unsigned int CHANNEL_ID = 3386034;
const int MESSAGE_DELAY = 20000;
const char* CHANNEL_API_KEY = "C26J1JO3MBCWQYHK";
WiFiClient espClient;
unsigned long int esperaWiFi = 0;

//tiempo envio de datos y wifi
unsigned long int finalTime = 0;

//pines
const int LED_V_IZQ = 32;
const int LED_R_IZQ = 33;
const int LED_V_DER = 13;
const int LED_R_DER = 14;
const int POT = 34;
const int BTN_CARROS_IZQ = 25;
const int BTN_CARROS_DER = 26;

//estado led parpadeo
bool estadoLed = false;
unsigned long tiempoParpadeo = 500;
unsigned long tiempoAnterior = 0;
String ultimo_msj = "";

// Potenciometro
unsigned long ultimoPot = 0;
long int valPot = 0;
int autos_U = 0;
//boton
int clic_count_IZQ = 0;
int clic_count_DER = 0;
int prom_CC_IZQ = 10; // promedio en base a las ultimas 10 tomas de ThingSpeak
int prom_CC_DER = 10; // inicia en 10 por default
bool lastButtonState_IZQ = LOW;
bool lastButtonState_DER = LOW;
bool isDesconnected = true; // para que al iniciar tome datos tambien

// Tiempos millis de semaforo
unsigned long lastTimeMillis = 0;
unsigned long secondMillis = 0;
unsigned long thirdMillis = 0;
unsigned long tiempo_U = 5000;
unsigned long tiempo_verde = 10000;
bool verde_izq;
bool rojo_izq;
bool verde_der;
int num_carros_max = 0;
bool cycle_over = false;

void reconectar();
void enviarPotenciometro();
void leerBoton(int boton, int &clic_count, bool &last_BTN);
void parpadearLed();
int asignarVerdes(int num_carros_max, int carros_en_espera, unsigned long &tiem_U);
void semaforos(unsigned long duracion, unsigned long duracion_U);
void ActualizarBuffer( int carros_izq, int carros_der, int carros_espera);
int enviarDatos();

void setup() {
  Serial.begin(9600);
  pinMode(LED_R_IZQ, OUTPUT);
  pinMode(LED_V_IZQ, OUTPUT);
  pinMode(LED_R_DER, OUTPUT);
  pinMode(LED_V_DER, OUTPUT);
  pinMode(BTN_CARROS_IZQ, INPUT_PULLUP);
  pinMode(BTN_CARROS_DER, INPUT_PULLUP);
  while(millis() < 6000){ // tratamos de conectar por 6 segundos, tiempo promedio en que tarda en conectarse
    reconectar();
  }
}

void loop() {

  //contar los carros que pasan
  leerBoton(BTN_CARROS_IZQ, clic_count_IZQ, lastButtonState_IZQ);
  leerBoton(BTN_CARROS_DER, clic_count_DER, lastButtonState_DER);
  
  //para simular la cuenta de los carros que se quedan a dar vuelta
  enviarPotenciometro();

  if(millis() - finalTime >= 10000){
    semaforos(tiempo_verde, tiempo_U);
  }else{
    lastTimeMillis = millis();
  }

  if (WiFi.status() == WL_CONNECTED){ //si está conectado a internet
    //retoma las cuentas de clics si se fue el internet o directamente al inicio, porque inicia true
    if(isDesconnected == true){
      delay(500);
      ThingSpeak.begin(espClient);
      isDesconnected = false;
    }

    //actualizamos la información rezagada en buffer
      if(elementos > 0){
        ThingSpeak.setField(1, bufferCircular[tail].carros_espera);
        ThingSpeak.setField(3, bufferCircular[tail].carros_izquierda);
        ThingSpeak.setField(4, bufferCircular[tail].carros_derecha);
      }else{
        ThingSpeak.setField(1, autos_U);
        ThingSpeak.setField(3, clic_count_IZQ);
        ThingSpeak.setField(4, clic_count_DER);
      }

    if(cycle_over){ //cuando termina un ciclo vrvp
      cycle_over = false;

      //y seguir asignando tiempos localmente
      num_carros_max = clic_count_DER + clic_count_IZQ;
      tiempo_verde =  asignarVerdes(num_carros_max, autos_U, tiempo_U);

      //enviar datos de monitoreo
      int status = enviarDatos();
      if (status == 200) { //aqui se comprueba, si writeFields regresa 200, es que la operacion se hizo con exito
        //se envia con exito
      }else {
        //si no se pudo enviar, guardamos en buffer, solo si no se pudo
        ActualizarBuffer(clic_count_IZQ, clic_count_DER, autos_U);
    }
      
      clic_count_DER = 0;
      clic_count_IZQ = 0;
      lastTimeMillis = millis();
      }

  }else{ 

    isDesconnected = true;
    
    if(cycle_over){ 
      // seguimos tomando datos cada tanto y asignando valores localmente, pero con frecuencia
      cycle_over = false;
      num_carros_max = clic_count_DER > clic_count_IZQ ? clic_count_DER : clic_count_IZQ;
      tiempo_verde =  asignarVerdes(num_carros_max, autos_U, tiempo_U);
      clic_count_DER = 0;
      clic_count_IZQ = 0;
      lastTimeMillis = millis();
    }  
    reconectar();
  }
} //loop end

void reconectar(){
  if(millis() - esperaWiFi >= 500){
    if(WiFi.status() != WL_CONNECTED){
      //WiFi.begin("HUAWEI-Y9s", "3319995858");
      WiFi.begin("MEGACABLE-2.4G-A2AB", "+n25LCpK_R4()7#Ch%5");
    }else{
      Serial.println("--> Conectado.");
      ThingSpeak.begin(espClient); //importante para poder enviar datos
    }
    esperaWiFi = millis();
  }
}

void leerBoton(int boton, int &clic_count, bool &last_BTN) {
  bool estado = digitalRead(boton);
  if (estado == HIGH && last_BTN == LOW) {
    clic_count++;
    delay(50);
  }
  last_BTN = estado;
}

void enviarPotenciometro() { // para los 2 carriles por falta de material, pero igual tomaría el valor más alto
  if (millis() - ultimoPot >= 2000) { //enviar cada 1 seg
    int valor = analogRead(POT);
    int porcentaje = map(valor,0,4095,0,10);
    autos_U = porcentaje;
    ultimoPot = millis();
    //Serial.println(porcentaje);
  }
}

void parpadearLed(){
    if (millis() - tiempoAnterior >= tiempoParpadeo){
      estadoLed = !estadoLed;
      digitalWrite(LED_V_DER, estadoLed);
      digitalWrite(LED_V_IZQ, estadoLed);
      tiempoAnterior = millis();
    }
}

void semaforos(unsigned long duracion, unsigned long duracion_U){
    if (millis() - lastTimeMillis >= duracion){ //si termina el verde
      if (millis() - secondMillis >= 10000){ //si termina el rojo - 8 para 20000
        if (millis() - thirdMillis >= duracion_U){
          digitalWrite(LED_R_IZQ, LOW);
          digitalWrite(LED_R_DER, LOW);
          cycle_over = true;
        }else{
          parpadearLed(); //parpadea
        }
      }else{
          digitalWrite(LED_V_DER, LOW);
          digitalWrite(LED_V_IZQ, LOW);
          digitalWrite(LED_R_IZQ, HIGH); //enciende el rojo
          digitalWrite(LED_R_DER, HIGH); //enciende el rojo
          thirdMillis = millis();
      }
    }else{
      digitalWrite(LED_V_IZQ, HIGH); //encender verde
      digitalWrite(LED_V_DER, HIGH);
      secondMillis = millis();
    }
}

// ciclo mas corto posible 30+25+10=55s, mas largo 120+25+20= 165
// asignar tiempos cada 2 min (deberían ser más, pero es para que se vea rapido)
int asignarVerdes(int num_carros_max, int carros_en_espera, unsigned long &tiem_U){
  int luz_carril = 0;
  //si no hay muchos carros frecuentes, no se deja tanto tiempo el verde
  if(num_carros_max <= 16){ 
    luz_carril = 10000;   // 10s para 20s
  } // mientras más carros pasen, da prioridad a que circulen dejando mas tiempo el verde
  else if(num_carros_max > 16 && num_carros_max <= 40){ 
    luz_carril = 16000;   //16 para 60s
  }
  else if(num_carros_max > 40){
    luz_carril = 24000; //24 para 120s
  }
  // si muchos carros en espera, se restan 20 segundos al verde normal y la vuelta en U dura mas
  if(carros_en_espera >= 6){
    tiem_U = 10000; //10 para 20
  }else{
    tiem_U = 5000; // 5 para 10 000
  }
  return luz_carril;
}

int enviarDatos(){
  int saturado = (clic_count_DER+clic_count_IZQ) > 90? 1 : 0; 
  ThingSpeak.setField(2, num_carros_max);
  ThingSpeak.setField(7, saturado);
  ThingSpeak.setField(6, float(tiempo_U));
  ThingSpeak.setField(5, float(tiempo_verde)); 
  return ThingSpeak.writeFields(CHANNEL_ID, CHANNEL_API_KEY);
}

void ActualizarBuffer( int carros_izq, int carros_der, int carros_espera) {
  bufferCircular[head] = { carros_izq, carros_der, carros_espera };
  head = (head + 1) % 10; //adelantamos un lugar, dividimos entre 10 para regresar al inicio 0 cuando se llene
  if ( elementos < 10) { //si no esta lleno
    elementos++; //aumentamos la cuenta de elementos
  } else {
    //si esta lleno movemos tail para sobreescribir el valor más viejo
    tail = (tail + 1) % 10;
  }
}

