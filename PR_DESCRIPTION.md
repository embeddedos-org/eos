# Fix GPRMC Parsing of Empty NMEA Fields

## 1. Issue

In `services/gps/src/gps_nmea.c`, `eos_gps_parse_gprmc()` parses NMEA 0183 `$GPRMC` GPS sentences by splitting sentence strings with `strtok(buf, ",")`.

In the NMEA 0183 specification:
* Fields are positional and comma-delimited.
* Unacquired or omitted fields are represented by adjacent commas (e.g. `,,`).
* Standard Void (no-fix) sentences (such as `$GPRMC,083559.00,V,,,,,,,270526,,,N*4D`) contain consecutive empty commas between status `V` and date `270526`.
* Active-fix sentences (`A`) often omit optional fields such as speed over ground, course, or magnetic variation (e.g. `$GPRMC,092750.000,A,3746.4949,N,12225.1644,W,,,270526,,,A*7B`).

Because standard C `strtok()` collapses consecutive delimiter characters into a single delimiter:
1. Empty fields are skipped entirely, destroying positional field alignments.
2. For Void status sentences (`V`), `tok_cnt` drops below 8, causing `eos_gps_parse_gprmc()` to falsely return an error code (`-2`) rather than setting `data->fix_valid = false` and returning `0`.
3. For Active status sentences with missing speed/course fields, subsequent tokens (such as the date string) shift into earlier token slots, corrupting telemetry (e.g. parsing date `"270526"` into `speed_knots` as `270526.0` knots).
4. Calling `eos_gps_parse_gprmc()` with `NULL` for `nmea` or `data` causes a segmentation fault due to a missing NULL check before `strncmp()`.

## 2. Reproduction

1. Pass a valid NMEA Void-fix sentence:
   `$GPRMC,083559.00,V,,,,,,,270526,,,N*4D`
   **Before fix:** `strtok` collapses `,,,,,,,`, producing only 5 tokens. The function rejects the sentence with `-2` instead of returning `0` with `fix_valid = false`.

2. Pass an active fix with omitted speed/course fields:
   `$GPRMC,092750.000,A,3746.4949,N,12225.1644,W,,,270526,,,A*7B`
   **Before fix:** `tokens[7]` receives `"270526"` instead of an empty string, resulting in `speed_knots = 270526.0`.

## 3. Proposed Approach & Implementation

1. **Positional Tokenization**: Replaced `strtok()` in `services/gps/src/gps_nmea.c` with safe comma-scanning using `strchr()`. This ensures consecutive delimiters produce empty string tokens (`""`), preserving exact field positions.
2. **Safe Input Validation**: Added NULL pointer checks for `nmea` and `data` at the start of `eos_gps_parse_gprmc()`.
3. **Empty Numeric Field Handling**: Guarded conversions for latitude, longitude, and speed so that empty fields fall back to `0.0`.
4. **Void Status Handling**: Ensured that sentences with status `V` set `fix_valid = false` and return `0`.

## 4. Testing & Validation

### Regression Tests Added:
* `test_nmea_gprmc_void_fix_empty_fields`: Validates parsing of standard void fix sentences with consecutive empty commas.
* `test_nmea_gprmc_empty_speed_field`: Validates parsing of active fix sentences where optional speed and course fields are omitted.
* `test_nmea_gprmc_invalid_sentence`: Validates handling of NULL pointers, non-GPRMC sentences, and truncated sentences.

### Test Results:
```text
Targeted tests: 4/4 passed
Relevant test suite: 4/4 passed
Full test suite: 10/10 passed
```

Execution output:
```text
tests/unit/test_gps_nmea.py::TestEoSGPSNMEA::test_nmea_gprmc_empty_speed_field PASSED
tests/unit/test_gps_nmea.py::TestEoSGPSNMEA::test_nmea_gprmc_invalid_sentence PASSED
tests/unit/test_gps_nmea.py::TestEoSGPSNMEA::test_nmea_gprmc_parsing PASSED
tests/unit/test_gps_nmea.py::TestEoSGPSNMEA::test_nmea_gprmc_void_fix_empty_fields PASSED
tests/unit/test_unit_core.py::TestEoSUnit::test_mutex_priority_inheritance PASSED
tests/unit/test_unit_core.py::TestEoSUnit::test_task_scheduling PASSED
tests/functional/test_functional_e2e.py::TestEoSFunctional::test_rtos_kernel_pipeline PASSED
tests/functional/test_gps_driver.py::TestEosGpsDriver::test_nmea_gga_parsing PASSED
tests/performance/test_performance_benchmarks.py::TestEoSPerformance::test_context_switch_latency PASSED
tests/simulation/test_emulation_simulation.py::TestEoSSimulation::test_hardware_timer_interrupt PASSED
```

## 5. Compatibility & Considerations

* **Backwards Compatibility**: Fully backwards compatible with valid sentences containing non-empty fields.
* **Standards Compliance**: Compliant with NMEA 0183 comma delimiter specifications for field positioning.
* **Assumptions & Scope**: Checksum validation and full protocol framing remain handled by the receiver transport layer before passing the sentence payload to `eos_gps_parse_gprmc()`.

## 6. Summary

Fixed NMEA `$GPRMC` sentence parsing in `services/gps/src/gps_nmea.c` by replacing `strtok()` with index-preserving comma tokenization, preventing token shifts on sentences containing empty fields.

