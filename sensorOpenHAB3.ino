/**
 *  
 *  Comunicación con openHAB mediante REST
 *  para reportar el estado de sensores
 *
 *  Creado: Febrero 2018
 *          Mayo 2018   Versión con mac address
 *                      Empleada pàra pruebas
 *                      Transicion entre WiFiMulti y simplemente WiFi
 *                      Ya solo con WiFi
 *          Diciembre 2018
 *                      Intento hacerle mas robusto
 *                      Compilación condicional segun DEBUG
 *                      Intenta recuperarse si la WiFi falla
 *                      Actualiza cada 15 minutos
 *          
 *
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <StreamString.h>
#include <DHT.h>

#include "wifi.h"               // Este fichero define el nombre de la SSID, la palabra de acceso 
                                // y la dirección del servidor REST de openHAB

#define USE_SERIAL Serial
#define DHTPIN      4
#define DHTTYPE     DHT22       // DHT 22  (AM2302), AM2321

#define INTERVAL_TRY     2000   // Intervalo entre muestras para dar tiempo al sensor = 2 segundos
#define MAX_TRIES           3   // Máximo número de reintentos de lectura de un dato del sensor
#define KEEP_ALIVE      60000   // Intervalo tras el cual mostraré que estoy vivo mediante un pulso en el LED

#define SHORT_WAIT    1200      // Periodo de espera entre intentos de conexión en el bucle interno
#define LONG_WAIT     5000      // Periodo de espera entre intentos de conexión en el bucle externo

#define BUCLE_EXTERNO   4       // Cuantas veces voy a intentar conectarme a la WiFi en el bucle externo
#define BUCLE_INTERNO   6       // Cuantas veces voy a intentar conectarme a la WiFi en el bucle interno

#define DEBUG 1

DHT dht(DHTPIN, DHTTYPE);

/*
 * const char *URLTemp       = "http://192.168.1.6:8080/rest/items/DespachoTemp/state";
*/
const char *strError1     = "==**== Aborting ==**== ";
const char *strErrorHTTP1 = "[HTTP] PUT... failed, error: %s\n";
const char *strHTTP1      = "[HTTP] PUT... return code: %d\n";

/*
 * 
 * En el fichero .h debe estar definida la variabla URL
 * 
 * const char *URL     = "http://openHAB_Server:port:8080/rest/items/";
 * 
 */

const char *sensor1 = "Temp";
const char *sensor2 = "Humedad";
const char *sensor3 = "HIC";
const char *state   = "/state";
const char *confInterval = "PollIntervalSensors";

unsigned long INTERVALO = 900000;      // 900 segundos = 15 minutos
//#define INTERVALO 10000       // A emplear durante las pruebas

char URLTemp[80];      
char URLHumedad[80];   
char URLHIC[80];       

#define MAX_SENSOR_NAME 20

char sensorName[MAX_SENSOR_NAME] = "";
char macAddressREST[14];

long lastMsg   = 0;
long lastBlink = 0;
char msg[10];

int intento = 0;
int nLoop = 0;

byte macAddress[6];
IPAddress ip, gw, mask;

boolean connected = false;

/*
// Define where debug output will be printed.
#define DEBUG_PRINTER Serial

// Setup debug printing macros.
#ifdef DHT_DEBUG
  #define DEBUG_PRINT(...) { DEBUG_PRINTER.print(__VA_ARGS__); }
  #define DEBUG_PRINTLN(...) { DEBUG_PRINTER.println(__VA_ARGS__); }
#else
  #define DEBUG_PRINT(...) {}
  #define DEBUG_PRINTLN(...) {}
#endif
 * 
 */
/*
typedef enum {
    WL_NO_SHIELD        = 255,   // for compatibility with WiFi Shield library
    WL_IDLE_STATUS      = 0,
    WL_NO_SSID_AVAIL    = 1,
    WL_SCAN_COMPLETED   = 2,
    WL_CONNECTED        = 3,
    WL_CONNECT_FAILED   = 4,
    WL_CONNECTION_LOST  = 5,
    WL_DISCONNECTED     = 6
} wl_status_t;
*/


void DebugPrint(char *txtMensaje)
{
#ifdef DEBUG
    USE_SERIAL.print(txtMensaje);
#endif
   
}
/**
 *  Función para hacer un blink de un determinado LED conectado a un puerto
 *
 *  La función es bloqueante hasta que termine su ejecución.
 *
 */
void blink(int veces, int pin, int tiempoON, int tiempoOFF) {

    for(uint8_t n = 0;n < veces; ++n) {
        digitalWrite(pin, LOW);
        delay(tiempoON);
        digitalWrite(pin, HIGH);
        delay(tiempoOFF);
    }
}

