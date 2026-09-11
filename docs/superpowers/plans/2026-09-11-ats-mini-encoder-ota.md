# ATS Mini Encoder + OTA Implementation Plan

**Date:** 2026-09-11
**Spec:** `docs/superpowers/specs/2026-09-11-ats-mini-encoder-ota-design.md`
**Status:** Ready for execution, planning only, no product changes made here

## Mandatory header

- **Goal:** Fix one-detent/one-step encoder navigation and reliable Enter on GPIO21 in the Launcher `ats-mini` env, make `wendells01/bruce-ats-mini` public with tag-driven permanent releases of `Bruce-ats-mini-vX.Y.Z-merged.bin`, register ATS Mini Original and Bruce ATS Mini families in the existing LauncherHub OTA catalog under `ats-mini` + `esp=S3`, and verify all of it with build plus hardware QA.
- **Architecture:** GitHub public releases are the artifact source; the ESP32-S3 device runs zero new OTA code and keeps calling `GetJsonFromLauncherHub()` in `Launcher/src/onlineLauncher.cpp` against `https://api.launcherhub.net/firmwares?category=` + `OTA_TAG` + `OTA_EXTRA`, then installs via `installFirmwareFromManifest(fid, version, installedName)` which handles merged and split layouts. Direct GitHub API polling on the ESP32 is rejected per spec section 4.1.
- **Tech stack:** PlatformIO (`pio run -e ats-mini`), Arduino core for ESP32-S3, `mathertel/RotaryEncoder @ ^1.5.3` via `hal_encoder_init`/`hal_encoder_poll`, LauncherHub REST (`api.launcherhub.net/firmwares`), GitHub Releases permanent assets, `gh api` read-only state checks, `esptool.py write_flash 0x0` USB fallback.
- **Required sub-skill instruction:** Execute workstream A with the programming skill (C++ HAL change, bite-sized diff, local build gate). Execute workstream B release verification with git-master discipline (inspect `git status`/`git diff` before each commit, atomic commits, `GIT_MASTER=1` prefix on every git command). Execute workstream D QA as hardware-gated checklist with recorded pass/fail per criterion, no claim of hardware validation without a device in hand.

## Verified starting state (2026-09-11, read-only `gh api`)

- `esp32-si4732/ats-mini` is public; release `v2.38` ships `ats-mini-v2.38-ospi.zip`, `ats-mini-v2.38-qspi.zip`, `ats-mini-v2.38-lilygo-t-embed.zip`.
- `wendells01/bruce-ats-mini` is private with 0 releases.
- `wendells01/Launcher` (fork holding `Launcher/boards/ats-mini/`) is private.
- Launcher `ats-mini` CI entry is commented out in `Launcher/.github/workflows/main.yml` line 52 (`# - { env: "ats-mini" }`); CI runners have failed with `runner_id=0`, so the local build is the authoritative gate.
- OTA path constants: `OTA_TAG="ats-mini"`, `OTA_EXTRA="esp=S3"` in `Launcher/boards/ats-mini/platformio.ini` lines 29-30; query builder `GetJsonFromLauncherHub()` at `Launcher/src/onlineLauncher.cpp:698`; installer `installFirmwareFromManifest` at `Launcher/src/onlineLauncher.cpp:749`.
- Encoder wiring: A=GPIO2, B=GPIO1, push=GPIO21 active-low, no Esc pin, in `encoderCfg()` at `Launcher/boards/ats-mini/interface.cpp:46-53`. `DeviceEncoder.pullup` defaults `false` in `Launcher/src/hal/device.h:39`. Init call `hal_encoder_init(encoderCfg(), EncoderLatchMode::TWO03)` at `interface.cpp:92`. HAL pull-up branch and latch mapping in `Launcher/src/hal/inputs/encoder.cpp:17-38`. Board long-press Esc (> 600 ms, Select consumed) in `InputHandler()` at `interface.cpp:178-198`. Wake in `powerOff()` at `interface.cpp:218-223`. Power-off hold (> 2 s) in `checkReboot()` at `interface.cpp:237-247`.

