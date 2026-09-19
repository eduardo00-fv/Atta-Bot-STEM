// Proyecto Atta-Bot STEM
// Calibración manual de los límites del servo de la herramienta.
// Señal del servo: GPIO 26 (myServo, igual que CodigoESP32.ino).
//
// Abra el Monitor Serial a 115200 baudios y seleccione "Nueva línea".
// El programa inicia en 90. Pruebe un paso a la vez hasta encontrar los
// valores superior e inferior que la herramienta puede aceptar sin forzarla.
//
// Comandos:
//   r              reinicia en 90
//   +              aumenta 5 grados desde el valor actual
//   -              disminuye 5 grados desde el valor actual
//   ?              muestra esta ayuda

#include <ESP32Servo.h>

const int pinServoHerramienta = 26;
const int valorInicial = 90;
const int valorMinimo = 0;
const int valorMaximo = 180;
const int pasoCalibracion = 5;

Servo servoHerramienta;
int valorActual = valorInicial;

void moverServo(int nuevoValor) {
  valorActual = constrain(nuevoValor, valorMinimo, valorMaximo);
  servoHerramienta.write(valorActual);
  Serial.print("Valor actual: ");
  Serial.println(valorActual);
}

void imprimirAyuda() {
  Serial.println();
  Serial.println("Comandos: r (reiniciar a 90), + (subir 5), - (bajar 5), ?");
}

void procesarComando(String comando) {
  comando.trim();
  comando.toLowerCase();
  if (comando.length() == 0) return;

  if (comando == "r") {
    moverServo(valorInicial);
    return;
  }
  if (comando == "?" || comando == "ayuda") {
    imprimirAyuda();
    return;
  }

  if (comando == "+") {
    moverServo(valorActual + pasoCalibracion);
    return;
  }
  if (comando == "-") {
    moverServo(valorActual - pasoCalibracion);
    return;
  }

  Serial.println("Comando no reconocido.");
  imprimirAyuda();
}

void setup() {
  Serial.begin(115200);
  servoHerramienta.setPeriodHertz(50);
  // Mismo rango de pulsos que utiliza el firmware principal para myServo.
  servoHerramienta.attach(pinServoHerramienta, 500, 2400);
  moverServo(valorInicial);

  Serial.println("\nCalibración manual de límites del servo");
  imprimirAyuda();
}

void loop() {
  if (Serial.available() > 0) {
    procesarComando(Serial.readStringUntil('\n'));
  }
}
