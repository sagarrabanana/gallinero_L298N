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

// --- Configuración de Seguridad ---
const unsigned long TIMEOUT_MOVIMIENTO = 15000; // 15 segundos

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

// --- ¡FUNCIÓN CORREGIDA Y SIMPLIFICADA! ---
void gestionarControlesManuales() {
  bool botonAbrirPulsado = (digitalRead(pulsadorAbrirPin) == LOW);
  bool botonCerrarPulsado = (digitalRead(pulsadorCerrarPin) == LOW);

  // Lógica de Interrupción: se activa si un botón se pulsa DURANTE el movimiento.
  if ((estadoActual == ABRIENDO || estadoActual == CERRANDO) && (botonAbrirPulsado || botonCerrarPulsado)) {
    Serial.println("Movimiento INTERRUMPIDO manualmente.");
    
    // 1. Detener el motor INMEDIATAMENTE.
    detenerMotor(); 
    
    // 2. Cambiar el estado del sistema.
    estadoActual = PARADA_MANUAL;
    
    // 3. Pausar para evitar lecturas dobles (debounce).
    // Esta pausa es crucial y soluciona el problema.
    delay(500); 
    
    return; // Salir de la función para asegurar que el nuevo estado se procesa limpiamente en el siguiente ciclo.
  }

  // Lógica de Arranque: se activa si un botón se pulsa MIENTRAS la puerta está parada.
  if (estadoActual == ABIERTA || estadoActual == CERRADA || estadoActual == PARADA_ERROR || estadoActual == PARADA_MANUAL) {
    if (botonAbrirPulsado && estadoActual != ABIERTA) {
      iniciarMovimiento(ABRIENDO);
    } else if (botonCerrarPulsado && estadoActual != CERRADA) {
      iniciarMovimiento(CERRANDO);
    }
  }
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
    case PARADA_MANUAL:
    case PARADA_ERROR:
    case ABIERTA:
    case CERRADA:
      // En cualquier estado de parada, nos aseguramos de que el motor esté detenido.
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
}```