/**
 * 
 * Crea la petición URL para acceder a un objeto via REST.
 * 
 * URL es una variable global que contiene la dirección del servidor openHAB
 * state es una vatiable global que define como se acceder al estado de un item
 * item es una cadena con el nombre del objeto.
 * 
 * NOTA: request debe permitir unos 80 caracteres para formar la petición.
 * 
 */
void getURLState(char *request, char *item){
    strcpy(request, URL);
    strcat(request, item);
    strcat(request, state);   
}

/**
 * 
 * Intenta obtener el intervalo al que hará polling 
 * de los sensores.
 * Se utiliza confInterval como nombre del item.
 * 
 * Deja el resultado en la variable global sensorName
 * 
 */
void getIntervalValue(void) {
    HTTPClient http;
    char URLrequest[80];
    char temporal[10];
    int httpCode;
    unsigned long numero;

    getURLState(URLrequest, (char *) confInterval);
    
//    strcpy(URLrequest, URL);
//    strcat(URLrequest, confInterval);
//    strcat(URLrequest, state);

    // URLrequest contiene la petición REST formada

    DebugPrint("Actualizando el valor de INTERVAL\n");
    DebugPrint(URLrequest);
    DebugPrint("\n");
    
    http.begin(URLrequest); 
    httpCode = http.GET();                                                                  //Send the request
 
    if (httpCode > 0) { //Check the returning code
        String payload = http.getString();   //Get the request response payload
        payload.toCharArray(temporal, 10);
        numero = atol(temporal);
        Serial.printf("La cadena es %s y el valor %ld\n",temporal, numero);
        if (numero > 0) INTERVALO = numero;
    }
    http.end();
#ifdef DEBUG
    USE_SERIAL.printf("Interval value %ld\n", INTERVALO);
#endif
    
}


/**
 * 
 * Intenta obtener el nombre del sensor del servidor OpenHAB
 * empleando REST API. Se utiliza la dirección MAC como nombre
 * del objeto.
 * 
 * Deja el resultado en la variable global sensorName
 * 
 */
boolean getSensorName(void) {
    HTTPClient http;
    char URLrequest[80];
    int httpCode;

//    strcpy(URLrequest, URL);
//    strcat(URLrequest, macAddressREST);
//    strcat(URLrequest, state);

    getURLState(URLrequest, macAddressREST);
    
    // URLrequest contiene la petición REST formada

    DebugPrint(URLrequest);
    
    http.begin(URLrequest); 
    httpCode = http.GET();                                                                  //Send the request
 
    if (httpCode > 0) { //Check the returning code
        String payload = http.getString();   //Get the request response payload
        payload.toCharArray(sensorName, MAX_SENSOR_NAME);
    }
    http.end();  
    if (strcmp(sensorName, "NULL") == 0) {
        sensorName[0] = '\0';
        return false;
    }
    else return true;
}

boolean sendSensorValue(char *URLToSend, char *msg) {
    HTTPClient  http;
    int         httpCode;
    
    http.begin(URLToSend); //HTTP

#ifdef DEBUG    
    USE_SERIAL.print("[HTTP] PUT: Value: ");
    USE_SERIAL.println(msg);
#endif

    // start connection and send HTTP header
    
    httpCode = http.sendRequest("PUT", msg);

#ifdef DEBUG
    USE_SERIAL.print("URL a usar ");
    USE_SERIAL.println(URLToSend);  
#endif

    // httpCode will be negative on error
    if(httpCode > 0) {
        // HTTP header has been sent and Server response header has been handled
#ifdef DEBUG        
        USE_SERIAL.printf(strHTTP1, httpCode);
#endif        

        // file found at server
        if(httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
#ifdef DEBUG
            USE_SERIAL.println(payload);
#endif            
        }
    } else {
#ifdef DEBUG        
        USE_SERIAL.printf(strErrorHTTP1, http.errorToString(httpCode).c_str());
#endif        
    }

    http.end();
}

/**
 * Configura la conexión WiFi
 * 
 */
