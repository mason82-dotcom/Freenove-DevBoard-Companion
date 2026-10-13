#include "ModuleGraphics.h"
#include <Arduino.h>
#include <GUIslice.h>
#include <stdint.h>
#include "PulsOxSensor.h"

// Readable, compact visualization on a 320x240 landscape FNK0086.
// The blue/green/coral visual language follows the source PulsOxMonitor.
// Every visualization explicitly distinguishes real samples from placeholders.
namespace {
static gslc_tsElemRef* canvases[CompanionModules::kModuleCount] = {};
static uint32_t heapHistory[72] = {};
static uint8_t heapCount = 0, heapHead = 0;
static const gslc_tsColor bg = {10, 17, 29};
static const gslc_tsColor grid = {34, 50, 69};
static const gslc_tsColor muted = {103, 124, 143};
static const gslc_tsColor accent = {82, 202, 244};
static const gslc_tsColor green = {69, 219, 146};
static const gslc_tsColor red = {250, 94, 104};
static const gslc_tsColor amber = {240, 182, 82};
static const gslc_tsColor pale = {198, 222, 243};

void dot(gslc_tsGui* g, int16_t x, int16_t y,
         uint16_t radius, gslc_tsColor color) {
    gslc_DrawFillCircle(g, x, y, radius, color);
}
void line(gslc_tsGui* g, int16_t x0, int16_t y0,
          int16_t x1, int16_t y1, gslc_tsColor color) {
    gslc_DrawLine(g, x0, y0, x1, y1, color);
}
void bar(gslc_tsGui* g, int16_t x, int16_t y, int16_t width,
         int16_t height, uint8_t percent, gslc_tsColor color) {
    gslc_DrawFillRect(g, {x,y,width,height}, grid);
    if(percent > 100) percent=100;
    if (percent != 0) {
        int16_t used = static_cast<int16_t>((static_cast<uint32_t>(width) * percent)/100);
        if (used == 0) used = 1;
        gslc_DrawFillRect(g, {x,y,used,height}, color);
    }
}
void pulse(gslc_tsGui* g, gslc_tsRect r) {
    // Separate read-only snapshot; never reads Wire on the GUI thread.
    const PulsOx::Snapshot s = PulsOx::read();
    const int16_t cy = r.y + 39;
    const bool active = s.running && s.sensorOk && s.finger;
    dot(g,r.x+26,cy-6,9,active ? red : muted);
    dot(g,r.x+42,cy-6,9,active ? red : muted);
    for(int16_t k=0;k<13;++k) {
        const int16_t w=13-k;
        line(g,r.x+17+k,cy+k/2,r.x+51-k,cy+k/2,
             active ? red : muted);
    }

    // Original PulsOxMonitor uses an IR ring buffer and a 320x86 sprite.
    // This condensed 320x240 variant uses 128 samples rendered entirely
    // through GUIslice: no second graphics driver or unguarded I2C reads.
    uint16_t samples[128]={};
    const uint16_t count=PulsOx::readWave(samples,128);
    uint16_t lo=0xffff,hi=0;
    for(uint16_t i=0;i<count;++i){
        if(samples[i]<lo)lo=samples[i];
        if(samples[i]>hi)hi=samples[i];
    }
    const uint32_t span=(hi>lo && hi-lo>=40) ? hi-lo : 40;
    const int16_t left=r.x+76, width=r.w-86, top=r.y+10, height=r.h-20;
    line(g,left,top+height/2,left+width,top+height/2,grid);
    if (!active || count<2) return; // no fake oxygen waveform
    for(uint16_t i=1;i<count;++i) {
        int16_t x0=left+(static_cast<int32_t>(i-1)*width)/(count-1);
        int16_t x1=left+(static_cast<int32_t>(i)*width)/(count-1);
        int16_t y0=top+height-1-(static_cast<uint32_t>(samples[i-1]-lo)*(height-3))/span;
        int16_t y1=top+height-1-(static_cast<uint32_t>(samples[i]-lo)*(height-3))/span;
        if(y0<top)y0=top;if(y1<top)y1=top;
        line(g,x0,y0,x1,y1,green);
    }
}
void sys(gslc_tsGui* g, gslc_tsRect r) {
    const uint32_t totalHeap=ESP.getHeapSize();
    const uint32_t freeHeap=ESP.getFreeHeap();
    const uint32_t totalPsram=ESP.getPsramSize();
    const uint32_t freePsram=ESP.getFreePsram();
    const uint8_t heapPct=totalHeap?static_cast<uint8_t>(
         (static_cast<uint64_t>(freeHeap)*100)/totalHeap):0;
    const uint8_t psramPct=totalPsram?static_cast<uint8_t>(
         (static_cast<uint64_t>(freePsram)*100)/totalPsram):0;
    bar(g,r.x+10,r.y+12,108,9,heapPct,accent);
    bar(g,r.x+10,r.y+32,108,9,psramPct,green);
    // Timeline of *actual* free heap readings, not decorative samples.
    const int16_t left=r.x+138, width=r.w-150;
    const int16_t top=r.y+9,height=r.h-20;
    line(g,left,top+height,left+width,top+height,muted);
    if(heapCount<2)return;
    uint32_t lo=UINT32_MAX,hi=0;
    for(uint8_t i=0;i<heapCount;++i) {
        uint8_t p=(heapHead+72-heapCount+i)%72;
        uint32_t v=heapHistory[p]; if(v<lo)lo=v; if(v>hi)hi=v;
    }
    uint32_t span=hi>lo?hi-lo:1;
    if(span<1024)span=1024;
    for(uint8_t i=1;i<heapCount;++i){
        const uint8_t a=(heapHead+72-heapCount+i-1)%72;
        const uint8_t b=(heapHead+72-heapCount+i)%72;
        const int16_t x0=left+(static_cast<int32_t>(i-1)*width)/71;
        const int16_t x1=left+(static_cast<int32_t>(i)*width)/71;
        const int16_t y0=top+height-1-((heapHistory[a]-lo)*(height-4))/span;
        const int16_t y1=top+height-1-((heapHistory[b]-lo)*(height-4))/span;
        line(g,x0,y0,x1,y1,accent);
    }
}
void hermes(gslc_tsGui* g,gslc_tsRect r) {
    const int16_t y=r.y+r.h/2;
    for(int i=0;i<3;++i){
        const int16_t x=r.x+47+i*95;
        if(i<2)line(g,x+10,y,x+85,y,grid);
        dot(g,x,y,19,grid);dot(g,x,y,10,muted);
    }
    // Deliberately inactive until the separate opt-in TLS branch is merged.
}
void radio(gslc_tsGui* g,gslc_tsRect r) {
    const int16_t cy=r.y+r.h-15;
    for(int i=0;i<12;++i){
        const int16_t x=r.x+35+i*21;
        const int16_t h=14+((i*7)%30);
        // Muted stationary equalizer: NOT live sound or an active stream.
        gslc_DrawFillRect(g,{x,static_cast<int16_t>(cy-h),10,h},grid);
    }
    line(g,r.x+20,cy+2,r.x+r.w-20,cy+2,muted);
}
void obd(gslc_tsGui* g,gslc_tsRect r){
    const int16_t cx=r.x+r.w/2, cy=r.y+r.h-6;
    // Gauge is parked at the stop because the ELM327 is disconnected.
    for(int i=0;i<=10;++i){
        int16_t x=r.x+34+i*(r.w-68)/10;
        line(g,x,cy-17,x,cy-22,muted);
    }
    line(g,r.x+34,cy-22,r.x+r.w-34,cy-22,grid);
    line(g,cx,cy-16,r.x+42,cy-16,amber);
    dot(g,cx,cy-16,4,amber);
}
void drone(gslc_tsGui* g,gslc_tsRect r){
    const int16_t cx=r.x+r.w/2,cy=r.y+r.h/2;
    line(g,r.x+18,cy,r.x+r.w-18,cy,grid);
    line(g,cx,r.y+12,cx,r.y+r.h-12,grid);
    line(g,cx-45,cy-16,cx-10,cy-16,muted);
    line(g,cx+10,cy-16,cx+45,cy-16,muted);
    line(g,cx-10,cy+11,cx,cy+20,amber);
    line(g,cx,cy+20,cx+10,cy+11,amber);
    // Fixed NO DATA state. This is not an attitude indicator.
}
}

