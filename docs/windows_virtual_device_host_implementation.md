# Windows Sunshine virtual-device host implementation plan

## 1. Document status

- Status: implementation design
- Date: 2026-08-28
- Target platforms: Windows 10/11 x64; the first release will define ARM64 interfaces but does not promise an ARM64 build
- Processes: Sunshine Core plus one Core-owned virtual-device host
- Initial runtime: HIDMaestro release tag `v1.6.2` (`HIDMaestro.Core.dll` `AssemblyFileVersion = 1.6.2.0`) plus unmodified, signed usbip-win2 0.9.7.7
- First new device: `Sunshine Virtual Microphone`

This design evolves the existing `Sunshine.Ds5Sidecar` from an accessory process for virtual DualSense devices into a Sunshine virtual-device host. The host continues to isolate the third-party .NET runtime while independently managing virtual DS5, DS5 USB Audio, and a USB virtual microphone.

The document covers process ownership, protocol extensions, device lifecycles, the microphone data plane, code locations, tests, and release gates. It supplements `docs/windows_dualsense_component_lifecycle.md`. Where the two documents conflict about the Sidecar process lifecycle, this host model is the target for subsequent implementation; existing DS5 devices remain session-scoped.

## 2. Goals and non-goals

### 2.1 Goals

1. Host every HIDMaestro virtual device in one process; do not create a second persistent process for the microphone.
2. Give the DS5 and virtual microphone independent lifecycles. Failure of one device must not require destruction of the other.
3. Use a user-mode standard USB Audio Class device and the Windows inbox USB Audio driver; do not write a new Windows audio function driver.
4. Let Sunshine submit client microphone PCM directly to a USB Audio IN endpoint, bypassing VB-CABLE's render/capture cable route.
5. Preserve a stable virtual-microphone identity across Moonlight sessions; emit silence instead of disappearing from Windows when the client disconnects.
6. Retain the VB-CABLE backend and an explicit fallback. Mark the USB/IP feature experimental in the first release.
7. Prioritize control-plane reliability. High-frequency audio must not block controller input, device detach, status replies, or host shutdown.
8. Do not modify the usbip-win2 kernel driver, preserving its upstream signature and upgrade boundary.

### 2.2 Non-goals

- Do not turn the host into a system service or long-lived orphan independent of Sunshine Core.
- Do not change the Windows default recording device automatically in the first release.
- Do not carry Dolby MAT, Atmos, IEC 61937, or another compressed bitstream through the virtual microphone.
- Do not implement a USB Audio render endpoint in the first release; the standalone microphone profile exposes only a capture endpoint.
- Do not remove the VB-CABLE code or automatically uninstall an existing VB-CABLE installation in the first release.
- Do not support multiple Sunshine virtual-microphone instances in the first release.
- Do not expose remote USB/IP to the network; the USB/IP device server is loopback-only.
- Do not rewrite HIDMaestro's USB/IP device framework or host the CLR inside Sunshine in the first release.

## 3. Current implementation baseline

### 3.1 DS5 Sidecar

The current `tools/sunshine-ds5-sidecar` is a .NET helper that:

- loads the verified `HIDMaestro.Core.dll`;
- uses the `SDS5` v1 binary Named Pipe protocol;
- accepts only one verified, elevated Sunshine owner per pipe connection;
- destroys every virtual device created by that connection and exits when it sees owner EOF, a broken pipe, or Core Job Object closure;
- uses HIDMaestro's UMDF2 backend for a normal DS5 and its USB/IP backend for a composite DS5;
- defines a 48 kHz, 16-bit, stereo UAC1 microphone IN endpoint in `dualsense-composite`, although `ControllerSession` currently consumes only USB Audio OUT data and never calls `Submit()` on the microphone.

### 3.2 Microphone redirection

The current Windows path is implemented by `mic_write_wasapi_t`:

1. Locate VB-CABLE or guide the user through installation.
2. Open the VB-CABLE render endpoint.
3. Write 48 kHz, mono S16 PCM through `IAudioRenderClient`.
4. Expose the VB-CABLE capture endpoint as the microphone seen by applications.
5. Initialize the device when a session begins; release it and restore the default endpoint after the last microphone session ends.

The target retains upstream PCM decoding, mixing, and the `write_mic_pcm()` call contract, replacing only the final sink.

## 4. Core architectural decisions

### 4.1 One host process, not one Sidecar per device