---

## Workstream A — Launcher encoder source fix (independent, no dependencies)

Owner files: `Launcher/boards/ats-mini/interface.cpp` only. No HAL changes. No `platformio.ini` changes.

### Step A1 — Enable GPIO21 internal pull-up in `encoderCfg()`

- **File/symbol:** `Launcher/boards/ats-mini/interface.cpp`, function `encoderCfg()` (lines 46-53).
- **Intended change (exact):**
  ```cpp
  static DeviceEncoder encoderCfg() {
      DeviceEncoder cfg;
      cfg.pin_a = 2;    // Encoder A
      cfg.pin_b = 1;    // Encoder B
      cfg.pin_sel = 21; // Encoder push-button (Select)
      cfg.pin_esc = -1; // No dedicated Esc button on ATS Mini
      cfg.pullup = true; // Active-low push button idles HIGH via internal pull-up
      return cfg;
  }
  ```
- **Why:** `hal_encoder_init` in `Launcher/src/hal/inputs/encoder.cpp:26-29` calls `launcherGpioInputPullup(cfg.pin_sel)` only when `cfg.pullup` is true; without it GPIO21 floats and short-press `SelPress` is unreliable. Code path that consumes this: `hal_encoder_poll` reads `sel = ... launcherGpioRead(cfg.pin_sel) == LOW` at `encoder.cpp:53` and raises `SelPress` at `encoder.cpp:69-73`.
- **Preserved:** `pin_esc` stays `-1`; HAL never sets `EscPress` itself.
- **Expected result:** GPIO21 idles HIGH, pulls LOW on press, `SelPress`/Enter fires on short tap.
- **Command:** none (edit only).
- **Commit:** `fix(ats-mini): enable encoder pullup on GPIO21` containing only `Launcher/boards/ats-mini/interface.cpp` hunk for `cfg.pullup = true`.
- **Rollback:** delete the `cfg.pullup = true;` line and rebuild.

### Step A2 — Latch mode `TWO03` to `FOUR3`

- **File/symbol:** `Launcher/boards/ats-mini/interface.cpp`, function `_setup_gpio()` line 92.
- **Intended change (exact):** replace
  ```cpp
  hal_encoder_init(encoderCfg(), EncoderLatchMode::TWO03);
  ```
  with
  ```cpp
  hal_encoder_init(encoderCfg(), EncoderLatchMode::FOUR3);
  ```
- **Why:** `TWO03` latches at quadrature states 0 and 3, yielding two logical positions per physical notch; `FOUR3` yields one step per notch. Mapping is 1:1 through `toLibMode` in `Launcher/src/hal/inputs/encoder.cpp:17-23` into `mathertel/RotaryEncoder`.
- **Preserved:** rotation direction mapping (`posDifference > 0` to `PrevPress`, `< 0` to `NextPress` in `encoder.cpp:60-67`); `InputHandler()` reset/debounce block at `interface.cpp:167-210` untouched; 600 ms threshold and `SelPress = false` consume invariant at `interface.cpp:189-193` untouched; `powerOff()` wake on GPIO21 LOW and `checkReboot()` 2000 ms hold untouched.
- **Fallback if hardware still double-steps:** keep `FOUR3` rejected only on device evidence, then try `EncoderLatchMode::FOUR0`, then swap `cfg.pin_a`/`cfg.pin_b` values in `encoderCfg()`; each fallback is a separate commit with a hardware note. Do not touch `Launcher/src/hal/inputs/encoder.cpp`.
- **Expected result:** one physical detent emits exactly one `PrevPress`/`NextPress`.
- **Command:** none (edit only).
- **Commit:** `fix(ats-mini): one detent one step with FOUR3 latch` containing only the line-92 hunk. If A1 and A2 land in sequence, squash into a single atomic commit `fix(ats-mini): encoder one-step navigation and reliable Enter` since both hunks are the same file and the same logical fix; two commits are acceptable only when hardware evidence forces separate bisection.
- **Rollback:** revert line 92 to `EncoderLatchMode::TWO03` and rebuild.

