#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

// Pines de entrada
#define DHTPIN 2
#define DHTTYPE DHT11
const byte PIN_LDR = A0;

// Pines de salida
const byte LED_VERDE = 12;
const byte LED_ROJO = 13;
const byte BUZZER = 9;

// Umbrales ajustados con las pruebas reales
const float TEMP_ADVERTENCIA = 30.0;
const float TEMP_ALARMA = 30.0;
const float HUM_ADVERTENCIA = 75.0;
const float HUM_ALARMA = 80.0;

// Pruebas observadas:
// luz normal ≈ 543 a 555  -> MONITOREO
// advertencia sugerida >= 620
// alarma sugerida >= 700
const int LUZ_ADVERTENCIA = 620;
const int LUZ_ALARMA = 700;

// Objetos de los dispositivos que ya funcionaron en el prototipo
DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);

enum EstadoSistema {
  INICIAL,
  MONITOREO,
  ADVERTENCIA,
  ALARMA
};

EstadoSistema estado = INICIAL;
float temperatura = 0.0;
float humedad = 0.0;
int luminosidad = 0;
bool errorDHT = false;
bool vistaPrincipal = true;
unsigned long ultimaLectura = 0;
unsigned long ultimoParpadeo = 0;
bool rojoEncendido = false;

const unsigned long INTERVALO_LECTURA = 2000;
const unsigned long INTERVALO_PARPADEO = 400;

const char* nombreEstado(EstadoSistema e) {
  switch (e) {
    case INICIAL: return "INICIAL";
    case MONITOREO: return "MONITOREO";
    case ADVERTENCIA: return "ADVERTENCIA";
    case ALARMA: return "ALARMA";
  }
  return "DESCONOCIDO";
}

void limpiarLinea(byte fila) {
  lcd.setCursor(0, fila);
  lcd.print("                ");
  lcd.setCursor(0, fila);
}

void evaluarEstado() {
  // Un error de lectura se trata como alarma para hacerlo visible
  if (errorDHT) {
    estado = ALARMA;
  } else if (temperatura >= TEMP_ALARMA ||
             humedad >= HUM_ALARMA ||
             luminosidad >= LUZ_ALARMA) {
    estado = ALARMA;
  } else if (temperatura >= TEMP_ADVERTENCIA ||
             humedad >= HUM_ADVERTENCIA ||
             luminosidad >= LUZ_ADVERTENCIA) {
    estado = ADVERTENCIA;
  } else {
    estado = MONITOREO;
  }
}

void controlarAlarmas() {
  if (estado == MONITOREO) {
    digitalWrite(LED_VERDE, HIGH);
    digitalWrite(LED_ROJO, LOW);
    noTone(BUZZER);

  } else if (estado == ADVERTENCIA) {
    digitalWrite(LED_VERDE, LOW);
    noTone(BUZZER);

    // El LED rojo parpadea en Advertencia
    if (millis() - ultimoParpadeo >= INTERVALO_PARPADEO) {
      ultimoParpadeo = millis();
      rojoEncendido = !rojoEncendido;
      digitalWrite(LED_ROJO, rojoEncendido);
    }

  } else if (estado == ALARMA) {
    digitalWrite(LED_VERDE, LOW);
    digitalWrite(LED_ROJO, HIGH);
    tone(BUZZER, 2000);

  } else {
    digitalWrite(LED_VERDE, LOW);
    digitalWrite(LED_ROJO, LOW);
    noTone(BUZZER);
  }
}

void actualizarLCD() {
  if (errorDHT) {
    limpiarLinea(0);
    lcd.print("Error DHT11");
    limpiarLinea(1);
    lcd.print("Revise conexion");
    return;
  }

  // Se alternan dos vistas porque la pantalla tiene dos filas
  if (vistaPrincipal) {
    limpiarLinea(0);
    lcd.print("Temp:");
    lcd.print(temperatura, 1);
    lcd.print((char)223);
    lcd.print("C");

    limpiarLinea(1);
    lcd.print("Hum:");
    lcd.print(humedad, 0);
    lcd.print("%");
  } else {
    limpiarLinea(0);
    lcd.print("Luz:");
    lcd.print(luminosidad);

    limpiarLinea(1);
    lcd.print("Est:");
    lcd.print(nombreEstado(estado));
  }

  vistaPrincipal = !vistaPrincipal;
}

void enviarUART() {
  if (errorDHT) {
    Serial.print("Temperatura: ERROR | Humedad: ERROR");
  } else {
    Serial.print("Temperatura: ");
    Serial.print(temperatura, 1);
    Serial.print(" C | Humedad: ");
    Serial.print(humedad, 0);
    Serial.print(" %");
  }

  Serial.print(" | Luminosidad: ");
  Serial.print(luminosidad);
  Serial.print(" | Estado: ");
  Serial.println(nombreEstado(estado));
}

void leerSensores() {
  humedad = dht.readHumidity();
  temperatura = dht.readTemperature();
  luminosidad = analogRead(PIN_LDR);
  errorDHT = isnan(humedad) || isnan(temperatura);
}

void setup() {
  // Configuración de salidas en estado seguro
  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_ROJO, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  digitalWrite(LED_VERDE, LOW);
  digitalWrite(LED_ROJO, LOW);
  noTone(BUZZER);

  // Inicialización de UART, DHT11 y LCD I2C
  Serial.begin(9600);
  dht.begin();
  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("Iniciando...");
  lcd.setCursor(0, 1);
  lcd.print("Estado INICIAL");

  Serial.println("Sistema iniciado");
  delay(2000);
  lcd.clear();
}

void loop() {
  // Lectura y actualización cada dos segundos
  if (millis() - ultimaLectura >= INTERVALO_LECTURA) {
    ultimaLectura = millis();
    leerSensores();
    evaluarEstado();
    actualizarLCD();
    enviarUART();
  }

  // Se atienden las salidas continuamente para permitir el parpadeo
  controlarAlarmas();
}