void setupWifi() {
    uint8_t status;

    WiFi.mode(WIFI_STA);

    WiFi.begin(mySSID, mySSID_PASSWORD);
  
    connected = false;

    for(uint8_t bucleExterno = BUCLE_EXTERNO; bucleExterno > 0; bucleExterno--) {
        for(uint8_t t = BUCLE_INTERNO; t > 0; t--) {
#ifdef DEBUG            
            USE_SERIAL.printf("[SETUP %d] Esperando Contador = %d...", bucleExterno, t);
            USE_SERIAL.flush();
#endif            
            delay(SHORT_WAIT);
            status = WiFi.status();
            if(status == WL_CONNECTED) {
                connected = true;
                t = 1;
#ifdef DEBUG                
                USE_SERIAL.printf("Conectado Status = %d isConnected = %d!!\n", status, WiFi.isConnected());
#endif                
            }
            else {
#ifdef DEBUG                
                USE_SERIAL.printf("Todavía no. Status = %d\n", status);
#endif                
            }
            USE_SERIAL.flush();
        }
        if (connected) {
            bucleExterno = 1;
        }
        else {
            delay(LONG_WAIT);  
        }  
    }
    if (connected) {
    // 
    // Muestro la información básica de la conexión conseguida
    // Cambiar el código por una compilación condicional para ahorrar
    // memoria una vez que esté correctamente depurado
    //
        ip   = WiFi.localIP();
        mask = WiFi.subnetMask();
        gw   = WiFi.gatewayIP();
    
    // Comprobar funcionamiento
    
        WiFi.macAddress(macAddress);
        macAddressREST[0] = 'M';
        sprintf(&macAddressREST[1], "%02x",macAddress[0]);
        sprintf(&macAddressREST[3],  "%02x",macAddress[1]);
        sprintf(&macAddressREST[5],  "%02x",macAddress[2]);
        sprintf(&macAddressREST[7],  "%02x",macAddress[3]);
        sprintf(&macAddressREST[9],  "%02x",macAddress[4]);
        sprintf(&macAddressREST[11], "%02x",macAddress[5]);
#ifdef DEBUG
        USE_SERIAL.printf("MAC Address: %s\n", macAddressREST);
        USE_SERIAL.printf("IP Address: %s\n", ip.toString().c_str());
        USE_SERIAL.printf("Mask:       %s\n", mask.toString().c_str());
        USE_SERIAL.printf("Gateway     %s\n", gw.toString().c_str());
#endif

        if (!getSensorName()) {
            connected = false;
            digitalWrite(D4, LOW);
#ifdef DEBUG            
            USE_SERIAL.printf("No he conseguido el nombre del sensor\n");
#endif            
        }
        else {
            strcpy(URLTemp, URL);
            
            strcat(URLTemp, sensorName);           
            strcpy(URLHumedad, URLTemp);
            strcpy(URLHIC, URLTemp);

            strcat(URLTemp, sensor1);
            strcat(URLHumedad, sensor2);
            strcat(URLHIC, sensor3);

            strcat(URLTemp, state);
            strcat(URLHumedad, state);
            strcat(URLHIC, state);

#ifdef DEBUG            
            USE_SERIAL.printf("Sensor name %s\n", sensorName);         
            USE_SERIAL.printf("URL 1 %s\n", URLTemp);
            USE_SERIAL.printf("URL 1 %s\n", URLHumedad);
            USE_SERIAL.printf("URL 1 %s\n", URLHIC);
#endif

            getIntervalValue();
        }
    }
    else {
      //
      // Propongo dejar el LED encendido si no me he conectado como señal al exterior de que algo no va bien
      //
      digitalWrite(D4, LOW);
    }
    WiFi.printDiag(Serial);
}

void setup() {
    uint8_t status;
  
//
// Inicializa el led
// Y le hace guiñar varias veces como aviso
// del comienzo de su trabajo
//
    pinMode(D4, OUTPUT);
    blink(3,D4,300,150);
    
// Inicializa el sensor DHT
    dht.begin();
  
// Inicializa el puerto serie  
    USE_SERIAL.begin(115200);

//
// Esta función se podrá activar para mostrar mensajes 
// adicionales o no durante el funcionamiento del código
//  
// USE_SERIAL.setDebugOutput(true);

    USE_SERIAL.println();
    USE_SERIAL.println();
    USE_SERIAL.println();

    setupWifi();

    lastMsg = -INTERVALO;
}

void dumpData(float h, float t, float hic) {
#ifdef DEBUG
    USE_SERIAL.println("Y los datos son ...");
    USE_SERIAL.print("Humidity: ");
    USE_SERIAL.print(h);
    USE_SERIAL.print(" %\t");
    USE_SERIAL.print("Temperature: ");
    USE_SERIAL.print(t);
    USE_SERIAL.print(" *C \t");
    USE_SERIAL.print("Heat index: ");
    USE_SERIAL.print(hic);
    USE_SERIAL.println(" *C ");
#endif
}

void tryConnectAgain() {
    setupWifi();
}

