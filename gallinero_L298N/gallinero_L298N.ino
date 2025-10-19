#include <EEPROM.h>

// --- Configuración de Pines ---
// Motor (Driver L298N)
const int motorIN1 = 8;
const int motorIN2 = 9;
const int motorENA = 10;

// Finales de Carrera (Limit Switches)
const int fcAbiertoPin = 2;
const int fcCerradoPin = 3;

// Pulsadores Manuales
const int pulsadorAbrirPin = 4;
const int pulsadorCerrarPin = 5;

// LEDs de Estado
const int ledAbrirPin = 11;  // Verde
const int ledCerrarPin = 12; // Rojo

// --- Configuración de Seguridad ---
// ¡¡AJUSTAR ESTE VALOR!! Mide cuánto tarda la puerta y añade un 20-30% de margen.
const unsigned long TIMEOUT_MOVIMIENTO = 15000; // 15 segundos en milisegundos

// --- Estados del Sistema ---
enum EstadoPuerta {
  ABIERTA,
  CERRADA,
  ABRIENDO,
  CERRANDO,
  PARADA_ERROR // Estado por timeout o fallo
};
EstadoPuerta estadoActual;

// --- Lógica de la EEPROM ---
int direccionEeprom = 0;

// --- Variables de Temporización ---
unsigned long tiempoAnteriorParpadeo = 0;
const long intervaloParpadeo = 250;     // Parpadeo normal
const long intervaloParpadeoError = 100; // Parpadeo rápido para error
bool estadoLedParpadeo = false;

// Variable para controlar el timeout del motor
unsigned long tiempoInicioMovimiento = 0;

//******************************************************************************
// SETUP
//******************************************************************************
void setup() {
  Serial.begin(9600);

  pinMode(motorIN1, OUTPUT);
  pinMode(motorIN2, OUTPUT);
  pinMode(motorENA, OUTPUT);

  pinMode(ledAbrirPin, OUTPUT);
  pinMode(ledCerrarPin, OUTPUT);

  pinMode(fcAbiertoPin, INPUT_PULLUP);
  pinMode(fcCerradoPin, INPUT_PULLUP);
  pinMode(pulsadorAbrirPin, INPUT_PULLUP);
  pinMode(pulsadorCerrarPin, INPUT_PULLUP);

  detenerMotor();
  
  // --- Lógica de Arranque: Determinar el estado inicial ---
  bool fcAbiertoPulsado = (digitalRead(fcAbiertoPin) == LOW);
  bool fcCerradoPulsado = (digitalRead(fcCerradoPin) == LOW);
  
  EstadoPuerta estadoAlArrancar;

  if (fcCerradoPulsado) {
    Serial.println("Arranque: Final de carrera CERRADO detectado.");
    estadoAlArrancar = CERRADA;
    EEPROM.update(direccionEeprom, 0);
  } else if (fcAbiertoPulsado) {
    Serial.println("Arranque: Final de carrera ABIERTO detectado.");
    estadoAlArrancar = ABIERTA;
    EEPROM.update(direccionEeprom, 1);
  } else {
    Serial.println("Arranque: Ningún final de carrera detectado. Usando EEPROM.");
    byte estadoGuardado = EEPROM.read(direccionEeprom);
    estadoAlArrancar = (estadoGuardado == 1) ? ABIERTA : CERRADA;
  }

  // --- Decidir la acción a realizar ---
  if (estadoAlArrancar == CERRADA) {
    Serial.println("Acción de arranque: ABRIR.");
    iniciarMovimiento(ABRIENDO);
  } else {
    Serial.println("Acción de arranque: CERRAR.");
    iniciarMovimiento(CERRANDO);
  }
}

//******************************************************************************
// LOOP
//******************************************************************************
void loop() {
  gestionarControlesManuales();
  gestionarMovimientoPuerta();
  gestionarLeds();
}

//******************************************************************************
// GESTIÓN DE FUNCIONES
//******************************************************************************