```text
Sunshine Core
  |
  | SDS5 v1 Named Pipe (control + bounded real-time messages)
  v
Sunshine Virtual Device Host
  |-- ControllerSession[0..N]
  |     |-- UMDF2 DS5
  |     `-- USB/IP DS5 composite
  |
  `-- VirtualMicrophoneSession[0..1]
        `-- USB/IP UAC1 capture-only device
              `-- usbip-win2 UDE bus
                    `-- Windows usbaudio.sys
```

The first release keeps the `Sunshine.Ds5Sidecar.exe` filename and existing component manifest so one change does not also migrate download assets, installation paths, hashes, and the GUI. Introduce the generic `VirtualDeviceHostServer` internally first. After the protocol and component-upgrade path stabilize, migrate separately to `Sunshine.DeviceSidecar.exe`.

### 4.2 Separate the host-process lifecycle from device lifecycles

Core is the sole owner of the host process and always places it in a Job Object with `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`. The host must not survive loss of the owner pipe.

Startup rules:

- If the user enables the USB virtual microphone, Core starts the host and creates the microphone after configuration loading, component validation, and interactive-session checks complete.
- If USB microphone support is disabled but a virtual DS5 is required, the first DS5 allocation starts the host as it does today.
- If both features are in use, they share one host and owner pipe.

Stop rules:

- While the USB virtual microphone is enabled, ending a Moonlight session neither stops the host nor destroys the microphone.
- DS5 devices may still detach after the grace period following the last DS5 session, without affecting the microphone.
- When the user explicitly disables the USB microphone and no other virtual device exists, Core may stop the host gracefully.
- Core exit, owner-pipe loss, or Job Object closure makes the host destroy every device and exit.

### 4.3 Keep the `SDS5` v1 frame format and extend it with capabilities

Do not immediately change the 16-byte header, magic, or `Version = 1`. A new Core sends microphone messages only when `HelloReply` advertises the corresponding capability:

```text
VirtualMicrophone      = 1 << 10
PersistentDeviceHost   = 1 << 11
MicrophoneStatus       = 1 << 12
```

This ensures that:

- an old Core uses only existing DS5 messages with a new host;
- a new Core sees that an old Sidecar lacks the capability and does not send unknown messages;
- explicitly selecting the USB microphone reports that the component needs an update rather than silently creating the wrong device.

The protocol magic and executable name may change in a later release, but that rename is not coupled to the first microphone phase.

### 4.4 Independent device fault domains

The following operations must affect only their target device:

- DS5 attach/detach does not create or destroy the virtual microphone.
- Microphone `flush`, remote-stream loss, or endpoint pin closure does not detach a DS5.
- A microphone submission failure first enters that device's fault/recreate flow; it does not immediately exit the complete host.
- Only pipe corruption, invalid owner identity, a global HMContext fault, or host shutdown cleans up every device.

A usbip-win2 kernel fault cannot be isolated in user mode. Manage that risk with a pinned version, stress tests, and release gates.

## 5. Runtime state model

### 5.1 Host states

```text
stopped
  -> starting          Core creates process and connects pipe
  -> negotiating       Hello/capability negotiation
  -> ready             Devices can be managed
  -> degraded          At least one device failed; others remain usable
  -> stopping          Core shutdown or last feature disabled
  -> stopped

starting/negotiating/ready/degraded
  -> faulted           Pipe, identity, or global HMContext failure
  -> stopping
```

### 5.2 Virtual-microphone states

```text
absent
  -> creating          Load profile and create USB/IP device
  -> enumerating       Wait for stable Windows capture endpoint
  -> idle              Endpoint exists; host application has not opened it
  -> remote_active     Valid remote PCM arrives with capture pin closed (is_host_streaming=false)
  -> idle              Capture pin is closed and STREAM_END or MicFlush arrives (is_host_streaming=false)
  -> host_capturing    Windows application opened the UAC IN stream
  -> remote_active     Valid client PCM is present
  -> host_capturing    Client stream ends; continue submitting silence
  -> idle              Host application closes capture pin
  -> destroying        User disables feature or host exits
  -> absent

creating/enumerating/idle/host_capturing/remote_active
  -> device_faulted
