# Per-client touch experience profile — layer-one design

## 1. Document status

- Status: implementation design
- Date: 2026-09-02
- Prerequisite: PR #1025 (virtual USB HID touchscreen device and POC; verified on physical hardware that tapping a text field automatically opens the touch keyboard)
- Scope: layer one (Sunshine-only configuration and registry transactions; no driver changes)
- Revision: the 2026-09-02 fully isolated variable-control experiment confirmed that **registry writes are required** (§5.2); the transaction design was restored and expanded to write the full key set

## 2. Background and goals

Two independent mechanisms determine the touch-keyboard experience:

1. **Whether it opens**: a touch digitizer is present, touch input occurs, and `EnableDesktopModeAutoInvoke` is enabled.
2. **How it appears**: the display EDID physical-size class (≥18 inches forces undocked mode; <18 inches permits docked mode). Software cannot override this; EDID is the sole source for this decision, according to the Apollo #818 experiment.

The `clients` JSON already contains a per-client `deviceSize` value (physical size in centimeters), which is passed to VDD through `CREATEMONITOR {GUID}:[nits][wCm,hCm]`; **the EDID class is therefore already bound to the client**. This design adds the other side of the "open" behavior—the virtual touchscreen device—to the per-client profile at the same level as `deviceSize`. The fully isolated variable-control experiment on 2026-09-02 confirmed that **device attachment supplies the touch input source, while the TabletTip AutoInvoke key group controls the opening policy; both are required**. With the entire key group set to 0, the keyboard did not open; it opened again after the keys were restored (§5.2).

### Goals

1. Add a `touch` object to the `clients[]` schema (enable switch and AutoInvoke preparation).
2. At session start, apply the profile by writing the AutoInvoke registry key group and recording undo data; at session end, restore it in reverse.
3. Productize the virtual touchscreen device: **out of scope for this design**. The device has been removed from the keyboard path and is reserved for a future direct-touch injection layer (§6).
4. Never block a streaming session on failure. Touch is an enhancement, so failures only produce downgrade logs.

### Non-goals

- No runtime switching of EDID or physical size. `deviceSize` already covers this, and changing classes during a session causes a display flash.
- No layer-two VDD EDID parameterization refactor.
- **No per-client override of TabletTip layout preferences such as `dockedState`.** The Apollo #818 experiment showed that the EDID size class overrides those values. The AutoInvoke key group and layout preferences are separate transactions: the former is required by this design; the latter is excluded.
- No solution for multi-display touch routing from an external USB digitizer. It maps to the virtual desktop and does not affect a single-VDD scenario.
- No POC UI beyond `--auto`.

## 3. Current baseline and reuse points

| Component | Location | Reuse |
|---|---|---|
| Client profile container | The `clients` key in `sunshine.conf` (`config.cpp:2011-2050`), with schema validation in `confighttp.cpp:1503-1592` | Add the `touch` object directly to the existing schema |
| Client identity | `client_cert_uuid` (`pairing.cpp:493`, carried by `launch_session_t`, `rtsp.h:43`) | Look up the profile by UUID within the session |
| Session attachment points | Start: `configure_display` (`nvhttp_stream_start.cpp:222/488/688`); end: `restore_state` (`stream.cpp:4160-4180`) | Attach the touch transaction after the VDD phase |
| USB/IP device side (direct-touch layer, independent of the keyboard path) | PR #1025 `virtual_touchscreen_device` + `loopback_usbip_bridge` | Enable when future input routing is connected; it is not a prerequisite for keyboard behavior |
| Panel entry point | Control-panel client management (`/api/clients/list`) | Extend the existing client editor with `touch` fields |

## 4. Schema design

Each `clients[]` item gains an optional `touch` object. **Absent means preserve current behavior exactly**: do not create a device or write the registry. This is backward compatible.

```json
{
  "uuid": "…",
  "name": "…",
  "deviceSize": "…",
  "touch": {
    "enabled": true,
    "autoInvoke": true
  }
}
```

- `enabled`: enable the touch-keyboard registry transaction for this client during the session. This assumes that the host already has a touch digitizer; see the non-goals.
- `autoInvoke`: write the AutoInvoke registry key group for the session and restore the original values afterward (§5.2). The fully isolated variable-control experiment on 2026-09-02 proved this is required: with all AutoInvoke keys set to 0, attaching the device and tapping no longer opened the keyboard; restoring the group made it open again. Automatic touch-keyboard opening depends on these registry keys and cannot omit them.

Validation ignores all other fields when `touch.enabled` is false.

## 5. Session transaction design

### 5.1 Sequence

```text
Session start (configure_display, after VDD prepare)
  ├─ Resolve the touch profile by client_cert_uuid; no touch/enabled=false → skip
  └─ [T1] Apply registry preparation and record undo → appdata/touch_keyboard_undo.json
        Failure → log downgrade and continue the session; existing undo file (concurrent session) → skip write

Session end (restore_state, when the last video session unregisters)
  └─ [R1] Apply touch_keyboard_undo.json to restore the registry; delete the undo file only after every value is restored successfully (retain it after a partial failure for retry)
```

### 5.2 Registry values and target hive