void gestionarControlesManuales() {
  // Permitir control manual si la puerta está parada (incluso en estado de error)
  if (estadoActual == ABIERTA || estadoActual == CERRADA || estadoActual == PARADA_ERROR) {
    if (digitalRead(pulsadorAbrirPin) == LOW && estadoActual != ABIERTA) {
      Serial.println("Pulsador ABRIR presionado.");
      iniciarMovimiento(ABRIENDO);
    } else if (digitalRead(pulsadorCerrarPin) == LOW && estadoActual != CERRADA) {
      Serial.println("Pulsador CERRAR presionado.");
      iniciarMovimiento(CERRANDO);
    }
  }
}

void gestionarMovimientoPuerta() {
  switch (estadoActual) {
    case ABRIENDO:
      if (digitalRead(fcAbiertoPin) == LOW) {
        Serial.println("Destino alcanzado: ABIERTA");
        detenerMotor();
        estadoActual = ABIERTA;
        EEPROM.update(direccionEeprom, 1);
      } else if (millis() - tiempoInicioMovimiento > TIMEOUT_MOVIMIENTO) {
        Serial.println("¡ERROR: Timeout durante la apertura! Motor detenido por seguridad.");
        detenerMotor();
        estadoActual = PARADA_ERROR;
      } else {
        abrirPuerta();
      }
      break;

    case CERRANDO:
      if (digitalRead(fcCerradoPin) == LOW) {
        Serial.println("Destino alcanzado: CERRADA");
        detenerMotor();
        estadoActual = CERRADA;
        EEPROM.update(direccionEeprom, 0);
      } else if (millis() - tiempoInicioMovimiento > TIMEOUT_MOVIMIENTO) {
        Serial.println("¡ERROR: Timeout durante el cierre! Motor detenido por seguridad.");
        detenerMotor();
        estadoActual = PARADA_ERROR;
      } else {
        cerrarPuerta();
      }
      break;

    default: // Incluye ABIERTA, CERRADA, PARADA_ERROR
      detenerMotor();
      break;
  }
}

void gestionarLeds() {
  unsigned long tiempoActual = millis();
  long intervaloActual = (estadoActual == PARADA_ERROR) ? intervaloParpadeoError : intervaloParpadeo;

  if (tiempoActual - tiempoAnteriorParpadeo >= intervaloActual) {
    tiempoAnteriorParpadeo = tiempoActual;
    estadoLedParpadeo = !estadoLedParpadeo;
  }

  switch (estadoActual) {
    case ABRIENDO:
      digitalWrite(ledAbrirPin, estadoLedParpadeo);
      digitalWrite(ledCerrarPin, LOW);
      break;
    case CERRANDO:
      digitalWrite(ledAbrirPin, LOW);
      digitalWrite(ledCerrarPin, estadoLedParpadeo);
      break;
    case ABIERTA:
      digitalWrite(ledAbrirPin, HIGH);
      digitalWrite(ledCerrarPin, LOW);
      break;
    case CERRADA:
      digitalWrite(ledAbrirPin, LOW);
      digitalWrite(ledCerrarPin, HIGH);
      break;
    case PARADA_ERROR:
      // Ambos LEDs parpadean rápido para indicar error
      digitalWrite(ledAbrirPin, estadoLedParpadeo);
      digitalWrite(ledCerrarPin, estadoLedParpadeo);
      break;
  }
}

// --- Funciones de Control del Motor y Estado ---
void iniciarMovimiento(EstadoPuerta nuevoEstado) {
  estadoActual = nuevoEstado;
  tiempoInicioMovimiento = millis(); // Reinicia el temporizador de seguridad
}

void abrirPuerta() {
  digitalWrite(motorIN1, HIGH);
  digitalWrite(motorIN2, LOW);
  digitalWrite(motorENA, HIGH);
}

void cerrarPuerta() {
  digitalWrite(motorIN1, LOW);
  digitalWrite(motorIN2, HIGH);
  digitalWrite(motorENA, HIGH);
}

void detenerMotor() {
  digitalWrite(motorIN1, LOW);
  digitalWrite(motorIN2, LOW);
  digitalWrite(motorENA, LOW);
}