### Step A3 — Local build gate (authoritative; CI is not trusted here)

- **Command (exact):** `cd Launcher && /tmp/pio-venv/bin/pio run -e ats-mini`
- **Expected result:** `SUCCESS`, output binary at the repo root via `merge.py` as `Launcher-ats-mini.bin` (prior proven run: RAM 25.8%, Flash 8.1%, ~2 min warm). If the venv is missing, recreate with `python3 -m venv /tmp/pio-venv && /tmp/pio-venv/bin/pip install platformio` then rerun the same build command.
- **Diagnostics gate:** `lsp_diagnostics` clean on `Launcher/boards/ats-mini/interface.cpp` (no new warnings).
- **Commit:** none (verification only). Record the SUCCESS line and RAM/Flash percentages in the QA log (Step D1).

### Workstream A blockers

- Hardware detent phasing unknown until Step D2 runs on a physical ATS Mini; code ships after build gate regardless, hardware verdict stays open.
- No external credential blocks this workstream.

---

## Workstream B — Bruce repo public plus tag-driven release workflow (precedes Bruce catalog entry)

All Bruce paths below are relative to the `wendells01/bruce-ats-mini` checkout (sibling `Bruce/` dir in this workspace).

### Step B1 — Make `wendells01/bruce-ats-mini` public

- **Action (website or CLI, operator decision recorded at execution time):** Settings page visibility change to public, or `gh repo edit wendells01/bruce-ats-mini --visibility public --accept-visibility-change-consequences`.
- **Why:** private repos on the free plan exhaust Actions minutes with the full board matrix (observed `runner_id=0` failures), and release downloads require auth while private; public removes both blocks.
- **Expected result:** `gh api repos/wendells01/bruce-ats-mini --jq '.private'` prints `false`.
- **No device credentials:** nothing token-based is added to firmware; downloads are anonymous HTTPS.
- **Rollback:** `gh repo edit wendells01/bruce-ats-mini --visibility private`; note this re-breaks no-auth downloads and is a deliberate regression, recorded in the rollout log.
- **Blocker:** requires repo admin rights on `wendells01/bruce-ats-mini`. Without admin, stop and record `BLOCKED: bruce-ats-mini visibility change needs owner admin`.

### Step B2 — Verify the tag-driven release workflow builds the ATS Mini merged binary as a permanent asset

- **Workflow file:** `Bruce/.github/workflows/buil_parallel.yml` (trigger `tags: "*"` at lines 25-26; `create_release` job at lines 310-368 attaches `Bruce-*.bin` via `softprops/action-gh-release@v1` with `tag_name: ${{ steps.bruce_version.outputs.version }}`).
- **Board env:** `[env:ats-mini]` in `Bruce/boards/ats-mini/ats-mini.ini` (line 1), pulled into the build via `extra_configs` in `Bruce/platformio.ini:86`.
- **Asset contract (exact):** each `vX.Y.Z` tag release attaches `Bruce-ats-mini-vX.Y.Z-merged.bin` as a permanent GitHub Release asset. This name matches the flashing instructions already documented in the Bruce README (`esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 460800 write_flash 0x0 Bruce-ats-mini-vX.Y.Z.bin`).
- **Trigger (exact):** `git tag vX.Y.Z && git push origin vX.Y.Z`, then `gh run watch`. If the workflow file needs an asset-rename step to guarantee the `-merged.bin` suffix, that edit is a separate commit in the Bruce repo touching only `Bruce/.github/workflows/buil_parallel.yml`, with commit message `ci(bruce-ats-mini): attach merged binary as Bruce-ats-mini-vX.Y.Z-merged.bin on vX.Y.Z tags`.
- **Expected result:** release page `https://github.com/wendells01/bruce-ats-mini/releases/tag/vX.Y.Z` lists `Bruce-ats-mini-vX.Y.Z-merged.bin`; direct download URL `https://github.com/wendells01/bruce-ats-mini/releases/download/vX.Y.Z/Bruce-ats-mini-vX.Y.Z-merged.bin` returns 200 without auth (`curl -fsSL -o /dev/null -w "%{http_code}" <url>` prints `200`).
- **Retention rule:** release assets are permanent; CI artifact links (5-day retention observed) are never used as the distribution URL.
- **Rollback:** delete the bad release (`gh release delete vX.Y.Z --yes`) and the tag (`git push --delete origin vX.Y.Z && git tag -d vX.Y.Z`), then re-tag after fixing the workflow.
- **Blocker:** requires Actions to run (public visibility from B1, or restored minutes). Without green CI, stop and record `BLOCKED: Bruce release CI not running`.