```

`host_capturing` and `remote_active` are orthogonal facts. `state` expresses the most important activity source, while `is_host_streaming` independently reports whether the Windows capture pin is open. Core and the GUI must interpret their combination using this table, not either field alone:

| Application/capture pin | Remote PCM | `state` | `is_host_streaming` | Core/GUI interpretation |
|---|---|---|---|---|
| Open | None | `host_capturing` | `true` | Application is capturing; host submits silence |
| Open | Present | `remote_active` | `true` | Host application is consuming remote PCM |
| Closed | Still sending | `remote_active` | `false` | Remote remains active with no host consumer; pin closure first clears queued PCM, and later data remains bounded by the queue |
| Closed | None | `idle` | `false` | Endpoint is enumerated and both sides are idle |

When the application closes, the host must clear any queued PCM before emitting status. Core therefore never observes a steady state where the pin is closed but stale buffered bytes remain. Absence of remote PCM does not mean that no Windows application has opened the microphone; in that case the host must submit silence.

### 5.3 Lifecycle invariants

1. At most one `Sunshine Virtual Microphone` exists in one host.
2. A Moonlight session-count transition from 1 to 0 sends only `MicFlush`, never `MicDestroy`.
3. Only configuration disablement, an explicit repair flow, or host exit can destroy the microphone.
4. `MicCreate` is idempotent; a duplicate request returns the existing generation and format.
5. Every device re-enumeration monotonically increments its generation; PCM for an old generation must be dropped.
6. A fixed serial/container identity preserves endpoint identity; it is not derived from a Moonlight session ID.

## 6. Virtual-microphone USB profile

Add:

```text
tools/sunshine-ds5-sidecar/profiles/sunshine-virtual-microphone.json
```

Fixed first-release format:

| Property | Value |
|---|---|
| USB class | Audio Class 1.0 |
| Functions | `audioControl` + `audioStreamingIn` |
| Endpoint | Isochronous IN |
| Sample rate | 48,000 Hz |
| Channels | 1 |
| Bit depth | 16-bit signed little-endian PCM |
| Packet interval | 1 ms |
| Normal packet | 96 bytes |
| Maximum packet | 98 bytes, permitting 48/49-frame scheduling |
| Product string | `Sunshine Virtual Microphone` |
| Serial | Stable for the installation instance; no random session value |

The profile contains no HID interface, AudioStreamingOut, or speaker feature unit. Windows should create only a capture endpoint.

A prototype may use an internal test VID/PID, but a release build must receive a legitimate assignment that cannot collide with real hardware. Do not reuse the Sony VID/PID or treat a random VID/PID as a durable identity.

Perform static and runtime validation when loading the profile:

- exactly one AudioStreamingIn alternate setting;
- format exactly mono/48 kHz/S16;
- endpoint is IN, isochronous, and 1 ms;
- `HMController.UsbAudio.Microphone` exists;
- `Channels`, `SampleRateHz`, and `BitsPerSample` match the profile.

## 7. Core/host protocol extensions

### 7.1 Message numbers

Keep the existing ranges: Core requests use low numbers; asynchronous host status uses `100+`.

```text
MicCreate       = 12
MicCreateReply  = 13
MicPcm          = 14
MicFlush        = 15
MicFlushReply   = 16
MicDestroy      = 17
MicDestroyReply = 18
HostStatus      = 106
MicStatus       = 107
```

`MicCreate`, `MicFlush`, and `MicDestroy` use a nonzero `request_id` and wait for their corresponding `Reply`. `MicPcm` uses `request_id = 0` and receives no per-packet reply.

### 7.2 `MicCreate`

Request payload:

```text
sample_rate_hz : u32 = 48000
channels       : u8  = 1
bits_per_sample: u8  = 16
flags          : u16
```

Reply payload:

```text
result          : i32
generation      : u32
sample_rate_hz  : u32
channels        : u8
bits_per_sample : u8
reserved        : u16
```

The host performs no implicit format negotiation. A request for any other format returns a stable error code.

### 7.3 `MicPcm`

Each message should carry 10 ms, or 480 frames:

```text
generation      : u32
sequence        : u32
capture_time_us : u64
frame_count     : u16
flags           : u16
pcm_s16le       : frame_count * 2 bytes
```

Flags:

```text
STREAM_START    = 1 << 0
STREAM_END      = 1 << 1
DISCONTINUITY   = 1 << 2
SILENCE         = 1 << 3
```

Constraints:

- In the first release, `frame_count` is at most 960 frames (20 ms).
- Payload length must exactly match the header and frame count.
- If generation differs from the current device generation, drop the packet and increment a counter.
- On a sequence gap, clear old PCM and give the next packet discontinuity semantics.
- `capture_time_us` is diagnostic and reserved for future drift control; it is not an absolute USB-packet schedule.

### 7.4 `MicFlush`

`MicFlush` clears remote PCM that has not yet been submitted, resets the sequence, and continues sending silence to host capture. It does not change the USB device, alternate setting, or Windows endpoint identity.

### 7.5 `MicDestroy`

Use this only when the user disables the USB microphone, component repair or upgrade must release the device, or the host exits. A normal Moonlight disconnect must not send it.

### 7.6 `MicStatus`

Send immediately on a state change and at most once per second during sustained activity:

```text
generation       : u32
state            : u8
is_host_streaming: u8
reserved         : u16
buffered_bytes   : u32
underruns        : u32
dropped_frames   : u32
submit_errors    : u32
last_error       : i32
```

Status uses the reliable control queue and is not paired with every PCM packet.

## 8. Data plane and threading model

### 8.1 Core side

Network and mixing threads must not perform a potentially blocking pipe write. `usbip_mic_sink_t` owns a bounded SPSC PCM queue and a dedicated sender worker:

```text
Moonlight receive/decode/mix
  -> write_mic_pcm()
      -> enqueue 10 ms PCM
          -> virtual-device-host writer
              -> Named Pipe