| Entry | Value | Undo |
|---|---|---|
| `TabletTip\1.7\EnableDesktopModeAutoInvoke` | 1 | Record the original value, including "absent," and restore it at session end |
| `TabletTip\1.7\EnableDesktopModeDockedAutoInvoke` | 1 | Same |
| `TabletTip\1.7\EnableInDesktopMode` | 1 | Same |
| `TabletTip\1.7\EnableInDesktopModeTabletKeyboard` | 1 | Same |
| `TabletTip\1.7\EnableInDesktopModeAutoInvoke` | 1 | Same |
| `TabletTip\1.7\EnableDockedModeAutoInvoke` | 1 | Same |
| `TabletTip\1.7\EnableTouchKeyboardAutoInvokeInDesktopMode` | 1 | Same |
| `TabletTip\1.7\EnableTouchKeyboardAutoInvoke` | 1 | Same |
| `TabletTip\1.7\TouchKeyboardTapInvoke` | 2 | Same |
| `TabletTip\1.7\PreventTouchKeyboardAutoInvoke` | 0 | Same; write only when the original value is not 0 |

> **Experimental basis (2026-09-02, fully isolated variable control):** after setting every key above to 0 and restarting TabTip, attaching the virtual touchscreen and tapping **did not open the keyboard** (`IFrameworkInputPane` reported `visible=0` three times). Restoring the key group to 1 made it open again. Windows exposes no policy interface beyond its API/settings UI. Because it is not known which individual key is sufficient, writing the complete group with undo is the most robust strategy and is equivalent to enabling the setting as the user.

Target hive: when Sunshine runs as a service under SYSTEM, write `HKEY_USERS\<interactive user SID>\…` by enumerating the active console-session token and resolving its SID. In portable mode, write HKCU directly. The transaction module exposes both through `write_user_registry()`.

### 5.3 Known limitations to document and log

- The TextInputHost process reads and caches TabletTip values. A value written during a session **may not take effect until the keyboard process starts again**; in practice, the primary AutoInvoke key takes effect immediately. The design accepts best-effort behavior and does not forcibly restart TextInputHost, because killing it would disrupt a user who is typing.
- Concurrent clients on the same host share per-user global registry values, so a later write overrides an earlier one. Layer one **supports only one active session**. When sessions overlap, skip the touch transaction and log a warning. VDD itself also assumes a single active session.

## 6. Role of the virtual touchscreen device (outside the keyboard path)

The attachment A/B test (keyboard opened both with the device attached and after it was detached) proves that the keyboard experience **does not depend on the custom device**. A host touch digitizer already present through VDD or remote software (`\?\VIRTUAL_DIGITIZER`) supplies the input source. The USB/IP virtual touchscreen device and POC from PR #1025 remain in the repository, but their role changes to:

- **Repair for direct-touch input injection.** Sunshine's synthetic pointer injection (`InjectSyntheticPointerInput`) no longer works on 25H2 (26200), so direct-touch client events do not reach host applications. Injection through a USB HID touchscreen over the `hidusb` path is a verified alternative; a tap in the console session successfully placed the caret in Notepad.
- It depends on the separately deployed usbip-win2 component. If that component is absent, only direct-touch injection is unavailable; the keyboard experience is unaffected.
- The `virtual_touchscreen_session` wrapper has been removed from the main target and can be restored when the input path is connected.

## 7. Code locations

| Content | File |
|---|---|
| Schema validation extension | `confighttp.cpp` (clients route validation) |
| Profile lookup helper | `config.cpp` `get_client_touch_keyboard_enabled(uuid)` |
| Registry transaction module | `src/touch_keyboard_session.h` + `src/platform/windows/touch_keyboard_session.cpp` (hive resolution and undo JSON) |
| Device attach/detach wrapper | `src/virtual_touchscreen_session.h` + `src/platform/windows/virtual_touchscreen_session.cpp` (combines device, bridge, and usbip attach) |
| Attachment points | `nvhttp_stream_start.cpp` (start) and `stream.cpp` (end beside `restore_state`) |
| Panel UI | Add a touch switch to the control-panel client editor; the backend only needs to pass through the fields |

## 8. Test plan

- Unit tests: `touch` schema validation, undo write/restore, and backward compatibility when the touch field is absent.
- Key-group dependency ablation (completed 2026-09-02): set every key to 0 and restart TabTip → does not open; restore → opens.
- Attachment A/B test (completed 2026-09-02): opens when attached and still opens when detached. Keyboard behavior does not depend on the custom device, so the architecture was simplified accordingly.
- Physical hardware in a console session: tap a text field → keyboard opens; after the session ends, the registry key group is restored; a later session applies it again.
- Downgrade behavior: a registry transaction failure does not affect the streaming session.
- Power-loss class failure: when an undo file remains, try to restore it before the next session starts.

## 9. Release gates

1. The `clients` schema change requires a synchronized control-panel release for the new field UI.
2. Pinning the usbip-win2 component version (currently 0.9.7.7) constrains only the future direct-touch injection layer; the keyboard registry transaction does not depend on it.
3. Registry writes are limited to the key group in §5.2, with undo. Changes to this set require review.
