# ATS Mini Encoder + OTA Catalog Design

**Date:** 2026-09-11
**Status:** Approved design, awaiting implementation plan
**Scope:** Launcher `ats-mini` board env only. No product source changes in this task.

## 1. Problem statement

Two follow-ups block the ATS Mini Launcher port from being usable and distributable:

1. **Encoder input is wrong.** One physical detent of the rotary encoder does not
   produce one launcher navigation step, and the active-low encoder push button on
   GPIO21 does not reliably emit Select. Navigation feels broken.
2. **No public OTA path exists for either firmware family.** The device can query
   LauncherHub, but there is no registered catalog offering the ATS Mini Original
   firmware and the Bruce ATS Mini firmware as versioned, no-auth downloads backed
   by public GitHub releases.

## 2. Verified current architecture

All facts below were verified against the tree and the GitHub API on 2026-09-11.

- Encoder wiring: A=GPIO2, B=GPIO1, push=GPIO21 active-low, no dedicated Esc button
  (`Launcher/boards/ats-mini/interface.cpp`, `encoderCfg()`).
- `DeviceEncoder.pullup` defaults to `false` (`Launcher/src/hal/device.h`); the ATS Mini
  `encoderCfg()` does not override it, so GPIO21 currently floats without the internal
  pull-up (`Launcher/src/hal/inputs/encoder.cpp`, `hal_encoder_init`).
- The board calls `hal_encoder_init(encoderCfg(), EncoderLatchMode::TWO03)`. TWO03 latches
  at quadrature states 0 and 3, yielding **two logical positions per physical notch**;
  FOUR3 is the one-notch/one-step mode. The HAL maps all three modes through to the
  `mathertel/RotaryEncoder` library (`toLibMode`).
- Short press on GPIO21 raises `SelPress` via `hal_encoder_poll`; long press (> 600 ms)
  is detected board-side in `InputHandler()` and raises `EscPress` while consuming the
  Select, since `pin_esc == -1` means the HAL never sets `EscPress` itself.
- OTA path is LauncherHub-authoritative, not GitHub-direct:
  `GetJsonFromLauncherHub()` builds
  `https://api.launcherhub.net/firmwares?category=` + `OTA_TAG` + `OTA_EXTRA`
  (`Launcher/src/onlineLauncher.cpp:698`), with `OTA_TAG="ats-mini"` and
  `OTA_EXTRA="esp=S3"` (`Launcher/boards/ats-mini/platformio.ini`).
  `installFirmwareFromManifest(fid, version, installedName)` consumes LauncherHub
  manifests and installs merged or split images.
- Upstream original repo `esp32-si4732/ats-mini` is **public**; latest release `v2.38`
  (verified via `gh api`) ships `ats-mini-v2.38-ospi.zip`, `ats-mini-v2.38-qspi.zip`,
  and `ats-mini-v2.38-lilygo-t-embed.zip`. Each zip contains a merged image plus split
  images.
- `wendells01/bruce-ats-mini` is **private** with **zero releases** (verified via
  `gh api`). Its README documents the intended USB-flash asset name
  `Bruce-ats-mini-vX.Y.Z.bin`.
- Local build is proven (`pio run -e ats-mini` succeeds); all physical
  encoder/OTA verification remains pending hardware.

## 3. Encoder fix design

Two one-line changes in `Launcher/boards/ats-mini/interface.cpp`, no HAL changes:

1. **Latch mode TWO03 to FOUR3.**
   `hal_encoder_init(encoderCfg(), EncoderLatchMode::FOUR3)` so one physical detent
   emits exactly one `PrevPress`/`NextPress` step.
2. **Enable the GPIO21 internal pull-up.**
   Set `cfg.pullup = true` in `encoderCfg()` so the active-low push button idles HIGH
   and reliably pulls LOW, producing `SelPress`/Enter on short press.

Preserved behavior:

- Short press on GPIO21 = Select/Enter (`SelPress`).
- Long press on GPIO21 (> 600 ms hold) = Esc/back (`EscPress`, Select consumed).
  Threshold and consume logic in `InputHandler()` are unchanged.
- `pin_esc` stays `-1`; no dedicated Esc button exists on this hardware.
- Deep-sleep wake on GPIO21 LOW (`powerOff()`) and the > 2 s hold-to-power-off
  path (`checkReboot()`) are untouched but must be re-verified after the pull-up
  change, since idle-level behavior on GPIO21 changes.

## 4. OTA / release / catalog design

### 4.1 Architecture

GitHub public releases are the **source of artifacts**; the device keeps using the
**existing LauncherHub API/manifest/install path** and never calls the GitHub API
directly.

- LauncherHub remains responsible for manifest metadata, version listing, and
  install layout (merged vs split sources per manifest).
- The device flow is unchanged: `OTA_TAG`/`OTA_EXTRA` query
  `api.launcherhub.net/firmwares`, then `installFirmwareFromManifest()` installs.
- Direct GitHub API polling from the ESP32 is **rejected**: it would add rate-limit
  handling, JSON/schema drift against the GitHub API, and on-device manifest
  generation, all of which LauncherHub already absorbs server-side.

### 4.2 Upstream original firmware (`esp32-si4732/ats-mini`)

- Repo is already public with versioned releases; no visibility work needed.
- ATS Mini consumes the **OSPI merged image** (`ats-mini-vX.YY-ospi.zip`) unless a
  later hardware verification proves a different package matches the board.
- Upstream asset naming stays as-is; the catalog references whatever the release
  publishes. Do not ask upstream to rename.

### 4.3 Bruce firmware (`wendells01/bruce-ats-mini`)

 Preconditions before catalog registration:

1. Make the repo **public** (Actions minutes and release downloads both depend on it).
2. Publish **versioned releases** containing an ATS Mini merged binary per release.
3. No private authentication anywhere on the device path: public release asset URLs
   plus the existing LauncherHub query/install flow.

### 4.4 Asset naming contract

- Original (upstream-owned, reference only): `ats-mini-vX.YY-ospi.zip`
  (also `-qspi.zip`, `-lilygo-t-embed.zip` exist; the catalog pins OSPI).
- Bruce (this project owns): `Bruce-ats-mini-vX.Y.Z-merged.bin`, matching the name
  already documented in the Bruce README flashing instructions.
- Future releases must use stable, non-expiring **GitHub Release assets** (older
  Launcher CI artifacts expired after 5 days, so CI artifact links are not an
  acceptable distribution channel). Minimum bar: 30-day retention or permanent
  release assets; permanent release assets preferred.

### 4.5 Catalog entries

Two user-facing firmware families under the existing `ats-mini` + `esp=S3` query
surface:

| Family            | Source repo               | Artifact consumed     |
|-------------------|---------------------------|-----------------------|
| ATS Mini Original | `esp32-si4732/ats-mini`   | OSPI merged image     |
| Bruce ATS Mini    | `wendells01/bruce-ats-mini` | `Bruce-ats-mini-vX.Y.Z-merged.bin` |

LauncherHub owns the manifest metadata, version listing, and install layout for
both entries. Registering the Bruce entry is blocked on Section 4.3; registering
the Original entry is blocked only on confirming the OSPI merged image against
hardware.

## 5. Release workflow requirements

1. Bruce repo goes public; no device-side tokens or private-release handling.
2. Tag-driven releases (`vX.Y.Z`) build the merged binary in CI and attach
   `Bruce-ats-mini-vX.Y.Z-merged.bin` as a permanent release asset.
3. Each release keeps the asset available indefinitely (no expiry-based links).
4. LauncherHub catalog entries point at the release assets via manifests; the
   device needs no firmware change to pick up new versions beyond its normal
   LauncherHub query.
5. Original-firmware entry tracks upstream `esp32-si4732/ats-mini` releases;
   no fork or re-host of upstream binaries.

## 6. Considered approaches and trade-offs

### Option A (recommended): LauncherHub-backed catalog

Device keeps querying LauncherHub; releases live on public GitHub; LauncherHub
maps them into manifests. Minimal device change (zero OTA code changes), no auth
on device, version listing and merged/split layout handled server-side.
Trade-off: catalog registration depends on the LauncherHub operator accepting both
entries.

### Option B (rejected): ESP32 polls the GitHub Releases API directly

Removes the LauncherHub dependency but forces the firmware to handle GitHub
rate limits, paginated JSON, schema drift, and manifest synthesis in RAM on an
S3 already at tight margins. Rejected for complexity with no user-visible gain.

### Option C (rejected): sideload-only distribution (USB/esptool, no OTA entry)

Zero catalog work and matches the current Bruce README flow, but leaves the
Launcher OTA menu empty for ATS Mini users and splits the two firmware families
across unrelated download pages. Rejected as the steady state; USB flash remains
only as the fallback path.

## 7. Failure handling

- Encoder still double-steps after FOUR3: suspect encoder variant with different
  detent phasing; fall back to FOUR0 or swap A/B in `encoderCfg()` and re-test on
  hardware. Do not change the HAL.
- GPIO21 floats or double-fires Select: confirm `pullup=true` is actually applied
  (scope the pin idle level); external pull-up is the fallback, not a code change.
- Long-press Esc misfires on short presses: re-tune the 600 ms threshold; keep the
  consume-Select invariant.
- Upstream renames or drops the OSPI package: catalog pins the new name after
  hardware verification; device code unchanged.
- Bruce release asset missing at install time: LauncherHub manifest must fail
  closed with the existing fetch/install error surfacing; no partial flash.
- LauncherHub entry rejected or delayed: USB/esptool flash of the release asset
  is the interim path for both families.

## 8. Rollout order

1. Land the two-line encoder fix; verify on hardware (Section 9).
2. Make `bruce-ats-mini` public; cut a versioned release with the merged binary.
3. Confirm the OSPI merged image against ATS Mini hardware.
4. Register both catalog entries in LauncherHub.
5. Verify end-to-end OTA install of each family on hardware, including no-auth
   download.

## 9. Test / verification criteria

- [ ] **Encoder steps (pending hardware):** one physical detent moves exactly one
      launcher menu row, in both directions, across slow and fast rotation.
- [ ] **Short press (pending hardware):** GPIO21 tap emits Select/Enter with no
      ghost Esc.
- [ ] **Long press (pending hardware):** > 600 ms hold emits Esc/back exactly once
      and does not also emit Select.
- [ ] **Wake/power paths (pending hardware):** deep-sleep wake on GPIO21 LOW and
      > 2 s hold-to-power-off still work with the internal pull-up enabled.
- [ ] **Public release assets:** upstream `v2.38` zips downloadable without auth;
      Bruce release exposes `Bruce-ats-mini-vX.Y.Z-merged.bin` without auth.
- [ ] **Manifest install:** each catalog entry installs to completion via the
      existing `installFirmwareFromManifest` path (merged image; split tolerated
      where the manifest declares it).
- [ ] **No-auth download:** full OTA flow on device with no tokens configured.
- [ ] **Build gate:** `pio run -e ats-mini` still succeeds after the fix.

## 10. Non-goals

- No product source, workflow, visibility, release, or LauncherHub data changes.
- No new dependencies.
- No on-device GitHub API client.
- No renaming of upstream `esp32-si4732/ats-mini` assets.
- No claim of hardware validation; all physical tests are marked pending above.
- This spec does not mark the implementation complete; a separate implementation
  plan follows after review/approval.