namespace ModuleGraphics {
void bind(uint8_t index,gslc_tsElemRef* canvas){
    if(index<CompanionModules::kModuleCount)canvases[index]=canvas;
}
void sampleSystem(uint32_t heap) {
    heapHistory[heapHead]=heap;
    heapHead=(heapHead+1)%72;
    if(heapCount<72)++heapCount;
}
void refresh(gslc_tsGui* gui,uint8_t index) {
    if(index<CompanionModules::kModuleCount && canvases[index])
        gslc_ElemSetRedraw(gui,canvases[index],GSLC_REDRAW_FULL);
}
bool draw(void* pvGui,void* pvElemRef,gslc_teRedrawType) {
    auto* g=static_cast<gslc_tsGui*>(pvGui);
    auto* ref=static_cast<gslc_tsElemRef*>(pvElemRef);
    if(!g || !ref)return false;
    uint8_t index=0xff;
    for(uint8_t i=0;i<CompanionModules::kModuleCount;++i)
        if(canvases[i]==ref) {index=i;break;}
    if(index==0xff)return false;
    const gslc_tsRect r=gslc_ElemGetRect(g,ref);
    gslc_DrawFillRect(g,r,bg);
    gslc_DrawFrameRect(g,r,grid);
    switch(CompanionModules::kModules[index].id){
        case CompanionModules::Id::Hermes: hermes(g,r);break;
        case CompanionModules::Id::Webradio: radio(g,r);break;
        case CompanionModules::Id::Obd2: obd(g,r);break;
        case CompanionModules::Id::Drone: drone(g,r);break;
        case CompanionModules::Id::System: sys(g,r);break;
        case CompanionModules::Id::PulseOx: pulse(g,r);break;
    }
    return true;
}
}
