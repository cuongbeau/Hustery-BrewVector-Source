# Web Brew: buffered measurements and independent rendering

Source updated 2026-09-11. This change has not been built into firmware, flashed,
or measured on the physical HMI. Existing Main telemetry cadence is unchanged:
50 ms during active operation, 250 ms at idle.

## Data path

`MainLink` records each accepted Brew/HotWater telemetry packet into a fixed
512-sample PSRAM ring (approximately 16 KiB, 25.6 seconds at 20 Hz). Capture does
not allocate, serialize JSON, or send extra UART requests. The ring and dashboard
handler run on the same owner context. Allocation failure retains the previous
snapshot-only Web behavior.

`GET /dash/live?boot=...&session=...&after=...` returns up to 64 pending samples,
alongside the existing live snapshot. Rows contain sequence, elapsed seconds,
pressure, target, flow, weight, estimated brew temperature, and control mode.
Each browser maintains its own cursor. `streamMore` requests further batches;
`streamGap` indicates an expired cursor. A boot identity and shot session prevent
mixing shots or data across HMI restarts. A new shot resets the ring; ending a
shot retains its measurements until the next one starts.

## Browser behavior

- Active Brew/HotWater or backlog: target 100 ms between request starts, with a
  minimum 20 ms yield after a slow response. This is a target, not a guaranteed
  network latency or sample rate. One live request may be in flight at a time.
- Idle/recovery: 250 ms after completion; hidden tabs: 5 seconds. Commands retain
  priority, abort the live read, and are never automatically repeated.
- Up to 2,400 real measurements are retained in the browser. Duplicate samples
  are discarded. Missing-buffer or invalid-sample recovery clears the chart
  segment so it does not bridge unknown data.
- Animation uses requestAnimationFrame capped around 30 draws/second. It moves
  through received samples and interpolates only the displayed leading point;
  stored measurements and summary calculations are unchanged. It never
  extrapolates future measurements. The canvas is resized only when its actual
  dimensions change. Hidden tabs stop animation.
- The result is frozen only after the last pending batch arrives. The final
  graph displays all retained samples. A stall longer than the HMI ring capacity
  cannot recover the entire shot from this live endpoint; TF history is separate.

## Verification

Run `powershell -NoProfile -ExecutionPolicy Bypass -File tests/heater/check_brew_stream.ps1`
from the project root (the execution-policy override applies only to that process).
The suite checks ring capacity/order, duplicate timestamps, session changes,
Main timestamp restart, Arduino C++11 header compatibility, batched Web merge,
invalid rows, interpolation without raw mutation or extrapolation, hidden-tab
animation, deferred final summary, 100 ms scheduling, slow responses, cursor
requests, Stop/read cancellation, no command retries, and existing heater Web
behavior. It also verifies that the packaged gzip page matches and parses the
current source. Regenerate it with `python scripts/package_hmi_web.py` after
editing Web assets.

These checks passed. Full firmware compilation and on-device Brew/Stop latency,
HTTP load, free memory, and visible smoothness remain to be verified after an
authorized build/upload. Preserve Wi-Fi modem sleep while BLE is enabled.
