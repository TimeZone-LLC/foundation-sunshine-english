# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

`foundation-sunshine` (AlkaidLab) is a fork of [LizardByte/Sunshine](https://github.com/LizardByte/Sunshine) — a
self-hosted GameStream host for Moonlight clients. The fork is **Windows-first**: most new subsystems (VDD virtual
display, DualSense sidecar, directory mapping, HDR analysis, vmouse) are Windows-only, while the Linux/macOS paths
are inherited from upstream and largely untouched. Upstream files keep the LizardByte layout and Doxygen-style
`@file`/`@brief` headers; match that when adding C++ files.

The CMake project is still named `Sunshine` and the binary is still `sunshine`.

## Build

Windows builds run in the **MSYS2 UCRT64** shell (gcc/ninja, not MSVC). `docs/building.md` has the full dependency
list; `.github/workflows/main.yml` is the authoritative reference for how a clean build is done.

```bash
git submodule update --init --recursive     # third-party/* are required, not optional
cmake -B build -G Ninja -S .
ninja -C build
```

Local Windows dev preset (`CMakePresets.json`) skips docs and driver downloads:

```bash
cmake --preset dev-win && cmake --build --preset dev
```

Options worth knowing (`cmake/prep/options.cmake`):

- `BUILD_TESTS=ON` — builds the aggregate `test_sunshine` target plus the per-module test executables.
- `BUILD_TRAY_TESTS=ON` — the small standalone tray/service-state suites, buildable without `BUILD_TESTS`.
- `BUILD_WEB_UI=OFF` — skip the npm build step (CI builds the Web UI separately, then configures with this off).
- `FETCH_DRIVER_DEPS` / `DRIVER_DEPS_REQUIRED` — downloading the private vmouse/VDD driver blobs. Set
  `DRIVER_DEPS_REQUIRED=OFF` when the token is unavailable; the build then just excludes those drivers.
- `SUNSHINE_ENABLE_LEGACY_TRAY=ON` — use the in-process C++ tray instead of the out-of-process GUI agent.

Packaging: `cpack -G ZIP --config ./build/CPackConfig.cmake` (portable), Inno Setup for the Windows installer
(`cmake/packaging/sunshine.iss.in`).

## Tests

C++ tests are GoogleTest, registered with CTest:

```bash
ctest --test-dir build --output-on-failure                 # everything
ctest --test-dir build -R abr_unit_tests                   # one registered test
./build/tests/test_sunshine --gtest_filter=CryptoTests.*   # one case in the aggregate suite
```

`test_sunshine` links all of `src/` minus `main.cpp`, so it is slow to build; most newer work instead gets a narrow
executable in `tests/CMakeLists.txt` that compiles only the sources under test plus stubs (`abr_unit_tests`,
`client_fingerprint_unit_tests`, `ds5_sidecar_client_unit_tests`, `vmouse_unit_tests`, …). Prefer extending that
pattern over growing the aggregate suite.

`DownloadFileTests/*` is excluded from the CTest invocation because it hits httpbin.org — run it by hand with
`--gtest_filter` when touching `http::download_file`. `tests/tools/` holds standalone diagnostic programs (fake
sidecar, vmouse probes), deliberately excluded from the gtest binary.

Web UI tests use the Node test runner:

```bash
npm run lint:webui      # eslint, --max-warnings 0; CI runs this before tests and build
npm run test:webui      # node --test src_assets/common/assets/web/tests/*.test.js
node --test src_assets/common/assets/web/tests/usePin.test.js   # single file
```

## Web UI

Vue 3 + Composition API, Bootstrap 5, vite-plugin-ejs templating, built by Vite into the CMake binary dir. Source
is `src_assets/common/assets/web/` (`views/`, `components/`, `composables/`, `services/`, `utils/`). See
`docs/WEBUI_DEVELOPMENT.md`.

```bash
npm run dev             # vite build --watch
npm run dev-server      # HTTPS dev server on :3000 with API proxy + mock data
npm run build
```

The Vite build reads `SUNSHINE_SOURCE_ASSETS_DIR` and `SUNSHINE_ASSETS_DIR` from the environment to decide
input/output paths; running `npm run build` bare uses the in-tree defaults, which is why CMake's `web-ui` target
sets both.

ESLint only enforces `no-undef`, but that is the point: every identifier must be declared or imported, and browser
vs Node globals are scoped per directory in `eslint.config.js`. Don't paper over a missing import with
`eslint-disable`.

`src_assets/common/sunshine-control-panel/` is a **separate** Tauri 2 + Vue desktop app (its own package.json and
Cargo manifest). CI consumes a prebuilt binary from the GUI repo's releases; build it locally with
`ninja -C build sunshine-control-panel` (requires npm + cargo).

## i18n

Only ever edit `src_assets/common/assets/web/public/assets/locale/en.json`. Other locales are managed by Crowdin
and get overwritten. After adding keys: `npm run i18n:sync` → `npm run i18n:format` → `npm run i18n:validate`. CI
fails on unsynced or unformatted locale files. Brand names (Sunshine, LizardByte, AMD, Intel, NVIDIA) are never
translated.

## DualSense sidecar (Windows)

`tools/sunshine-ds5-sidecar/` is a .NET 10 helper that isolates Sunshine from the third-party HIDMaestro runtime and
owns virtual DS5 devices behind the versioned `SDS5` named-pipe protocol. The C++ side is
`src/platform/windows/ds5/ds5_sidecar_client.cpp`. Build/package with `scripts/build-ds5-sidecar.ps1` (it downloads
and SHA-verifies the pinned HIDMaestro release and refuses paths outside `build/`).
`dotnet Sunshine.Ds5Sidecar.dll --self-check` validates the channel layout without elevation or hardware. Lifecycle
rules are in `docs/windows_dualsense_component_lifecycle.md`.

## Architecture

**Startup** (`src/main.cpp`) is a chain of RAII deinit guards — logging, config, display device session, platform,
process, input, http — followed by three long-lived server threads plus the tray/GUI agent. Order matters and the
guards unwind in reverse; new global subsystems should follow the same `auto x_guard = subsystem::init()` shape.

**Three HTTP surfaces**, all TLS via `SimpleWeb`:

- `src/nvhttp.cpp` + `src/nvhttp/` — the GameStream protocol Moonlight talks to (`/serverinfo`, `/pair`,
  `/applist`, `/launch`, `/resume`). Client identity is enforced by cert verification. Handlers for each area live
  in `src/nvhttp/` (`pairing`, `apps`, `sessions`, `display_control`, `dynamic_params`, `abr_api`, `ai_api`,
  `clipboard_api`, `network_probe`). Adding an endpoint = a handler there plus one `https_server.resource[...]`
  registration in `nvhttp.cpp`.
- `src/confighttp.cpp` — the Web UI's config server: serves the built pages and the `/api/*` config endpoints.
- `src/tray/tray_http.cpp` and `src/file_mapping/file_mapping_ws*` — local control channels for the out-of-process
  GUI agent (`docs/tray-protocol.md`).

**Streaming path**: `rtsp.cpp` negotiates the session → `stream.cpp` runs the RTP/FEC control and video/audio send
loops → `video.cpp` / `audio.cpp` own the capture-encode pipelines → `platform/*/display*.cpp` and the encoder
backends (`src/nvenc/`, `src/amf/`, ffmpeg/VAAPI) do the actual work. `process.cpp` starts/stops the app for the
session; `launch_session_manager.cpp` bounds pending launch tickets.

**Platform abstraction** is `src/platform/common.h`: `display_t`, `encode_device_t` (with `avcodec_`, `nvenc_`,
`amf_` subclasses), `audio_control_t`, `mic_t`, and the input structs. Each platform dir implements those. Windows
has several parallel display backends — `display_ram`, `display_vram`, `display_wgc`, `display_vdd`,
`display_vdd_vram`, `display_amd` — selected at runtime; a capture-path change usually needs to be made in more
than one of them.

**Fork-specific subsystems** worth reading the docs for before touching:

- `src/hdr/`, `src/video_hdr_*` — dual PQ/HLG encode, per-frame GPU luminance analysis, HDR10+/HDR Vivid SEI.
- `src/platform/windows/display_vdd*`, `vdd_frame_channel.h` — ZakoVDD virtual display integration and the
  zero-copy sealed frame channel (`docs/windows_vdd_sealed_frame_channel.md`, `docs/vdd-prerequisite-closure.md`).
- `src/file_mapping/` — Windows host directory sharing over an authenticated WebSocket, token-scoped and read-only
  by default (`docs/windows_directory_mapping_*.md`). Heavily unit-tested; keep it that way.
- `src/abr.cpp` + `src/ai/` — adaptive bitrate with a threshold fallback tier and an LLM-driven tier (prompt in
  `src/assets/abr_prompt.md`).
- `src/webhook/` — outbound event notifications (`docs/webhook_design.md`, `docs/webhook_format.md`).
- `src/clipboard_bridge.*` — deliberately protocol-ignorant byte forwarder; clipboard parsing belongs in the Rust
  GUI agent, not here.
- `src/client_fingerprint.*` — per-client capability/quirk rules (`docs/client-fingerprint-rules.md`).

## Conventions

- C++23. `.clang-format` is LLVM-based and centrally managed upstream — don't edit it, just run it.
- Conventional commits with a scope, e.g. `feat(ds5):`, `fix(windows):`, `fix(webui):`, `fix(hdr):`. Subjects are
  English or Chinese; PR numbers are appended on merge.
- `.coderabbit.yaml` drives automated review (in Chinese) and states the per-path focus: memory/thread safety and
  RAII in `src/**`, cross-platform consistency in `src/platform/**`, XSS/CSRF in `src_assets/**`.
- `third-party/` submodules are pinned deliberately — the NVENC header lines (1100/1200/1300/1301) are separate
  gitlinks on purpose so an older fallback API line is never replaced by a newer one that raises the minimum
  driver version. AMF is a sparse checkout of `amf/public/include` only.