void loop() {
    long        now;    // Momento actual
    int         tries;  // Número de intentos 
    float       h;      // Humedad
    float       t;      // Temperatura
    float       hic;    // Heat Index
    HTTPClient  http;
    int         httpCode;
    
    now = millis();

/*
 *  Comprueba al heartbeat
 * 
 */ 
    if (now - lastBlink > KEEP_ALIVE) {
        lastBlink = now;
        blink(1, D4, 500, 200);
    }

/*    
 *  Comprueba el pulso de funcionamiento
 *  
 */  
    if (now - lastMsg < INTERVALO) return;

    if (!connected) {
        tryConnectAgain();
        if (!connected) {
            switch(intento) {
                case 0: DebugPrint("Esperando al segundo intento\n");
                        delay(15000);
                        intento = 1;
                        break;
                case 1: DebugPrint("Esperando al tercer intento\n");
                        delay(30000);
                        intento = 2;
                        break;
                default: intento = 0;
                         DebugPrint("Ooops. Anulando este intento\n");
                         lastMsg = now;
                         break;
            }
            return;
        }
        else {
            DebugPrint("Superado el fallo de conexión\n");
            intento = 0;
        }
    }

    //
    // Toca trabajar para recoger los datos pues el periodo ha expirado
    //
    // Si la conexión WiFi no funciona, regreso y espero al siguiente
    // pulso de loop(), pues no actualizo el valor de lastMsg.
    // Es decir, en el siguiente bucle, la condición será cierta y llegará
    // hasta aqui el código, para reintentar la conexión WiFi
    // 
    // Probablemente sea necesario poner el valor de connected a 0 pues no estoy conectado
    //
    // Puede merecer la pena si la condicion se cumple, señalizarlo con algun led para que quede claro que ha habido un problema
    //
    if((WiFi.status() != WL_CONNECTED)) {
#ifdef DEBUG
        USE_SERIAL.printf("No estoy conectado por algun motivo %d\n",WiFi.status());
        delay(500);
        intento = 0;
        connected = 0;
#endif        
        return;    
    }

    lastMsg = now;
    
#ifdef DEBUG    
    USE_SERIAL.println("Publicando toma de datos");
#endif    
    
    // Como el sensor no es muy rápido, tenemos que dejar tiempo entre las muestras
    // O corresmos el riesgo de no obtener lectura. Hay una constante definida al principio
    // indicando el periodo mínimo de espera, qué está alrededor de los 2 segundos

    // Lectura de la humedad primero
    
    tries = 0;
    while (tries < MAX_TRIES)
    {
        h = dht.readHumidity();
        if (isnan(h) ) {
#ifdef DEBUG            
            snprintf (msg, 75, "Loop # %d", tries);
            USE_SERIAL.print("Failed to read from DHT sensor! Humidity: ");
            USE_SERIAL.println(msg);
#endif            
            delay(INTERVAL_TRY);
        }
        else break;
        ++tries;
    }

    if (tries == MAX_TRIES) {
#ifdef DEBUG        
        snprintf (msg, 75, "== HUMIDITY == Run out of tries");
        USE_SERIAL.println(msg);
        USE_SERIAL.println(strError1);
#endif        
        return;
    }
     
    delay(INTERVAL_TRY);

    // Lectura de la temperatura
    // Read temperature as Celsius (the default)
    tries = 0;
    while (tries < MAX_TRIES)
    {
        t = dht.readTemperature();
        if (isnan(t) ) {
#ifdef DEBUG            
            snprintf (msg, 75, "Loop %d", tries);
            USE_SERIAL.print("Failed to read from DHT sensor! Temperature: ");
            USE_SERIAL.println(msg);
#endif            
            delay(INTERVAL_TRY);
        }
        else break;
        ++tries;
    }

    if (tries == MAX_TRIES) {
#ifdef DEBUG        
        snprintf (msg, 75, "== TEMPERATURE == Run out of tries");
        USE_SERIAL.println(msg);
        USE_SERIAL.println(strError1);
#endif        
        return;
    }

    // Compute heat index in Celsius (isFahreheit = false)
    hic = dht.computeHeatIndex(t, h, false);

    dumpData(h,t,hic);
    
//
// Ahora con la Temperatura
//

#ifdef DEBUG
    USE_SERIAL.print("[HTTP] begin Temperature...\n");
#endif    

    dtostrf(t, 5, 2, msg);
    sendSensorValue(URLTemp, msg);

//
// Ahora con la humedad
//

#ifdef DEBUG
    USE_SERIAL.print("[HTTP] begin Humidity...\n");
#endif    

    dtostrf(h, 5, 2, msg);
    sendSensorValue(URLHumedad, msg);

//
// Ahora con Heat Index
//

#ifdef DEBUG
    USE_SERIAL.print("[HTTP] begin Heat Index...\n");
#endif    

    dtostrf(hic, 5, 2, msg);
    sendSensorValue(URLHIC, msg);
   
    // Señalizo que he enviado los mensajes

    blink(3, D4, 400, 200);

    ++nLoop;
    if (nLoop == 4) {
        getIntervalValue();
        nLoop = 0;
    }
}


