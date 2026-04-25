# Chapter 25: eHealth365 — Wearable Health Monitoring

*Author: Srikanth Patchava & EmbeddedOS Contributors*

---

## 25.1 Introduction

The **eHealth365** is a two-device health monitoring ecosystem designed to
cover approximately 90% of all health metrics using just a smart ring, a
smart patch, and a mobile app. The system combines continuous biometric
sensing with periodic blood chemistry analysis.

---

## 25.2 System Architecture

```
┌─────────────────────────┐     ┌─────────────────────────┐
│   Smart Ring Pro        │     │   Smart Patch Pro        │
│   Finger — worn 24/7    │     │   Upper arm — weekly     │
│                         │     │                         │
│  Heart rate + HRV       │     │  Blood glucose (CGM)    │
│  SpO2 oxygen            │     │  Electrolytes (sweat)   │
│  Body temperature       │     │  Hydration (bioZ)       │
│  Sleep stages           │     │  Skin pH                │
│  Ketones (breath)       │     │  Monthly blood labs     │
│  Steps and activity     │     │  Lactate (workout)      │
│  Stress score (HRV)     │     │                         │
└────────────┬────────────┘     └────────────┬────────────┘
             │         Bluetooth LE          │
             └──────────────┬────────────────┘
                            ▼
                  ┌─────────────────────┐
                  │   Mobile App        │
                  │   Single Health Hub │
                  │                     │
                  │  AI food camera     │
                  │  Digital twin       │
                  │  Doctor sharing     │
                  │  Deficiency alerts  │
                  └─────────────────────┘
```

---

## 25.3 The Two Devices

### Smart Ring Pro — Vitality Hub

The ring monitors the autonomic nervous system and movement. It is designed
for 24/7 wear with no screen and haptic feedback only.

| Specification    | Value                              |
|------------------|------------------------------------|
| Form Factor      | Titanium/ceramic ring              |
| Battery Life     | 4-5 days                           |
| Charging         | Wireless (Qi)                      |
| Sensors          | PPG, thermistor, accelerometer     |
| Connectivity     | Bluetooth LE                       |
| Water Resistance | IP68                               |

### Smart Patch Pro — Chemistry Hub

The patch monitors blood and interstitial fluid chemistry. It adheres to
the upper arm and is replaced weekly.

| Specification    | Value                              |
|------------------|------------------------------------|
| Form Factor      | Flexible adhesive puck             |
| Battery Life     | 7 days                             |
| Replacement      | Weekly patch, monthly cartridge    |
| Sensors          | CGM, sweat biosensors, bioimpedance|
| Connectivity     | Bluetooth LE                       |

---

## 25.4 Health Metrics Coverage

| Metric                    | Device  | Method              | Schedule      |
|---------------------------|---------|---------------------|---------------|
| Heart rate and HRV        | Ring    | Optical PPG         | Continuous    |
| SpO2 oxygen               | Ring    | Optical sensor      | Continuous    |
| Body temperature          | Ring    | Thermistor          | Continuous    |
| Sleep stages              | Ring    | Accelerometer + HRV | Nightly       |
| Steps and activity        | Ring    | Accelerometer       | Continuous    |
| Stress score              | Ring    | HRV analysis        | Hourly        |
| Ketones                   | Ring    | Micro breath port   | 2x daily      |
| Blood glucose             | Patch   | CGM micro-sensor    | Every 15 min  |
| Sodium, potassium         | Patch   | Sweat biosensor     | Every 4 hrs   |
| Magnesium, zinc           | Patch   | Sweat biosensor     | Every 4 hrs   |
| Hydration level           | Patch   | Bioimpedance        | 3x daily      |
| Skin pH                   | Patch   | pH sensor           | Every 4 hrs   |
| Lactate                   | Patch   | Sweat sensor        | During exercise|
| Calories burned           | Patch   | Temp + glucose      | Continuous    |
| Vitamins (A-K, B1-B12)    | Patch   | Monthly cartridge   | Monthly       |
| Minerals (Fe, Zn, Ca, Mg) | Patch   | Monthly cartridge   | Monthly       |
| Calories in (nutrition)   | App     | AI food camera      | Per meal      |

---

## 25.5 Hardware Design — Smart Ring Pro

The ring PCB uses a flexible circuit board that wraps around the inside
of the titanium band:

### Key Components

- **MCU:** Nordic nRF52840 (Cortex-M4F, BLE 5.0)
- **PPG Sensor:** MAX30102 (heart rate, SpO2)
- **Accelerometer:** LIS3DH (steps, sleep detection)
- **Thermistor:** NTC 10K (body temperature)
- **Haptic Motor:** LRA vibration motor
- **Battery:** 30 mAh LiPo (wireless charging)

### Power Budget

