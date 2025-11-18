#include <EEPROM.h>

// --- Configuración de Pines ---
const int motorIN1 = 8;
const int motorIN2 = 9;
const int motorENA = 10;
const int fcAbiertoPin = 2;
const int fcCerradoPin = 3;
const int pulsadorAbrirPin = 4;
const int pulsadorCerrarPin = 5;
const int ledAbrirPin = 11;
const int ledCerrarPin = 12;

// --- Configuración de Seguridad y Debounce ---
const unsigned long TIMEOUT_MOVIMIENTO = 15000; // 15 segundos
const int DEBOUNCE_DELAY = 50; // 50 milisegundos para filtrar rebotes

// --- Estados del Sistema ---
enum EstadoPuerta {
  ABIERTA,
  CERRADA,
  ABRIENDO,
  CERRANDO,
  PARADA_ERROR,
  PARADA_MANUAL
};
EstadoPuerta estadoActual;

// --- Lógica de la EEPROM ---
int direccionEeprom = 0;

// --- Variables de Temporización ---
unsigned long tiempoAnteriorParpadeo = 0;
const long intervaloParpadeo = 250;
const long intervaloParpadeoError = 100;
bool estadoLedParpadeo = false;
unsigned long tiempoInicioMovimiento = 0;

// --- Variables para Detección de Flanco (Solución al problema) ---
int ultimoEstadoPulsadorAbrir = HIGH;
int ultimoEstadoPulsadorCerrar = HIGH;

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
  
  bool fcAbiertoPulsado = (digitalRead(fcAbiertoPin) == LOW);
  bool fcCerradoPulsado = (digitalRead(fcCerradoPin) == LOW);
  EstadoPuerta estadoAlArrancar;

  if (fcCerradoPulsado) {
    estadoAlArrancar = CERRADA;
    EEPROM.update(direccionEeprom, 0);
  } else if (fcAbiertoPulsado) {
    estadoAlArrancar = ABIERTA;
    EEPROM.update(direccionEeprom, 1);
  } else {
    byte estadoGuardado = EEPROM.read(direccionEeprom);
    estadoAlArrancar = (estadoGuardado == 1) ? ABIERTA : CERRADA;
  }

  if (estadoAlArrancar == CERRADA) {
    iniciarMovimiento(ABRIENDO);
  } else {
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

// --- ¡NUEVA FUNCIÓN DE CONTROL MANUAL ROBUSTA! ---
void gestionarControlesManuales() {
  int lecturaPulsadorAbrir = digitalRead(pulsadorAbrirPin);
  int lecturaPulsadorCerrar = digitalRead(pulsadorCerrarPin);

  // --- Lógica para el botón ABRIR ---
  // Comprobar si el botón acaba de ser pulsado (transición de HIGH a LOW)
  if (lecturaPulsadorAbrir == LOW && ultimoEstadoPulsadorAbrir == HIGH) {
    delay(DEBOUNCE_DELAY); // Esperar para filtrar el rebote
    
    // Si la puerta se está moviendo, la paramos.
    if (estadoActual == ABRIENDO || estadoActual == CERRANDO) {
      Serial.println("Movimiento INTERRUMPIDO por botón Abrir.");
      estadoActual = PARADA_MANUAL;
    } 
    // Si la puerta está parada, la abrimos.
    else if (estadoActual != ABIERTA) {
      Serial.println("Pulsador ABRIR: iniciando movimiento.");
      iniciarMovimiento(ABRIENDO);
    }
  }

  // --- Lógica para el botón CERRAR ---
  // Comprobar si el botón acaba de ser pulsado (transición de HIGH a LOW)
  if (lecturaPulsadorCerrar == LOW && ultimoEstadoPulsadorCerrar == HIGH) {
    delay(DEBOUNCE_DELAY); // Esperar para filtrar el rebote
    
    // Si la puerta se está moviendo, la paramos.
    if (estadoActual == ABRIENDO || estadoActual == CERRANDO) {
      Serial.println("Movimiento INTERRUMPIDO por botón Cerrar.");
      estadoActual = PARADA_MANUAL;
    } 
    // Si la puerta está parada, la cerramos.
    else if (estadoActual != CERRADA) {
      Serial.println("Pulsador CERRAR: iniciando movimiento.");
      iniciarMovimiento(CERRANDO);
    }
  }

  // Actualizar el último estado de los botones para el siguiente ciclo.
  ultimoEstadoPulsadorAbrir = lecturaPulsadorAbrir;
  ultimoEstadoPulsadorCerrar = lecturaPulsadorCerrar;
}


void gestionarMovimientoPuerta() {
  switch (estadoActual) {
    case ABRIENDO:
      if (digitalRead(fcAbiertoPin) == LOW) {
        detenerMotor();
        estadoActual = ABIERTA;
        EEPROM.update(direccionEeprom, 1);
      } else if (millis() - tiempoInicioMovimiento > TIMEOUT_MOVIMIENTO) {
        detenerMotor();
        estadoActual = PARADA_ERROR;
      } else {
        abrirPuerta();
      }
      break;
    case CERRANDO:
      if (digitalRead(fcCerradoPin) == LOW) {
        detenerMotor();
        estadoActual = CERRADA;
        EEPROM.update(direccionEeprom, 0);
      } else if (millis() - tiempoInicioMovimiento > TIMEOUT_MOVIMIENTO) {
        detenerMotor();
        estadoActual = PARADA_ERROR;
      } else {
        cerrarPuerta();
      }
      break;
    default: // Incluye todos los estados de parada
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
      digitalWrite(ledAbrirPin, estadoLedParpadeo);
      digitalWrite(ledCerrarPin, estadoLedParpadeo);
      break;
    case PARADA_MANUAL:
      digitalWrite(ledAbrirPin, estadoLedParpadeo);
      digitalWrite(ledCerrarPin, !estadoLedParpadeo);
      break;
  }
}

void iniciarMovimiento(EstadoPuerta nuevoEstado) {
  estadoActual = nuevoEstado;
  tiempoInicioMovimiento = millis();
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