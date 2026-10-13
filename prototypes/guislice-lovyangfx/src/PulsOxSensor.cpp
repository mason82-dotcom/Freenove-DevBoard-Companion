#include "PulsOxSensor.h"
#include "SharedI2C.h"
#include <Arduino.h>
#include <Wire.h>
#include <MAX30105.h>
#include <heartRate.h>
#include <spo2_algorithm.h>
#include <string.h>
#include <atomic>

// Adapted from the user-provided PulsOxMonitor source (SparkFun MAX3010x
// v1.1.2). No parallel TFT_eSPI UI task and NO LED output on GPIO48.
// This is an educational demo, not a medical device.
namespace PulsOx {
namespace {
constexpr uint8_t kAddress=0x57;
constexpr uint8_t kAvg=4;
constexpr uint16_t kRate=100;
constexpr uint8_t kEffectiveHz=kRate/kAvg;
constexpr uint16_t kWindow=100;
constexpr uint8_t kShift=25;
constexpr uint16_t kWave=128;
constexpr uint32_t kFingerThreshold=50000UL;
static_assert(kEffectiveHz==25 && kWindow==4*kEffectiveHz && kShift==kEffectiveHz,
              "Maxim SpO2 algorithm expects 25 Hz effective sample rate");

MAX30105 sensor;
portMUX_TYPE dataMux=portMUX_INITIALIZER_UNLOCKED;
Snapshot latest{};
uint16_t waveform[kWave]={};
uint16_t waveHead=0,waveCount=0;
uint32_t redBuf[kWindow]={},irBuf[kWindow]={};
uint8_t bpmRates[4]={};
uint8_t ratePos=0, rateCount=0, fingerCandidate=0;
uint16_t windowCount=0;
uint32_t lastBeat=0;
TaskHandle_t sensorWorker=nullptr;
std::atomic<bool> desiredActive{false};
// A stop must not disappear when Back and re-enter occur very quickly.
std::atomic<bool> stopPending{false};

void publish(const Snapshot& s) {
    portENTER_CRITICAL(&dataMux);
    latest=s;
    portEXIT_CRITICAL(&dataMux);
}
void append(uint32_t ir) {
    const uint16_t value=static_cast<uint16_t>((ir >> 4)&0xffffu);
    portENTER_CRITICAL(&dataMux);
    waveform[waveHead]=value;
    waveHead=(waveHead+1)%kWave;
    if (waveCount<kWave)++waveCount;
    portEXIT_CRITICAL(&dataMux);
}
void reset(Snapshot& s) {
    ratePos=rateCount=fingerCandidate=0;
    lastBeat=0;windowCount=0;
    memset(bpmRates,0,sizeof(bpmRates));
    s.bpm=s.spo2=0;
    s.bpmValid=s.spo2Valid=false;
    s.lastBeatMs=0;
    // Only a genuine new sample can make the sensor state fresh again.
    portENTER_CRITICAL(&dataMux);
    memset(waveform,0,sizeof(waveform));
    waveHead=waveCount=0;
    portEXIT_CRITICAL(&dataMux);
}

bool initialize(Snapshot& s) {
    SharedI2C::Guard lock(150);
    if (!lock) return false;
    // SparkFun's begin() calls Wire.begin() on the already configured Wire bus.
    // SharedI2C initialized it first with the verified SDA2/SCL1 at 400kHz.
    const bool found=sensor.begin(Wire, I2C_SPEED_FAST, kAddress);
    if(found) {
        sensor.setup(60,4,2,kRate,411,4096);
        sensor.clearFIFO();
    }
    s.sensorOk=found;
    reset(s);
    publish(s);
    return found;
}
void onSample(uint32_t red,uint32_t ir,Snapshot& s) {
    const uint32_t now=millis();
    s.lastSampleMs=now;
    append(ir);
    const bool detected=ir>=kFingerThreshold;
    if (detected != s.finger) {
        if(++fingerCandidate>=3) {
            s.finger=detected;
            fingerCandidate=0;
            reset(s);
        }
    } else fingerCandidate=0;
    if(!s.finger || !detected) {
        publish(s);
        return;
    }
    if(s.bpmValid && static_cast<uint32_t>(now-s.lastBeatMs)>3000u) {
        s.bpmValid=false;s.bpm=0;rateCount=0;lastBeat=0;
    }
    if(checkForBeat(static_cast<int32_t>(ir))) {
        if (lastBeat!=0 && now>lastBeat) {
            const uint32_t bpm=60000u/(now-lastBeat);
            if(bpm>20 && bpm<255) {
                bpmRates[ratePos++]=static_cast<uint8_t>(bpm);
                ratePos%=4;
                if(rateCount<4)++rateCount;
                uint16_t sum=0;
                for(uint8_t i=0;i<rateCount;++i)sum+=bpmRates[i];
                s.bpm=sum/rateCount;
                s.bpmValid=true;
                s.lastBeatMs=now;
                ++s.beatCounter;
            }
        }
        lastBeat=now;
    }
    irBuf[windowCount]=ir; redBuf[windowCount]=red;
    ++windowCount;
    if(windowCount==kWindow) {
        int32_t oxygen=0,hr=0;
        int8_t oxygenValid=0,hrValid=0;
        maxim_heart_rate_and_oxygen_saturation(
            irBuf,kWindow,redBuf,&oxygen,&oxygenValid,&hr,&hrValid);
        if(oxygenValid && oxygen>0 && oxygen<=100){
            s.spo2=oxygen;s.spo2Valid=true;
        } else {s.spo2=0;s.spo2Valid=false;}
        memmove(irBuf,irBuf+kShift,(kWindow-kShift)*sizeof(uint32_t));
        memmove(redBuf,redBuf+kShift,(kWindow-kShift)*sizeof(uint32_t));
        windowCount=kWindow-kShift;
    }
    publish(s);
}
void task(void*) {
    Snapshot s{};
    bool configured = false;
    bool sessionActive = false;
    uint32_t lastRetry = 0, lastData = 0;
    for (;;) {
        // A stop request is latched, so even a rapid Back -> PulsOx cycle
        // shuts down the previous session before starting a new one.
        const bool mustStop = stopPending.exchange(false);
        if (!desiredActive.load() || mustStop) {
            if (configured) {
                // Power down the MAX30102 LED/ADC ONLY from this worker.
                // FT6336U touch shares Wire, so all I2C stays serialized.
                SharedI2C::Guard lock(150);
                if (!lock) {
                    stopPending.store(true);
                    vTaskDelay(pdMS_TO_TICKS(10));
                    continue;
                }
                sensor.shutDown();
                configured = false;
                Serial.println("PulsOx MAX30102: SLEEP");
            }
            if (sessionActive || mustStop) {
                Serial.println("PulsOx MAX30102: STOPPED");
            }
            sessionActive = false;
            s = Snapshot{};
            reset(s);
            publish(s);
            if (!desiredActive.load()) {
                // No sampling, I2C polling or LED activity while inactive.
                ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            }
            continue;
        }

        if (!sessionActive) {
            // Never reuse pulse, SpO2, or the trace from an earlier visit.
            sessionActive = true;
            s = Snapshot{};
            reset(s);
            s.running = true;
            publish(s);
            lastRetry = millis() - 2000u;
            lastData = millis();
            Serial.println("PulsOx MAX30102: START");
        }

        if(!s.sensorOk) {
            if(static_cast<uint32_t>(millis()-lastRetry)>=2000u) {
                lastRetry=millis();
                if(initialize(s)){
                    configured = true;
                    Serial.println("PulsOx MAX30102: FOUND");
                    lastData=millis();
                } else Serial.println("PulsOx MAX30102: MISSING");
            }
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        // Only the sensor's I2C read is locked; beat and Maxim algorithms run
        // outside the mutex and can never stall the FT6336U touch read.
        {
            SharedI2C::Guard lock(30);
            if (lock) sensor.check();
        }
        bool haveSample=false;
        // SparkFun FIFO ring contains already fetched in-memory samples.
        uint8_t batch=0;
        while(sensor.available() && batch++<8) {
            const uint32_t red=sensor.getFIFORed();
            const uint32_t ir=sensor.getFIFOIR();
            sensor.nextSample();
            haveSample=true;
            onSample(red,ir,s);
        }
        if(haveSample) lastData=millis();
        else if(static_cast<uint32_t>(millis()-lastData)>=2000u) {
            s.sensorOk=false;s.finger=false;
            reset(s);publish(s);
            lastRetry=millis()-2000;
            Serial.println("PulsOx MAX30102: no samples, retry");
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
}
bool begin() {
    if (sensorWorker) return true;
    if (!SharedI2C::ready()) return false;
    // Create once, but no sensor I2C until the page is entered.
    return xTaskCreatePinnedToCore(
        task, "pulsox", 6144, nullptr, 2, &sensorWorker, 0) == pdPASS;
}
bool requestActive(bool active) {
    if (!sensorWorker) return false;
    // Only the worker accesses MAX30102; this UI call never blocks on I2C.
    if (!active) stopPending.store(true);
    desiredActive.store(active);
    portENTER_CRITICAL(&dataMux);
    latest = Snapshot{};
    memset(waveform, 0, sizeof(waveform));
    waveHead = waveCount = 0;
    portEXIT_CRITICAL(&dataMux);
    xTaskNotifyGive(sensorWorker);
    return true;
}
Snapshot read() {
    Snapshot out;
    portENTER_CRITICAL(&dataMux);
    out=latest;
    portEXIT_CRITICAL(&dataMux);
    if (!desiredActive.load()) return Snapshot{};
    return out;
}
uint16_t readWave(uint16_t* out,uint16_t capacity) {
    if (!out || !capacity || !desiredActive.load()) return 0;
    portENTER_CRITICAL(&dataMux);
    const uint16_t n=capacity<waveCount?capacity:waveCount;
    const uint16_t start=(waveHead+kWave-n)%kWave;
    for(uint16_t i=0;i<n;++i)out[i]=waveform[(start+i)%kWave];
    portEXIT_CRITICAL(&dataMux);
    return desiredActive.load() ? n : 0;
}
} // namespace PulsOx