### Step B3 — Record the original upstream asset contract (reference only, no fork)

- **Source:** `esp32-si4732/ats-mini` release `v2.38`, asset `ats-mini-v2.38-ospi.zip` (contains merged image plus split images).
- **Rule:** catalog pins the OSPI package unless hardware verification in Step D5 proves a different package matches the board. Never ask upstream to rename; never re-host upstream binaries.
- **Expected result:** `gh api repos/esp32-si4732/ats-mini/releases/latest --jq '{tag: .tag_name, assets: [.assets[].name]}'` shows `v2.38` with the three zips.

### Workstream B commits (Bruce repo only, git-master discipline)

1. `ci(bruce-ats-mini): attach merged binary on vX.Y.Z tags` (only if the workflow edit is needed; `Bruce/.github/workflows/buil_parallel.yml` only).
2. Release commit is created by the workflow itself (`Bruce Release vX.Y.Z`); no manual binary commits.

---

## Workstream C — LauncherHub OTA catalog registration (externally blocked until credentials exist)

Device path is frozen: `OTA_TAG` `ats-mini` + `OTA_EXTRA` `esp=S3` query `https://api.launcherhub.net/firmwares`, install via `installFirmwareFromManifest` (merged path `downloadSplitFirmware` mergedFirmware branch at `Launcher/src/onlineLauncher.cpp:1226-1246`, split path at lines 1247-1270, direct-install path `installFirmwareDynamic` at lines 480-597).

### Step C1 — Register catalog entry: ATS Mini Original

- **Family:** ATS Mini Original.
- **Source repo:** `esp32-si4732/ats-mini`.
- **Artifact consumed:** OSPI merged image inside `ats-mini-vX.YY-ospi.zip`.
- **Query surface:** existing `ats-mini` + `esp=S3`; no firmware change, no new `OTA_TAG`.
- **Manifest requirement:** LauncherHub manifest declares merged layout (factory image at `0x0`, optional data image appended at the offset from the embedded partition table at `0x8000`, resolved by `firstDataPartitionOffsetFromTable` at `onlineLauncher.cpp:1160-1170`); split tolerated where the manifest declares `bootloader`/`partitions`/`firmware` sources.
- **Expected result:** `https://api.launcherhub.net/firmwares?category=ats-mini&esp=S3` response lists the Original family with a version matching upstream `v2.38`.
- **Blocker:** `BLOCKED: LauncherHub registration needs authenticated maintainer or API access; no undocumented mutation endpoint is invented in this plan`. Interim path is USB/esptool flash of the OSPI merged image.

### Step C2 — Register catalog entry: Bruce ATS Mini (depends on B1 plus B2)

- **Family:** Bruce ATS Mini.
- **Source repo:** `wendells01/bruce-ats-mini` (must already be public per B1).
- **Artifact consumed:** `Bruce-ats-mini-vX.Y.Z-merged.bin` (must already exist as a permanent release asset per B2).
- **Query surface:** same existing `ats-mini` + `esp=S3`.
- **Manifest requirement:** merged layout pointing at the public release asset URL; device needs no change beyond its normal LauncherHub query.
- **Expected result:** same firmwares query lists the Bruce family with the tagged version; full OTA flow runs with no tokens configured.
- **Blocker:** same `BLOCKED` as C1, plus hard dependency on B1/B2 completion. Interim path is USB/esptool flash: `esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 460800 write_flash 0x0 Bruce-ats-mini-vX.Y.Z-merged.bin`.

