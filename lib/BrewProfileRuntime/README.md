# BrewProfileRuntime

Vendored from GaggiMate. Pinned revision and byte hashes: `UPSTREAM.json`. `upstream/profile.h` is an
unmodified copy. Include only `src/BrewProfileRuntime.h`: it preloads dependency
headers and wraps upstream names to avoid legacy Profile/Phase collisions.

The active Main executor now includes `src/BrewProcessRuntime.h`, an
adapted upstream BrewProcess. Originals remain under `upstream/process/`.
Local changes seed initial measurements, enforce phase timeouts and the
60-second machine limit, and preserve the commanded target on scale-loss
time fallback. Shared validation is in `protocol/profile_codec.h`.
Always parse into a fresh value-initialized Profile: upstream appends phases.

Repository root at this pinned revision contained no LICENSE-named file;
no license is invented here. Preserve source attribution and verify applicable
distribution terms before redistributing a release containing this module.
