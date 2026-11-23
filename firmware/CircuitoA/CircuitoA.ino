// ESP32 A. Cada computador usa a sua placa e o seu par de módulos RF.
#define NO_EH_B 0
#include <TcdRadio.h>

void setup() {
  iniciarCircuito();
}

void loop() {
  atenderCircuito();
}