### Workstream C commits

- None in this repo. Catalog data lives in LauncherHub. If the operator provides a documented submission flow (PR, form, API call with credentials), record the exact request and response in the rollout log; do not improvise endpoints.

---

## Workstream D — QA and rollout (hardware-gated)

### Step D1 — Build gate (no hardware)

- **Command:** `cd Launcher && /tmp/pio-venv/bin/pio run -e ats-mini`
- **Expected result:** SUCCESS with recorded RAM/Flash percentages; `lsp_diagnostics` clean on changed files.
- **Record:** paste the SUCCESS tail lines into the rollout log.

### Step D2 — Encoder rotation (hardware, new binary with A1 plus A2)

- **Procedure:** flash the Step A3 binary via USB; open the Launcher menu; rotate one physical detent clockwise, observe one row move; rotate one detent counterclockwise, observe one row return; repeat 10 detents each way at slow speed, then 10 each way at fast speed.
- **Expected result:** 1 detent = 1 row every time, both directions, both speeds. No double-step, no direction inversion.
- **On failure:** try `FOUR0` first, then A/B swap, per A2 fallback; each attempt is a new binary and a new log line.

### Step D3 — Short press Enter (hardware)

- **Procedure:** tap GPIO21 push button briefly (< 600 ms) on a menu row.
- **Expected result:** `SelPress`/Enter fires once, no ghost `EscPress`, row activates.

### Step D4 — Long press Esc (hardware)

- **Procedure:** hold GPIO21 longer than 600 ms.
- **Expected result:** `EscPress`/back fires exactly once and no Select activates (consume invariant from `interface.cpp:189-193`).

### Step D5 — Wake and power-off paths (hardware)

- **Procedure:** enter deep-sleep via the OFF menu, press GPIO21, observe wake; then hold GPIO21 longer than 2 s via `checkReboot()` (`interface.cpp:237-247`), observe `powerOff()` deep-sleep entry.
- **Expected result:** wake on LOW works; > 2 s hold powers off; no stuck wake loop with the pull-up enabled.

### Step D6 — Public no-auth downloads (no hardware)

- **Commands:**
  - `curl -fsSL -o /dev/null -w "%{http_code}\n" https://github.com/esp32-si4732/ats-mini/releases/download/v2.38/ats-mini-v2.38-ospi.zip`
  - `curl -fsSL -o /dev/null -w "%{http_code}\n" https://github.com/wendells01/bruce-ats-mini/releases/download/vX.Y.Z/Bruce-ats-mini-vX.Y.Z-merged.bin`
- **Expected result:** both print `200` with no auth header.

### Step D7 — Manifest install on device (hardware, depends on C1/C2)

- **Procedure:** on ATS Mini with working WiFi, open Launcher OTA list (query `ats-mini` + `esp=S3`), install ATS Mini Original, confirm boot; reinstall Launcher, install Bruce ATS Mini, confirm boot.
- **Expected result:** each family installs to completion via `installFirmwareFromManifest` (merged image; split tolerated where declared), no partial flash (fail-closed with existing fetch/install error surfacing per spec section 7).

### Rollout order (spec section 8, concrete)

1. Land A1 plus A2, pass D1, flash, pass D2 through D5.
2. Complete B1, cut B2 release, pass D6 for the Bruce asset.
3. Confirm OSPI package against hardware (D7 Original leg).
4. Complete C1 plus C2 (blocked until maintainer/API access exists; USB flash is the interim path).
5. Pass D7 for both families including no-auth device flow.

---

## Dependencies

- A1, A2, A3 form one chain (edit, edit, build); A is independent of B and C.
- B1 precedes B2 (public visibility precedes usable release CI and no-auth downloads).
- B1 plus B2 precede C2 (Bruce catalog entry needs the public permanent asset).
- D2 through D5 need the A3 binary on hardware.
- D6 needs B2 plus upstream `v2.38` (already published).
- D7 needs C1/C2 plus D2 through D5.

