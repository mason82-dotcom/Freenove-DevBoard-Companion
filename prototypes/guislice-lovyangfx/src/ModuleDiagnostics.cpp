#include "ModuleDiagnostics.h"
#include <string.h>

namespace ModuleDiagnostics {
bool capture(CompanionModules::Id id, Snapshot& out, uint32_t uptimeMs) {
    // Clear local snapshot, not the GUI's retained strings. The caller uses
    // gslc_ElemSetTxtStr() to detect changes and invalidate text for redraw.
    memset(&out, 0, sizeof(out));
    switch (id) {
        case CompanionModules::Id::Hermes:
            Adapter::hermes(out, uptimeMs); return true;
        case CompanionModules::Id::Webradio:
            Adapter::webradio(out, uptimeMs); return true;
        case CompanionModules::Id::Obd2:
            Adapter::obd2(out, uptimeMs); return true;
        case CompanionModules::Id::Drone:
            Adapter::drone(out, uptimeMs); return true;
        case CompanionModules::Id::System:
            Adapter::system(out, uptimeMs); return true;
        case CompanionModules::Id::PulseOx:
            Adapter::pulseOx(out, uptimeMs); return true;
    }
    return false;
}
}
