#include "../../lib/Marlin/Marlin/src/gcode/queue.h"
#include "PrusaGcodeSuite.hpp"
#include "cmsis_os.h"

void PrusaGcodeSuite::M999() {
    queue.ok_to_send();
    osDelay(1000);
    NVIC_SystemReset();
}