| Mode        | Current | Duration    | Daily mAh |
|-------------|---------|-------------|-----------|
| Active PPG  | 3.5 mA  | 10 min/hr   | 14.0      |
| Sleep track | 0.8 mA  | 8 hrs       | 6.4       |
| Idle + BLE  | 0.15 mA | Remaining   | 1.5       |
| **Total**   |         |             | ~22 mAh   |

With a 30 mAh battery, this yields approximately 1.3 days. The actual
4-5 day battery life is achieved through aggressive duty cycling and
on-demand sensor activation.

---

## 25.6 Hardware Design — Smart Patch Pro

The patch uses a flexible PCB with integrated biosensors:

### Key Components

- **MCU:** Nordic nRF5340 (dual Cortex-M33, BLE 5.3)
- **CGM Sensor:** Custom glucose micro-needle array
- **Sweat Sensors:** Ion-selective electrodes (Na+, K+, Mg2+, Zn2+)
- **Bioimpedance:** AD5940 analog front-end
- **pH Sensor:** ISFET-based pH electrode
- **Blood Cartridge:** Microfluidic slot for monthly labs
- **Battery:** 150 mAh flexible LiPo

### Monthly Blood Cartridge

The blood cartridge is a microfluidic device that performs a finger-prick
blood test. It measures:
- Complete vitamin panel (A, C, D, E, K, B1-B12)
- Mineral levels (iron, zinc, calcium, magnesium)
- Basic metabolic panel

---

## 25.7 Mobile App Architecture

The mobile app serves as the central health hub:

| Module              | Function                               |
|---------------------|----------------------------------------|
| **Dashboard**       | Real-time vitals, trends, scores       |
| **AI Food Camera**  | Photograph meals for calorie estimation|
| **Digital Twin**    | 3D body model with health overlays     |
| **Doctor Portal**   | Share reports with healthcare providers|
| **Alert Engine**    | Deficiency alerts and recommendations  |
| **Data Export**     | FHIR/HL7 compatible health records     |

---

## 25.8 Sensing Strategy — The Life Flow Pattern

eHealth365 implements a 24-hour sensing schedule called "Life Flow" that
balances data completeness with battery life:

| Time Period    | Ring Activity              | Patch Activity             |
|----------------|---------------------------|---------------------------|
| Morning        | HR, temp, HRV baseline    | Glucose, hydration check  |
| Active/Work    | Steps, stress monitoring  | Glucose every 15 min      |
| Exercise       | Continuous HR, SpO2       | Lactate, electrolytes     |
| Meals          | (App: food camera)        | Post-meal glucose spike   |
| Evening        | Stress recovery, ketones  | Electrolyte summary       |
| Sleep          | Sleep staging, HR, SpO2   | Overnight glucose trend   |

---

## 25.9 Data Architecture

```
Sensors --> Edge Processing (on-device) --> BLE --> Phone App --> Cloud (optional)
```

All health data is processed on-device first. The phone app stores data
locally with optional cloud sync for:
- Multi-device access
- Doctor sharing portal
- Long-term trend analysis (AI-powered)
- Emergency alert routing

---

## 25.10 Pricing and Go-To-Market

| Item                        | Price          |
|-----------------------------|----------------|
| Smart Ring Pro              | $299           |
| Smart Patch Pro starter kit | $199           |
| Weekly patch refills        | $15/week       |
| Monthly blood cartridge     | $25/month      |
| App subscription            | $9.99/month    |
| **Total first year**        | **~$1,100**    |

### Phased Rollout

| Phase        | Timeline | Scope                                      |
|--------------|----------|--------------------------------------------|
| **MVP**      | Year 1-2 | Ring + patch with HR, sleep, glucose        |
| **Growth**   | Year 2-3 | Breath analyzer, food AI, electrolytes      |
| **Complete** | Year 4-5 | Blood cartridge, full vitamin/mineral panel |

---

## 25.11 EoS Integration

Both devices run EoS firmware with the `wearable` product profile:

```bash
cmake -B build -DEOS_PRODUCT=wearable \
  -DCMAKE_TOOLCHAIN_FILE=toolchains/arm-none-eabi.cmake
```

The firmware leverages:
- **EoS HAL** — sensor drivers (PPG, ADC, I2C, SPI)
- **EoS Kernel** — task scheduling, power management
- **eBoot** — secure boot and OTA firmware updates
- **eAI (Min)** — on-device health scoring algorithms

---

## 25.12 Summary

eHealth365 demonstrates how EoS-powered wearable devices can provide
comprehensive health monitoring through a carefully designed two-device
ecosystem.

**Key takeaways:**

- Two devices cover ~90% of health metrics
- Smart Ring Pro for autonomic/movement monitoring (24/7 wear)
- Smart Patch Pro for blood/fluid chemistry (weekly replacement)
- Monthly blood cartridge for vitamin and mineral panels
- AI-powered food camera for nutrition tracking
- EoS firmware with wearable product profile

---

*Next: Chapter 26 — ePAM Personal Air Mobility*
