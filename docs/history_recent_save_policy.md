# Ten recent shots and save filter

Implemented in source following user confirmation: show only ten recent shots;
save new shots only when duration is strictly greater than 10 seconds and peak
filtered pressure is at least 1 bar. Duration and pressure must be finite, and
there must be at least two trace samples. Exactly 10 seconds is rejected;
exactly 1 bar is accepted when the duration condition also passes.

The trace writer evaluates the rule at finish, using stop-event elapsed time
when available, otherwise the last trace elapsed time. It closes and removes
the current rejected trace without appending a summary or advancing latest saved
shot. Failure to remove is reported as FILTER REMOVE ERROR. Acquisition still
writes while a shot runs to preserve its complete trace if it qualifies later;
this is a retention filter, not elimination of all temporary TF writes. A power
loss before finalization may leave an unindexed file. The legacy ShotStore save
entry point applies the same rule before allocating an ID or writing a CSV.

Existing CSVs and indexes are not rewritten or deleted. Existing indexed shots
can therefore still appear in the ten recent entries even when they do not meet
the new rule. IDs may have gaps because rejected trace IDs are not reused.

`/traces/list` now returns up to ten summaries from the storage worker's published
RAM snapshot on the owner loop. It performs no directory scan or CSV reads and
does not queue behind Profile jobs. Fields include duration, peak, sample count,
result flags and valid stop yield. Byte size is no longer needed for the list.
The existing index is read by the storage worker at startup; absent/incomplete
indexes are not silently rebuilt by this endpoint. The 40-entry native HMI
snapshot capacity is unchanged; only the Web response is limited to ten.

Web retains the list and selected loaded shot across tab changes. Refresh is
explicit. It downloads no CSV until a shot is selected; switching away cancels
an unfinished CSV request, and selecting the shot again can restart it. Old
archives remain on TF, but this Web view is intentionally limited to ten entries.

Checks passed: C++11 compile-time boundary assertions, ten newest entries,
cached navigation, explicit refresh, no automatic detail request, cancellation,
HTML escaping, UTF-8 chunk transport, existing Web job/timeout behavior, and
packaged root JavaScript parsing. Run:

```
powershell -NoProfile -ExecutionPolicy Bypass -File tests/heater/check_history.ps1
```

The remaining Web findings were also addressed: the PID link is now in the
navigation row instead of overlaying Save; startup honors #history once; History
parses the final stop metadata and uses stop elapsed time and valid physical-scale
yield. Legacy CSVs without a valid stop record show unavailable final yield,
rather than treating a periodic sample or disconnected zero as a final weight.
Tests cover differing last-sample/stop values, offline scale, legacy CSV and
History/Dashboard startup selection.

The gzip Web asset was regenerated. This change has not yet been built into
firmware or uploaded; physical TF retention, response-time measurements and
updated visual layout remain unverified on the device.
