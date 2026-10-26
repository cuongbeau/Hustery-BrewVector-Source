# Upstream provenance

- Project: https://github.com/jniebuhr/gaggimate
- Revision: `61cacea506cdc9f8cf5032089a92ba4dd5d4a24a`
- Source paths:
  - `lib/NayrodPID/src/PressureController/`
  - `lib/NayrodPID/src/SimpleKalmanFilter/`
  - `lib/NayrodPID/src/TwoStateKalmanFilter/` (added 2026-09-05)
- License: CC BY-NC-SA 4.0, as declared by the upstream project.

The executable control implementation is vendored so local integration stays
reviewable. BrewVector-specific calibration, profile transport,
telemetry and hardware adaptation remain outside this directory.

Pressure milestone 2: TwoStateKalmanFilter files are byte-identical to the
local upstream reference in `.codex_research_control/gaggimate_latest/`.
SHA256 header: `F6D2F31FFE06ECA47AA0DBBDC3F98164CD4EA8747400337B2640F7243C41ACF6`;
source: `7439BC13CD1944F2E2454F513A1F3A5C64EC77AA38B41B778512C6DC845CA107`.
`PressureController.h` has one local read-only `getFilteredPressure()` accessor;
controller calculations are unchanged.