```

Queue policy:

- Target backlog: 20 ms.
- Maximum backlog: 60 ms.
- When full, drop the oldest PCM and set `DISCONTINUITY` on the next packet.
- Never add client microphone latency by waiting for pipe capacity.
- Control messages take priority over PCM; PCM cannot starve `MicDestroy`, shutdown, or DS5 detach.

### 8.2 Host side

The pipe read loop performs only frame validation and fast enqueue. The microphone pump is independent of the pipe loop:

```text
Pipe ReadLoop
  -> validate MicPcm
  -> bounded PCM ring

Microphone Pump
  -> inspect IsStreaming / BufferedBytes
  -> Submit(PCM)
  -> Submit(silence) on underrun
  -> count partial submit and drop unaccepted old tail
```

The production implementation uses `ArrayPool<byte>` or a fixed-block pool to avoid about 100 persistent allocations per second. No logging, JSON serialization, disk access, or network operation belongs in the pump's hot path.

### 8.3 Buffering and drift policy

The first release avoids a high-quality resampler and prioritizes bounded latency:

- When `BufferedBytes` is below about 10 ms, add silence and increment underruns.
- Above about 60 ms, drop the oldest data back to about 20 ms and mark a discontinuity.
- If `Submit()` returns a partial length or 0, do not block retrying old data; record the error and prioritize newer PCM.
- After the threshold of consecutive submission failures, place the device in `device_faulted`, stop high-frequency logs, and notify Core.

Add narrow adaptive resampling later only if measurement shows unacceptable long-term drift between the client clock and USB host clock.

## 9. Sunshine Core code structure

### 9.1 Generic host client

Proposed additions:

```text
src/platform/windows/virtual_device_host/
  host_client.h
  host_client.cpp
  protocol.h
  process_owner.h
  process_owner.cpp
