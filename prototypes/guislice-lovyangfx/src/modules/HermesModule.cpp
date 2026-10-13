#include "ModuleDiagnostics.h"
#include "HermesStatus.h"

namespace ModuleDiagnostics::Adapter {
void hermes(Snapshot& out, uint32_t uptimeMs) {
    // A GUI read is a zero-wait copy. No Wi-Fi, HTTP, TLS or mutex waiting
    // is performed in the GUIslice event loop.
    static HermesStatus::Snapshot last{};
    HermesStatus::Snapshot fresh{};
    if (HermesStatus::copy(fresh)) last = fresh;
    HermesStatus::format(last, uptimeMs, out.text);
}
}
