#include "psp/psp_rain.h"

namespace PspRain {

namespace {
u32 s_layers = 0;
} // namespace

void SetLayers(u32 layers) {
    s_layers = layers;
}

u32 GetLayers() {
    return s_layers;
}

} // namespace PspRain