```

Responsibilities:

- Validate fixed paths and the component manifest.
- Create a random pipe, launch the elevated helper, and add it to the Job Object.
- Negotiate Hello/capabilities.
- Provide one reader and a priority writer.
- Correlate requests and replies; handle timeouts, closure, and one-shot recovery.
- Maintain host reference counts and the device-owner registry.

In phase one, preserve the public `ds5_sidecar_client_t` interface and delegate internally to the shared `host_client_t`. This prevents the host refactor from spreading to input-backend callers.

### 9.2 Microphone sink abstraction

Promote the current `mic_write_wasapi_t` to one backend of a common interface:

```cpp
class mic_sink_t {
public:
  virtual int start() = 0;
  virtual int write_pcm(std::span<const std::int16_t> samples) = 0;
  virtual void flush() = 0;
  virtual void stop_stream() = 0;
  virtual void shutdown() = 0;
  virtual ~mic_sink_t() = default;
};
```

Implementations:

```text
wasapi_cable_mic_sink_t   Existing VB-CABLE path
usbip_mic_sink_t          New host/UAC path
```

Existing `write_mic_pcm(samples, frame_count)` callers need not supply a timestamp. `usbip_mic_sink_t` generates a diagnostic-only `capture_time_us` from a monotonic clock when it queues PCM for sending. If upstream later provides a source timestamp, pass it as optional adapter metadata without changing the existing call contract.

`stop_stream()` and `shutdown()` must remain distinct: the former corresponds to the end of a remote session and only flushes; only the latter may destroy the endpoint.

### 9.3 Configuration

Proposed new settings:

```text
microphone_redirect_backend = vb_cable | usbip_experimental | auto | disabled
microphone_set_default      = false
```

Behavior:

- `vb_cable`: preserve current behavior.
- `usbip_experimental`: if host or UAC creation fails, fail explicitly; never silently write to another playback device.
- `auto`: try USB/IP first. Fall back only when both VB-CABLE capture and render endpoints are already available, and log the selected backend.
- `disabled`: create no sink.
- `microphone_set_default=false`: create the endpoint only; the user selects it explicitly in the game.

Until the USB/IP backend is production-ready, existing configurations must not migrate automatically from VB-CABLE to USB/IP.

## 10. Host code structure

Gradually split `SidecarServer` into:

```text
tools/sunshine-ds5-sidecar/
  VirtualDeviceHostServer.cs
  Protocol.cs
  DeviceRegistry.cs
  ControllerSession.cs
  VirtualMicrophoneSession.cs
  MicrophonePcmQueue.cs
  DefaultAudioEndpointGuard.cs
  profiles/
    sunshine-virtual-microphone.json
