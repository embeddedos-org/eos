# Appendix B: Product Profiles

*Author: Srikanth Patchava & EmbeddedOS Contributors*

---

## All EoS Product Profiles

EoS includes 48 product profile headers in the `products/` directory. Each
profile enables the specific HAL peripherals, kernel features, and services
needed for that product category.

| #  | Profile          | Header File        | Target Domain                     |
|----|------------------|--------------------|-----------------------------------|
| 1  | adapter          | `adapter.h`        | Protocol adapter/bridge           |
| 2  | aerospace        | `aerospace.h`      | Aerospace flight systems          |
| 3  | ai_edge          | `ai_edge.h`        | AI edge inference device          |
| 4  | automotive       | `automotive.h`     | Automotive ECU                    |
| 5  | autonomous       | `autonomous.h`     | Autonomous vehicle/robot          |
| 6  | banking          | `banking.h`        | Banking/fintech terminal          |
| 7  | cast_device      | `cast_device.h`    | Media casting device              |
| 8  | cockpit          | `cockpit.h`        | Vehicle cockpit/dashboard         |
| 9  | computer         | `computer.h`       | General-purpose computer          |
| 10 | crypto_hw        | `crypto_hw.h`      | Hardware security module          |
| 11 | desktop          | `desktop.h`        | Desktop computer                  |
| 12 | diagnostic       | `diagnostic.h`     | Diagnostic/test equipment         |
| 13 | drone            | `drone.h`          | UAV/drone flight controller       |
| 14 | ev               | `ev.h`             | Electric vehicle                  |
| 15 | fitness          | `fitness.h`        | Fitness tracker                   |
| 16 | gaming           | `gaming.h`         | Gaming console/handheld           |
| 17 | gateway          | `gateway.h`        | IoT gateway                       |
| 18 | ground_control   | `ground_control.h` | Ground control station            |
| 19 | hmi              | `hmi.h`            | Human-machine interface           |
| 20 | home_camera      | `home_camera.h`    | Smart home camera                 |
| 21 | industrial       | `industrial.h`     | Industrial controller             |
| 22 | infotainment     | `infotainment.h`   | Vehicle infotainment              |
| 23 | iot              | `iot.h`            | IoT sensor node                   |
| 24 | iptv_stb         | `iptv_stb.h`       | IPTV set-top box                  |
| 25 | medical          | `medical.h`        | Medical device                    |
| 26 | mobile           | `mobile.h`         | Mobile/smartphone                 |
| 27 | plc              | `plc.h`            | Programmable logic controller     |
| 28 | pos              | `pos.h`            | Point-of-sale terminal            |
| 29 | printer          | `printer.h`        | Printer controller                |
| 30 | robot            | `robot.h`          | Robotics platform                 |
| 31 | router           | `router.h`         | Network router                    |
| 32 | satellite        | `satellite.h`      | Satellite system                  |
| 33 | security_cam     | `security_cam.h`   | Security camera                   |
| 34 | server           | `server.h`         | Edge server                       |
| 35 | smart_home       | `smart_home.h`     | Smart home hub                    |
| 36 | smart_speaker    | `smart_speaker.h`  | Smart speaker                     |
| 37 | smart_tv         | `smart_tv.h`       | Smart TV                          |
| 38 | space_comm       | `space_comm.h`     | Space communication system        |
| 39 | telecom          | `telecom.h`        | Telecom equipment                 |
| 40 | telemedicine     | `telemedicine.h`   | Telemedicine device               |
| 41 | thermostat       | `thermostat.h`     | Smart thermostat                  |
| 42 | tv_os            | `tv_os.h`          | TV operating system               |
| 43 | vacuum           | `vacuum.h`         | Robotic vacuum cleaner            |
| 44 | vbox_test        | `vbox_test.h`      | VirtualBox test environment       |
| 45 | voice            | `voice.h`          | Voice assistant device            |
| 46 | watch            | `watch.h`          | Smartwatch                        |
| 47 | wearable         | `wearable.h`       | Generic wearable device           |
| 48 | xr_headset       | `xr_headset.h`     | XR/VR/AR headset                  |

---

## Usage

Select a product profile at build time:

```bash
cmake -B build -DEOS_PRODUCT=robot
cmake --build build
```

Each profile header defines `EOS_ENABLE_*` flags that control which
HAL peripherals, kernel features, and services are compiled into the
firmware.
