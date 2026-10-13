#include "PulsOxSensor.h"
// Native C++ host test: exercise the actual formatter without GPIO/I2C.
namespace PulsOx {
bool begin(){return false;}
Snapshot read(){return {};}
uint16_t readWave(uint16_t*,uint16_t){return 0;}
}
