#include "../CodigoESP32/RobotConfig.h"
#include <cassert>
#include <cstdio>
#include <iostream>

// Adaptador de pines/tiempo; se ejecuta el mismo MotionRuntime.h del firmware.
static uint32_t fakeNow = 0;
uint32_t millis() { return fakeNow; }
template<class T> T constrain(T x, T lo, T hi) { return x < lo ? lo : x > hi ? hi : x; }
int encoderMux = 0;
void portENTER_CRITICAL(int*) {}
void portEXIT_CRITICAL(int*) {}
uint32_t rightEncoderPos = 0, leftEncoderPos = 0;
const int rightMotorM1 = 14, rightMotorM2 = 12, leftMotorM1 = 13, leftMotorM2 = 15;
int pinPWM[40] = {};
bool ledcWrite(int pin, int pwm) { pinPWM[pin] = pwm; return true; }
enum State { ESPERA, DETENERSE, MOVERSE, GIRAR };
State estado = ESPERA;
bool waitingForStopSettle = false, recibeProgra = false, flancoNegRecibeProgra = false;
bool paro_emergencia = false, obstaculo_detectado = false, flagParar = false, flagEjecucion = false;
bool primer_ciclo = true, ignorarHastaIFFIN = false, ignorarHastaElse = false, ignorarHastaWHILEFIN = false;
int anidamientoWhile = 0, anidamientoWhileIgnorar = 0, anidamientoIF = 0, anidamientoIFIgnorar = 0;
int indicesWhile[5] = {}, inst_actual = 0, inst_final = 0;
bool ejecutandoRamaIf[5] = {};
struct ServoStub { void write(int) {} } myServo;
#include "../CodigoESP32/MotionRuntime.h"

void idle() { estado = ESPERA; paro_emergencia = false; waitingForStopSettle = false; }
void stoppedPins() { assert(pinPWM[14]==0 && pinPWM[12]==0 && pinPWM[13]==0 && pinPWM[15]==0); }

int main() {
  loadConfig(); motorOutputsReady = true;
  processSerialCommand("DEV AttaDev2026");
  processSerialCommand("MOVE 300");
  assert(estado == MOVERSE && diagnosticMove);
  assert(!advanceDesiredDistance(diagnosticAmount));
  fakeNow = 25; advanceDesiredDistance(diagnosticAmount);
  assert(pinPWM[14] > 0 && pinPWM[13] > 0 && pinPWM[12]==0 && pinPWM[15]==0);
  const auto writes = prefs.writes;
  processSerialCommand("SET rppr 900");
  assert(prefs.writes == writes && motionConfig.right.ppr == 820);
  processSerialCommand("STOP");
  stoppedPins(); assert(estado==DETENERSE && !motion.active && !diagnosticMove);
  idle(); fakeNow = 1000;
  // Cambio de dirección y PWM repetido: ambos pines deben actualizarse.
  processSerialCommand("TURN 90"); turnDesiredAngle(diagnosticAmount);
  fakeNow = 1025; turnDesiredAngle(diagnosticAmount);
  assert(pinPWM[12]>0 && pinPWM[13]>0 && pinPWM[14]==0 && pinPWM[15]==0);
  processSerialCommand("STOP"); idle(); fakeNow=2000;
  processSerialCommand("TURN -90"); turnDesiredAngle(diagnosticAmount);
  fakeNow=2025; turnDesiredAngle(diagnosticAmount);
  assert(pinPWM[14]>0 && pinPWM[15]>0 && pinPWM[12]==0 && pinPWM[13]==0);
  processSerialCommand("STOP"); idle();
  processSerialCommand("SET rppr nan"); assert(motionConfig.right.ppr==820);
  processSerialCommand("SET rppr 900"); assert(motionConfig.right.ppr==900 && verifyConfig());
  processSerialCommand("MOTOR R F 80 100");
  assert(motorTestActive && pinPWM[14]==80 && pinPWM[13]==0);
  fakeNow += 100; updateMotorTest();
  stoppedPins(); assert(!motorTestActive && estado==DETENERSE);
  idle(); processSerialCommand("MOVE 100"); advanceDesiredDistance(100);
  fakeNow+=25; advanceDesiredDistance(100);
  fakeNow+=300; advanceDesiredDistance(100);
  stoppedPins(); assert(motion.fault==atta::Fault::Timing && estado==DETENERSE);
  idle(); processSerialCommand("MOVE 100"); assert(estado==ESPERA);
  processSerialCommand("CLEAR"); assert(motion.fault==atta::Fault::None);
  processSerialCommand("MOVE 0"); assert(advanceDesiredDistance(0)); stoppedPins();
  std::cout << "runtime_test: OK\n";
}
