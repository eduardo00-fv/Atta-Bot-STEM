# Control y calibración de movimiento V2

Implementación del 18 de septiembre de 2026. Firmware común para los 13 Attas,
con perfil persistente por robot y parámetros separados por rueda/sentido.
Los valores nuevos son puntos de partida que requieren validación en suelo.

## Respaldo y retorno al código anterior

La copia anterior a estas modificaciones está en
`Respaldos/Robot_antes_control_v2_2026-09-18.tar.gz`. Incluye los 11 archivos que
tenía `Robot`, con los cambios locales de servo y Preferences que ya existían.
El archivo `.sha256.json` junto al respaldo registra su contenido y fue
verificado contra el archivo comprimido. No contiene una lectura de NVS de
los robots: para eso guarde la salida de `SHOW` de cada Atta.

Para consultar/restaurar, descomprima en una carpeta separada, por ejemplo:

```sh
mkdir -p /tmp/atta-anterior
tar -xzf Robot/Respaldos/Robot_antes_control_v2_2026-09-18.tar.gz -C /tmp/atta-anterior
```

Abra el sketch de esa carpeta si necesita volver a cargar la versión anterior.
No mezcle sus archivos con los `.h` V2. El nuevo controlador conserva las claves
antiguas `Rppr/Lppr/kp/ki/kd`; el firmware anterior seguirá leyendo esos valores.
Los cambios de movimiento realizados mediante V2 viven en `motionV2` y no se
reflejan en las claves antiguas. Los ángulos y el nombre sí comparten claves:
un cambio de servo realizado en V2 también será visible en el firmware anterior.

## Archivos y cambios

- `CodigoESP32.ino`: máquina de estados, sensores, BLE y herramientas existentes.
- `MotionControl.h`: PI/PID por rueda, tiempo real, filtro de velocidad,
  anti-windup, rampas, desaceleración, sincronización de recorridos y fallos.
- `MotionRuntime.h`: snapshot de encoders, PWM, inicio/parada, pruebas y telemetría.
- `RobotConfig.h`: perfil, migración, validación y lectura/escritura NVS.

Cada maniobra toma nuevos orígenes sin reiniciar los contadores compartidos con
las interrupciones. Cada rueda debe llegar a su distancia; el promedio ya no
puede ocultar una rueda detenida. El controlador permite PWM cero y el impulso
de arranque tiene duración limitada. Las escrituras al puente siempre incluyen
ambas ruedas y apagan primero el pin opuesto. Se reservan ambos servos antes
del PWM de motores para evitar asignaciones incompatibles. Motores: 1 kHz,
8 bits, igual a los valores predeterminados del `analogWrite` anterior.

STOP cancela la maniobra y el programa, limpia su estado y espera 500 ms. BLE
atiende STOP en cada vuelta cuando hay mensaje, también durante asentamiento
y retroceso por obstáculo. No se permite reemplazar un programa mientras está
ejecutándose. El resto de funciones de herramientas conserva su comportamiento;
sus esperas bloqueantes todavía pueden retrasar STOP mientras actúa una herramienta.

Se mantiene el conteo de cambios en A y B (x4) sin signo, para conservar el PPR
existente. No se añadió detección física de giro contrario ni de transiciones
inválidas de cuadratura. GPIO35 se configura como entrada sin pull-up interno;
verificar el circuito externo. La tensión de motor sigue requiriendo medición
externa. Los encoders no detectan deslizamiento del chasis.

## Migración y persistencia

En el primer arranque se crea un bloque `motionV2` dentro de `Atta-Creds`:

- PPR derecho/izquierdo se copian de sus claves anteriores.
- `Kp` se conserva, `Ki` anterior se multiplica por 0.4 y `Kd` por 0.025,
  convirtiendo aproximadamente el controlador anterior al período nominal de
  25 ms. Es una conversión de unidades, no una garantía de respuesta idéntica:
  ahora hay rampas, filtro, anti-windup y derivada sobre la medición.
- Se conservan nombre y ángulos existentes. Si faltan, se crean con los valores
  iniciales del firmware.
- Los siguientes arranques leen el perfil V2 sin repetir la conversión.

El perfil se escribe como un solo bloque y se relee para verificarlo. Valores
no finitos, rangos inválidos o errores NVS no se anuncian como guardado correcto.
Si la configuración almacenada es inválida, se bloquea movimiento sin reemplazarla
silenciosamente; repárela mediante `SET` y compruebe `VERIFY`. `SHOW` muestra RAM,
`VERIFY` comprueba NVS. Guarde ambas salidas con el ID del robot antes/después de
calibrar. No borre flash ni cambie particiones durante una actualización ordinaria.

## Comandos Serial

Monitor a **115200**, terminación **Nueva línea**. Cierre otras aplicaciones que
usen el puerto. Abra el acceso con `DEV AttaDev2026` (o la clave configurada).

