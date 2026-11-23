// ESP32 B. Cada computador usa a sua placa e o seu par de módulos RF.
#define NO_EH_B 1
#include <TcdRadio.h>

void setup() {
  iniciarCircuito();
}

void loop() {
  atenderCircuito();
}