```

`DeviceRegistry` owns:

- the mapping from DS5 device ID to `ControllerSession`;
- the sole `VirtualMicrophoneSession`;
- idempotent create/destroy;
- shutdown cleanup in this order: stop pump → remove event handlers → dispose USB device → dispose HMContext.

The first release need not rename the namespace, project directory, or release asset. Perform a naming-only migration after behavior is stable, separate from data-plane changes.

## 11. Security boundary

1. The Named Pipe retains `CurrentUserOnly`, a random name, and a single instance.
2. The Sidecar validates the owner process token and elevation when the connection is established. A PID, name, or token string in the protocol is not authorization.
3. The Sidecar must reside in a fixed, canonicalized active-component directory whose manifest and SHA-256 Core validates. Before product enablement, also validate directory and file owner/DACL, reject reparse points on every path component and the executable, and hold a verified file handle that prevents writes or replacement from digest validation through completion of `CreateProcessW`, or use an equivalent atomic mechanism. The current path-based reopen has a TOCTOU risk; digest validation alone does not close it.
4. Core does not pass an arbitrary profile path, DLL path, URL, command line, or VID/PID to the host.
5. The microphone profile is a validated component asset; the host accepts only a built-in profile ID.
6. `MicPcm` uses checked arithmetic and rejects length overflow, non-fixed formats, and packets longer than 20 ms.
7. Every PCM queue has a hard limit; a remote client cannot make an elevated Sidecar allocate unbounded memory through the audio stream.
8. The USB/IP server binds only to local loopback and adds no firewall rule.
9. The GUI cannot connect directly to the runtime pipe. It queries status and requests tests through Core.
10. Host logs contain neither PCM contents nor a reusable owner secret.

## 12. Failure handling and fallback

| Failure | Core/host behavior | User-visible result |
|---|---|---|
| Component missing or digest mismatch | Do not start host | Prompt to install or repair component |
| Old Sidecar lacks microphone capability | DS5 can continue; USB microphone unavailable | Prompt to update component |
| usbip-win2 unavailable | Do not loop installation; return stable error code | Experimental backend fails; `auto` may fall back to VB-CABLE |
| UAC endpoint enumeration times out | Destroy this microphone object; retain host | Report device-creation failure |
| PCM queue overflow | Drop oldest data and mark discontinuity | Brief missing audio without accumulated latency |
| PCM underrun | Submit silence | Application continues receiving a silent microphone |
| Repeated `Submit()` failure | Put microphone in faulted state | DS5 keeps working; offer rebuild action |
| Pipe write stalls | Cancel I/O, close owner, terminate host | One controlled recovery; audio thread does not hang |
| Sidecar crashes | Core restarts it automatically at most once per backoff window | Endpoint re-enumerates and reports a generation change |
| Core crashes | Job Object terminates host | Windows cleans up virtual devices |
| usbip-win2 kernel crash | User mode cannot recover | Prevent default enablement and production release |

Automatic recovery requires a circuit breaker. After a second host failure within 30 seconds, stop automatic restarts to avoid repeated attach/detach when a kernel-lifecycle problem is suspected.

## 13. Observability

Core structured logs include at least:

```text
[virtual-device-host] state=ready protocol=1 capabilities=...
[virtual-mic] backend=usbip generation=3 state=idle endpoint_id=...
[virtual-mic] state=remote_active buffered_ms=20 underruns=0 dropped_frames=0
[virtual-mic] fallback=vb_cable reason=transport_unavailable
```

Do not log every PCM packet. During activity, emit aggregate debug metrics once every ten seconds; emit state changes and faults immediately.

The GUI separates at least these states:

- Runtime component: missing, verified, update required, corrupt.
- USB/IP transport: missing, available, disallowed version, restart required.
- Virtual microphone: not created, enumerating, idle, host capturing, remote active, faulted.
- Current backend: USB/IP, VB-CABLE, disabled.
- Endpoint name and stable ID.
- Diagnostic details for underruns, drops, and the latest error code.

## 14. Phased implementation

### Phase 0: Contract and risk baseline

- Pin the current HIDMaestro, usbip-win2, Windows build, and HVCI test combination.
- Add golden tests for the current `SDS5` header, capabilities, and message codecs.
- Record known usbip-win2 kernel-stability issues and version selection; do not upgrade opportunistically during implementation.
- Freeze the `HMMicrophoneInput` format, `Submit()`, `IsStreaming`, and `BufferedBytes` contracts with reflection/runtime tests.

Completion criteria: CI validates protocol ABI without creating a virtual device; a disposable Windows test machine can execute the microphone API smoke test.

### Phase 1: Generic host refactor with unchanged behavior

- Extract the shared process/pipe owner from `ds5_sidecar_client_t`.
- Refactor `SidecarServer` internally into `VirtualDeviceHostServer + DeviceRegistry`.
- Keep all existing DS5 message, profile, and lifecycle tests passing.
- Verify that one host can hold several DS5 sessions while the real-time feedback path cannot starve the control queue.

Completion criteria: with the virtual microphone disabled, DS5 behavior and release artifacts match the current release.

### Phase 2: USB microphone data-path prototype

- Add the capability and `MicCreate/MicPcm/MicFlush/MicDestroy/MicStatus` messages.
- Use the existing composite profile's `UsbAudio.Microphone` first for development-only path validation.
- Temporarily copy mono PCM into stereo to verify that Windows Recorder receives a test tone and Moonlight voice.
- Do not set the default recording device or expose this temporary DS5 endpoint as a product feature.

Completion criteria: `Sunshine PCM -> pipe -> HMMicrophoneInput.Submit -> Windows capture` works end to end and reports sequence, drop, and underrun metrics.

### Phase 3: Standalone UAC1 capture-only profile

- Complete `sunshine-virtual-microphone.json` and descriptor tests.
- Confirm that Windows creates only one capture endpoint and no render or HID endpoint.
- Pin serial/container identity and verify that reconnects and Sunshine-session changes do not create duplicate `(2)` or `(3)` devices.
- Permit only internal/experimental builds until a release VID/PID is selected.

Completion criteria: applications can select `Sunshine Virtual Microphone` reliably, and 100 Moonlight reconnects do not change the endpoint ID.

### Phase 4: Sunshine sink and lifecycle integration

- Introduce `mic_sink_t` and make VB-CABLE an explicit backend.
- Implement the bounded queue and worker in `usbip_mic_sink_t`.
- Split the current "release device after last session" behavior into `flush/stop_stream` and `shutdown`.
- Keep the host alive across Moonlight sessions while the USB microphone is enabled.
- Add explicit configuration and backend-selection logs.

Completion criteria: USB/IP and VB-CABLE can be selected independently; the final `auto` route is diagnosable; existing VB-CABLE tests do not regress.

### Phase 5: GUI, installation, and diagnostics

- Add backend selection, an experimental warning, device status, and a test button to the audio configuration page.
- Update the component-management description from DS5 Sidecar to virtual-device component without immediately forcing an asset rename.
- Guide installation of the system-level transport only when the user explicitly selects the USB/IP backend.
- The test action creates or reuses the microphone, submits a short test tone, and verifies the capture endpoint rather than merely checking enumeration.

Completion criteria: the user can always see the current backend, device state, failure reason, and recovery action.

### Phase 6: Stability and release gates

- Stress pin open/close, audio-service restart, sleep/resume, Core/Sidecar kill, and long-running audio.
- Use Driver Verifier only on an isolated test machine; do not enable system-wide verification on an everyday development system.
- Cover the Secure Boot/HVCI, Windows build, and pinned usbip-win2 version matrix.
- Discuss default enablement only after the relevant usbip-win2 kernel pool-corruption/teardown problems have a verifiable, signed fix.

Completion criteria: satisfy the acceptance criteria in §16 and pass the release gates. Otherwise, remain experimental with VB-CABLE fallback.

## 15. File-change inventory

### 15.1 Proposed additions

```text
docs/windows_virtual_device_host_implementation.md
src/platform/windows/virtual_device_host/host_client.h
src/platform/windows/virtual_device_host/host_client.cpp
src/platform/windows/virtual_device_host/protocol.h
src/platform/windows/mic_sink.h
src/platform/windows/mic_usbip.h
src/platform/windows/mic_usbip.cpp
tools/sunshine-ds5-sidecar/DeviceRegistry.cs
tools/sunshine-ds5-sidecar/VirtualDeviceHostServer.cs
tools/sunshine-ds5-sidecar/VirtualMicrophoneSession.cs
tools/sunshine-ds5-sidecar/MicrophonePcmQueue.cs
tools/sunshine-ds5-sidecar/profiles/sunshine-virtual-microphone.json
```

### 15.2 Proposed modifications

```text
src/platform/windows/ds5/ds5_sidecar_client.h
src/platform/windows/ds5/ds5_sidecar_client.cpp
src/platform/windows/audio.cpp
src/platform/windows/mic_write.h
src/platform/windows/mic_write.cpp
src/stream.cpp
src/config.h
src/config.cpp
tools/sunshine-ds5-sidecar/Program.cs
tools/sunshine-ds5-sidecar/Protocol.cs
tools/sunshine-ds5-sidecar/SidecarServer.cs
tools/sunshine-ds5-sidecar/ProtocolSelfTest.cs
tools/sunshine-ds5-sidecar/README.md
src_assets/common/assets/web/configs/tabs/AudioVideo.vue
src_assets/common/assets/web/public/assets/locale/en.json
```

Split actual changes by phase. Do not combine the generic host refactor, new protocol messages, profile descriptors, GUI, and component-asset rename in one commit.

## 16. Test matrix and acceptance criteria

### 16.1 Automated tests

| Test | Requirement |
|---|---|
| Protocol golden test | Fix all fields' little-endian encoding, lengths, and maxima |
| Capability compatibility | New Core + old Sidecar sends no microphone messages; old Core + new host retains normal DS5 behavior |
| Queue overflow | Drop oldest frame, set discontinuity, keep memory bounded |
| Sequence gap | Clear old data; do not replay stale PCM |
| Generation mismatch | Drop PCM for an old device |
| Partial `Submit()` | Do not block or retry an old tail indefinitely |
| Owner EOF | Release every USB object after stopping the pump |
| DS5/microphone isolation | Destroying either device does not affect the other |
| Configuration migration | Old configurations retain VB-CABLE behavior |

### 16.2 Local functional acceptance

- Windows shows exactly one `Sunshine Virtual Microphone` capture endpoint.
- The endpoint format is 48 kHz, mono, S16, with no new render endpoint.
- Windows Recorder, Discord, browser WebRTC, and at least two real games can capture remote voice.
- With no remote PCM, recordings contain silence rather than repeating the last audio segment.
- Moonlight disconnect/reconnect does not detach the microphone, and endpoint ID remains stable.
- The microphone remains healthy while DS5 attach/detach and HD Haptics operate.
- A microphone-creation failure does not destroy an existing DS5.
- The VB-CABLE backend still passes its existing test-tone and redirection tests.

### 16.3 Stability acceptance

- Run continuously for 72 hours with no sustained memory, handle, or queue growth.
- Connect/disconnect a Moonlight microphone session 1,000 times without re-enumerating the virtual microphone.
- Open/close the Windows application capture pin 10,000 times without BSOD, pool corruption, or loss of host control.
- After 100 `audiosrv`/`audiodg` restarts, recover automatically or report an explicit recoverable error.
- Produce deterministic outcomes for sleep/resume, Fast Startup, user logout/login, and Sunshine restart.
- After forcibly terminating the Sidecar, recover automatically no more than once and engage the circuit breaker.
- Pass the same stress tests with HVCI/Secure Boot enabled.

### 16.4 Performance targets

- Core PCM queue normally stays near 20 ms and has a hard 60 ms limit.
- `write_mic_pcm()` never waits for pipe or USB completion.
- Audio data produces no observable latency spike in DS5 input/feedback.
- Record measured end-to-end added latency. The first-phase goal is no more than 60 ms of additional queueing latency.

## 17. Release gates

Before promoting the USB virtual microphone from experimental to a default candidate, all of the following must be true:

1. Use an unmodified, validly signed, pinned usbip-win2 version.
2. Known kernel crashes involving local UAC1 attach/detach, pin close, and endpoint purge are fixed in that signed version and covered by reproduction regression tests.
3. Pass the HVCI/Secure Boot and long-duration stability matrix in §16.
4. Complete legal and compatibility review of the release VID/PID, product string, and device-identity policy.
5. Pin the component manifest, source, SHA-256, license, and rollback version. The manifest uses an explicit `hidmaestro_file_version = 1.6.2.0` field that matches the `AssemblyFileVersion` check in `HasPinnedMicrophoneRuntime`.
6. Pass dedicated security regression for protected owner/DACL on the active-component directory, rejection of full-path reparse points, and Sidecar validation-to-launch TOCTOU protection.
7. Let the user explicitly select VB-CABLE fallback; upgrades must not forcibly migrate existing configurations.
8. The GUI clearly distinguishes the user-level host component from the system-level USB/IP transport and explains installation/uninstallation effects before UAC.

Until every condition is met:

- keep the configuration marked experimental;
- do not switch existing users to USB/IP automatically;
- do not remove the VB-CABLE backend;
- do not claim production reliability merely because PoC audio works.

## 18. Implementation order and commit boundaries

Recommended commit sequence:

1. `test: freeze virtual device host protocol contracts`
2. `refactor: share the DS5 sidecar process owner`
3. `refactor: introduce the virtual device registry`
4. `feat: add capability-gated microphone protocol messages`
5. `test: prove HIDMaestro microphone submission`
6. `feat: add the capture-only UAC1 microphone profile`
7. `feat: add the USB/IP microphone sink`
8. `feat: keep the virtual microphone across client sessions`
9. `feat: expose virtual microphone configuration and diagnostics`
10. `test: add USB audio lifecycle stress coverage`

Every step must be independently revertible. Do not change the descriptor, kernel transport version, process lifecycle, and default backend selection in the same commit.

## 19. Questions to resolve before implementation

1. Does HIDMaestro fully support an audio-only profile with no HID interface? If not, first add audio-only device support upstream; do not add a fake HID endpoint to the product design.
2. How will the release VID/PID be obtained, and how will a stable serial/container identity be generated?
3. In Sunshine service mode, how does the Windows session that hosts the process affect visibility of the user's audio endpoint?
4. How stable is the pinned usbip-win2 version when an IN-only UAC device attaches, closes its pin, and tears down?
5. Does the partial-submission, buffer-ownership, and thread-safety contract of `HMMicrophoneInput.Submit()` need to be fixed in writing upstream?
6. What manifest, active-directory, and GUI upgrade compatibility is required when renaming DS5 Sidecar to Virtual Device Host?

These questions do not block phases 0 or 1, but items 1, 2, and 4 must be resolved before the standalone UAC profile enters a user-facing experimental release.

## 20. References

- [HIDMaestro](https://github.com/hifihedgehog/HIDMaestro): source of the current user-mode virtual-device runtime and `HMMicrophoneInput.Submit()` API.
- [usbip-win2](https://github.com/vadimgrn/usbip-win2): upstream project for the currently signed UDE bus.
- [usbip-win2 #181](https://github.com/vadimgrn/usbip-win2/issues/181): record of a kernel pool-corruption risk similar to local emulated UAC endpoint teardown.
- [usbip-win2 #180](https://github.com/vadimgrn/usbip-win2/issues/180): another kernel pool-corruption risk in the same version.
- [Microsoft USB Audio Class system driver (Usbaudio.sys)](https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/usb-audio-class-system-driver--usbaudio-sys-): Windows inbox driver used by the current UAC1 profile; UAC2/`usbaudio2.sys` is only a possible future path.
- [Microsoft UDE client driver](https://learn.microsoft.com/en-us/windows-hardware/drivers/usbcon/writing-a-ude-client-driver): UDE bus/client-driver boundary and signing responsibilities.