| Comando | Uso |
|---|---|
| `SHOW` | Configuración y campos `SET` |
| `VERIFY` | Verificación de almacenamiento |
| `SET campo valor` | Guardar parámetro; solo detenido y sin programa pendiente |
| `TEST 85` | Probar ángulo del pilot sin guardar |
| `MOVE 300` / `MOVE -300` | Recorrer 300 mm adelante/atrás; máximo absoluto 3000 mm |
| `TURN 90` / `TURN -90` | Giro derecha/izquierda; máximo absoluto 3600° |
| `MOTOR R F 80 1000` | Solo rueda derecha, adelante, PWM 80, durante 1000 ms |
| `MOTOR L R 80 1000` | Solo rueda izquierda, atrás |
| `ENCODERS` | Cuentas acumuladas derecha/izquierda; no reinicia contadores |
| `TELEMETRY ON` / `TELEMETRY OFF` | CSV por Serial, hasta 10 muestras/s |
| `STOP` | Parada sin requerir clave DEV |
| `CLEAR` | Despejar fallo estando detenido y con configuración válida |
| `EXIT` | Cerrar acceso y telemetría; no sustituye a STOP |

Los comandos de configuración y pruebas se ignoran durante movimiento o espera
de asentamiento. STOP y TELEMETRY siguen disponibles. Después de un fallo, revise
la causa y use CLEAR; no se reanuda automáticamente el programa interrumpido.
`SET device ...` requiere reiniciar para cambiar el nombre anunciado por BLE.

`MOTOR` admite PWM entre 0 y `maxpwm`, y duración 100–3000 ms. Se detiene al
terminar o si no recibe pulsos durante `stallms`. Imprime `MOTOR_BEGIN` y
`MOTOR_DONE` con los contadores, y espera 500 ms antes de admitir otra prueba.
En telemetría de esta prueba las referencias son cero porque el PWM es manual.

## Parámetros

Las unidades de distancia son mm; velocidad mm/s; aceleración mm/s².
`r` es derecha y `l` izquierda. En umbrales, `f` es adelante y el segundo `r`
es atrás: `rfstart` = derecha adelante; `rrstart` = derecha atrás.

| Campos | Rango permitido / significado |
|---|---|
| `rppr`, `lppr` | 1–100000 cuentas por vuelta de salida |
| `rradius`, `lradius` | 5–100 mm, radio efectivo |
| `track` | 40–400 mm, separación efectiva entre ruedas |
| `rkp/lkp`, `rki/lki` | 0–100; ganancias proporcionales e integrales |
| `rkd/lkd` | 0–10; derivada sobre velocidad medida; inicial 0 |
| `kp/ki/kd` | Alias que asignan ambas ruedas; unidades V2 |
| `rfstart/rrstart/lfstart/lrstart` | 0–`maxpwm`; impulso inicial si aún no hay avance; inicial 60 |
| `rfhold/rrhold/lfhold/lrhold` | 0–`start` del mismo sentido; piso solo mientras falta velocidad; inicial 0 |
| `rfff/rrff/lfff/lrff` | 0–10 PWM/(mm/s), aporte base proporcional a velocidad; inicial 0 |
| `maxpwm` | 1–255; inicial 200; no puede quedar por debajo de umbrales existentes |
| `speed`, `turnspeed` | 10–300; iniciales 90 y 60 |
| `accel`, `decel` | 10–1000; iniciales 120 y 180 |
| `sync` | 0–10 (mm/s)/mm; inicial 1; reduce referencia de rueda adelantada |
| `tolerance` | 0.1–5 mm por rueda; inicial 0.5; no es una garantía de precisión final |
| `filterms` | 0–200 ms; inicial 50; cero desactiva filtro |
| `stallms` | 300–5000 ms sin cuentas con PWM aplicado; inicial 1500 |
| `startms` | 0–500 ms y menor que `stallms`; inicial 200 |
| `neutral/active/inactive` | Enteros 0–180, servo del pilot |
| `forward/reverse/stop` | Enteros 0–180, servo continuo del set |

Fallos: `RIGHT_STALL`, `LEFT_STALL`, `TIMEOUT`, `CONTROL_LATE` y `CONFIG`.
El control corre cada 25 ms o más; si una ventana supera 250 ms, se aborta.
Cada maniobra tiene límite total derivado de distancia y velocidad, entre
5 y 120 segundos. Aumentar umbrales no corrige un encoder desconectado.

## Calibración en el mismo firmware

1. Respalde SHOW por Atta y compruebe VERIFY. Pruebe primero un robot; luego
   uno con buen comportamiento y dos con síntomas diferentes antes del resto.