## Commits (this repo and siblings, git-master discipline)

- Launcher repo: one atomic commit `fix(ats-mini): encoder one-step navigation and reliable Enter` touching only `Launcher/boards/ats-mini/interface.cpp` (two hunks: `cfg.pullup = true;` and `FOUR3`). Fallback attempts are separate commits with hardware notes.
- Bruce repo: at most one workflow commit (B2, only if needed) plus workflow-generated release tags.
- This repo (parent): one commit adding only `docs/superpowers/plans/2026-09-11-ats-mini-encoder-ota.md` (this file), created with `GIT_MASTER=1 git add docs/superpowers/plans/2026-09-11-ats-mini-encoder-ota.md` after `GIT_MASTER=1 git status` and `GIT_MASTER=1 git diff` inspection.
- No commits for LauncherHub data, repo visibility, or releases are made from this plan step; those are operator actions recorded in the rollout log.

## Rollback per workstream

- A: revert the `interface.cpp` hunks, rerun `cd Launcher && /tmp/pio-venv/bin/pio run -e ats-mini`, reflash the prior binary.
- B: delete the bad tag/release per B2; re-apply visibility only on operator order.
- C: remove or hide the catalog entry operator-side; devices fall back to USB/esptool flash with zero firmware change.
- D: any hardware failure stops rollout; prior passing binary stays flashed.

## Explicit blockers

- `BLOCKED: LauncherHub registration (C1, C2) needs authenticated maintainer or API access; no mutation endpoint is documented, none is invented here`.
- `BLOCKED: Bruce visibility plus release (B1, B2) needs owner admin on wendells01/bruce-ats-mini plus working Actions`.
- `BLOCKED: D2 through D5 plus D7 need a physical ATS Mini with the A3 binary flashed`.

## Spec coverage (spec section to plan step)

- Spec 3 (latch `TWO03` to `FOUR3`): A2.
- Spec 3 (`cfg.pullup = true`): A1.
- Spec 3 (short Select, long Esc consumed, wake, power-off preserved): A1/A2 preserved lists plus D3/D4/D5.
- Spec 4.1 (LauncherHub-authoritative, no GitHub API on device): header architecture plus workstream C intro.
- Spec 4.2 (upstream public, OSPI merged): B3 plus C1.
- Spec 4.3 (Bruce public, versioned release, no-auth): B1 plus B2 plus D6.
- Spec 4.4 (asset contract `ats-mini-vX.YY-ospi.zip` / `Bruce-ats-mini-vX.Y.Z-merged.bin`, permanent release assets): B2 plus B3.
- Spec 4.5 (two families under `ats-mini` + `esp=S3`): C1 plus C2.
- Spec 5 items 1-5 (public, `vX.Y.Z` trigger, permanent, manifest pointer, upstream tracking): B1, B2, C1, C2.
- Spec 7 failure handling (FOUR0/A-B swap fallback, external pull-up fallback, 600 ms retune, upstream rename pin, fail-closed manifest, USB interim): A2 fallback, A1 fallback, D steps, B2 rollback, C blockers.
- Spec 8 rollout order 1-5: Rollout order 1-5 above.
- Spec 9 verification criteria (8 checkboxes): D1 through D7 (D7 covers manifest install plus no-auth device flow; D1 covers build gate).
- Spec 10 non-goals (no source/workflow/visibility/release/catalog changes in this task, no new deps, no on-device GitHub client, no upstream rename, no hardware-validation claim): honored; this file changes nothing outside itself.

## Placeholder self-review

- Searched this file for the five banned placeholder tokens from the task brief with case-sensitive word-boundary grep over content lines; result: zero matches outside this self-review sentence.
- Every code step names the exact file and symbol and shows the intended diff or the exact command with its expected result.
- No product source, workflow, visibility, release, or LauncherHub data was touched by writing this plan.
- Open item: none in the plan text; execution blockers are listed under Explicit blockers with owner and unblock condition.