2. Con motores apagados, consulte ENCODERS, gire manualmente cada rueda varias
   vueltas completas en un sentido y consulte de nuevo. PPR = diferencia de
   cuentas / vueltas. Repita y compare ambos sentidos. Evite movimiento de la
   otra rueda al medir. El antiguo útil de media vuelta queda como referencia.
3. Con robot preparado para una prueba breve, use MOTOR en cada rueda/sentido,
   aumentando PWM gradualmente hasta medir arranque consistente. Repita bajo
   carga real en suelo; la respuesta elevada no sustituye la del suelo.
   Mida también el PWM necesario para sostener movimiento. Si dispara STALL,
   revise la causa y despeje el fallo antes de la siguiente prueba.
4. Guarde `*start` y `*hold`; construya una curva PWM/velocidad. El aporte base
   `*ff` representa una pendiente aproximada de esa curva, no una ganancia PID.
   Puede dejarlo en cero mientras no tenga datos. Calibre PI con D inicialmente
   cero, mirando velocidad, PWM y saturación. No ajuste PPR para cambiar rapidez.
5. Pruebe MOVE en ambos sentidos con pilot levantado y apoyado. Ajuste presión
   del pilot, rampas y frenado; luego radios efectivos y separación con rectas
   de varias longitudes y giros repetidos. No use geometría para ocultar una
   parada irregular. El frenado implementado reduce la referencia y termina
   con ambas entradas del puente en cero; no se asume un modo de freno activo
   del driver sin verificar su hardware.
6. Repita rectas, giros, cuadrados, STOP→nuevo movimiento y obstáculo→retroceso.
   Use las metas y registro de la auditoría. Guarde SHOW/VERIFY finales, reinicie
   y confirme persistencia. Extienda a los 13 cuando los resultados sean repetibles.

La distancia de telemetría queda en el último muestreo de la maniobra. Para
medir avance por inercia tras la parada, compare cuentas acumuladas al parar y
tras asentamiento; no interprete esa distancia congelada como posición física final.

## Captura en computadora

Instale `pyserial` en su entorno Python y ejecute desde la raíz del repositorio:

```sh
python3 Robot/tools/capturar_movimiento.py --port /dev/ttyUSB0 --output /tmp/Atta13_ensayo01.csv
```

En Windows sustituya el puerto por `COM3` u otro correspondiente. La herramienta
solicita la clave, pide SHOW/VERIFY y activa telemetría; no inicia movimientos.
Escriba comandos en su consola. `salir` o Ctrl+C envían STOP antes de cerrar.
Se crea CSV y un `.log` con configuración, comandos y fallos; no sobrescribe
ensayos existentes. Si pierde conexión, las maniobras están acotadas por sus
tiempos máximos, pero cerrar el cable no equivale a una parada física inmediata.

El CSV incluye ID, tiempo, modo, sentido solicitado, dt, cuentas, distancia,
referencia y velocidad por rueda, PWM aplicado, integral, saturación y fallo.
No contiene tensión medida. Se descartan muestras si el buffer Serial está
lleno para no bloquear el control. Este flujo reemplaza para V2 al script BLE
del banco histórico; no se añadió una característica BLE Notify.

## Validación de software

Se dispone de pruebas del controlador sin hardware, del almacenamiento con
NVS simulado y de integración de comandos/salidas con pines simulados. Desde la raíz:

```sh
g++ -std=c++11 -Wall -Wextra -Werror Robot/tests/motion_control_test.cpp -o /tmp/atta-motion-test
/tmp/atta-motion-test
g++ -std=c++11 -Wall -Wextra -Werror -IRobot/tests/stubs Robot/tests/config_test.cpp -o /tmp/atta-config-test
/tmp/atta-config-test
g++ -std=c++11 -Wall -Wextra -Werror -IRobot/tests/stubs Robot/tests/runtime_test.cpp -o /tmp/atta-runtime-test
/tmp/atta-runtime-test
arduino-cli compile --fqbn esp32:esp32:esp32 --build-path /tmp/atta-motion-v2-build Robot/CodigoESP32
```

Entorno de compilación usado: Arduino ESP32 3.3.11, ESP32Servo 3.2.1 y FastLED
3.10.5. La API de PWM requiere Arduino ESP32 3.x. Se compiló para ESP32 clásico
genérico; confirme la selección de IdeaBoard y sus particiones al cargar.
Las pruebas cubren unidades del PID, saturación, rampas, final por ambas ruedas,
sentidos, parada/reinicio, fallos, desbordamiento de contadores/tiempo, motores
simulados diferentes y migración sin pérdida de PPR/servo ni reconversión de Ki.
Las de integración comprueban salidas de ambos motores al cambiar de sentido,
STOP, bloqueo de SET durante marcha, fallos enclavados y duración de MOTOR.
No se ha cargado ni validado físicamente esta versión en los Attas.
