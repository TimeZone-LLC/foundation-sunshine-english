# Dolby Vision Profile 8.1 streaming implementation plan

## 1. Positioning

Dolby Vision 8.1 is **not a quality upgrade over HDR10+**. It is a parallel
path for native output on supported displays.

Both paths use the same HEVC Main10, BT.2020, PQ, 10-bit base image. They differ
only in the dynamic-metadata envelope and in **which component performs tone
mapping**:

| | HDR10+ | Dolby Vision Profile 8.1 |
|---|---|---|
| Base video | HDR10 PQ / BT.2020 / 10-bit | Same |
| Dynamic metadata | SMPTE ST 2094-40 | Dolby Vision RPU |
| Metadata carrier | HEVC prefix SEI (T.35) | HEVC UNSPEC 62 NAL |
| Mapping executor | Client shader or device HDR10+ engine | Device Dolby Vision engine |
| HDR10 fallback | Supported | Supported, with BL compatibility ID 1 |

The value of this path is therefore not additional picture data. It covers
endpoints that support Dolby Vision but not HDR10+, including devices in the
LG, Sony, Xiaomi, and OPPO ecosystems, and delegates mapping to the endpoint's
existing Dolby engine.

If a client instead parses the RPU in a custom shader and performs its own
mapping, this path has no advantage. Improving the existing HDR10+ path is more
valuable in that case.

### Initial scope

```text
Foundation Sunshine / Windows
        -> HEVC Main10, BT.2020, PQ, HDR10-compatible 10-bit base layer + Profile 8.1 RPU
Moonlight V+ / Android
        -> video/dolby-vision MediaCodec -> Direct Surface
Endpoint Dolby Vision engine performs mapping
```

Explicitly out of scope: AV1 Dolby Vision, HLG Profile 8.4, Profile 7 dual-layer
or FEL output, manually authored L2/L8 trims, coexistence with
FrameGen/ImageReader/Vulkan post-processing, a 4K120 launch requirement, and
non-Android "pseudo-native Dolby Vision."

---

## 2. Design principles

### 2.1 Separate the base format from the dynamic-metadata format

Keep the current `formats_t` model. The base video mode (SDR, HDR10-PQ, or HLG)
is orthogonal to the dynamic-metadata format (NONE, HDR10_PLUS, VIVID_PQ,
VIVID_HLG, or DOLBY_VISION_PROFILE_81).

Represent Dolby Vision 8.1 in the protocol as:

```text
base_hdr_mode      = HDR10_PQ      (dynamicRangeMode remains 1)
dynamic_hdr_format = DOLBY_VISION_PROFILE_81
codec              = HEVC
```

Do not add an `hdrMode = 4` value that would break the old protocol. An older
Sunshine build must not interpret an unknown value as HLG or SDR.

### 2.2 Select one dynamic HDR format for each session

Make the selection once while establishing the connection. Do not switch
formats inside a running decoder session. A format change or fallback requires:

```text
stop video session -> renegotiate -> recreate decoder -> request IDR -> resume playback
```

Implementation note: when DV is active, Sunshine **also sends HDR10+ SEI**. This
is established HDR Dual Carry practice: ST 2094-40 and a DV RPU can coexist in
one bitstream. A DV decoder ignores the SEI, an HDR10+ device ignores the RPU,
and the fallback path is already present. The "one format" constraint prohibits
mid-session switching; it does not require mutually exclusive carriage.

### 2.3 Dolby Vision requires Direct Surface

Native DV is useful only when the endpoint Dolby engine owns output. Therefore
`MediaCodec -> renderTarget.surface` is the only valid path. The client must
hard-disable its `framegenSurface` and ImageReader relay paths in DV mode.

---

## 3. Host changes

The existing infrastructure is sufficient. **Do not rewrite the HDR capture
and analysis pipeline.** It already provides:

- D3D11 GPU luminance analysis and reduction shaders in `display_vram.cpp`;
- synchronous analysis during PQ/P010 conversion;
- per-frame peak, average, and nine percentile values in
  `platf::hdr_frame_luminance_stats_t`;
- HDR10+ ST 2094-40 and CUVA serialization in `video_hdr_metadata.h`; and
- encoder side data and post-encode bitstream injection in
  `video_hdr_bitstream.h`, already used by the AMF path.

Only a new output backend is required.

### 3.1 RPU generation module

Add `src/video_dolby_vision.h` and `.cpp` in namespace
`video::dolby_vision`.

**Backend choice: a native C++ writer with no libdovi linkage.** Reasons:

1. This repository deliberately confines Rust and Cargo to the optional Tauri
   GUI. `cmake/packaging/FetchGUI.cmake` exists specifically to remove Rust from
   the main build. Linking libdovi would make Rust a required dependency.
2. The repository already has handwritten CUVA T.35 serialization and
   `detail::bit_writer_t`.
3. A no-op Profile 8.1 mapping RPU is a fixed template. Each frame changes only
   the three 12-bit L1 values and `scene_refresh_flag`, then recomputes the
   trailing CRC. This makes steady-state zero-allocation operation easier than
   the libdovi C API, which constructs Rust objects and returns an owned `Vec`
   for every frame.

Use libdovi and `dovi_tool` only for **development and CI cross-validation** by
parsing golden RPUs offline with `dovi_tool info`. The real-time path must not
write temporary JSON, launch subprocesses, or perform disk I/O.

#### Interface

```cpp
struct session_config_t {
  uint16_t source_mastering_peak_nits = 1000;
  uint16_t mastering_min_nits_x10000 = 1;   // L6 minimum, in 1/10000 nit
  uint16_t max_cll_nits = 0;
  uint16_t max_fall_nits = 0;
  uint16_t active_area_left = 0, right = 0, top = 0, bottom = 0;  // L5
};

struct frame_metadata_t {
  uint16_t min_pq = 0;   // 12-bit PQ code value
  uint16_t avg_pq = 0;
  uint16_t max_pq = 0;
  bool scene_refresh = false;
};

class rpu_generator_t {
public:
  bool configure(const session_config_t &config);   // Build both templates at session start.
  /// Return a complete UNSPEC 62 NAL, including the 0x7C 0x01 header and
  /// emulation-prevention bytes. The internal buffer remains valid until the
  /// next generate() or reset().
  std::span<const uint8_t> generate(const frame_metadata_t &metadata);
  void reset();
};

/// Derive L1 metadata. The average uses stats.avg_maxrgb_pq, which is a PQ-domain
/// mean. The maximum uses percentile_99 for outlier protection. With extended
/// near-black statistics, report zero only when near-black coverage reaches 1%;
/// otherwise use percentile_1_pq. Old analyzer results fall back to
/// percentile_10_pq. Clamp every value afterward.
std::optional<frame_metadata_t>
frame_metadata_from_stats(const platf::hdr_frame_luminance_stats_t &stats);

/// Smooth min, average, and max consistently between scene boundaries. Call
/// reset() at a scene cut so history from the old scene cannot trail into it.
class level1_temporal_filter_t { ... };

/// Fixed-capacity queue of in-flight RPUs keyed by frame_id. Encoder output
/// retrieves the matching RPU by frame_index. Exceeding the maximum number of
/// in-flight frames fails; the caller must stop DV instead of attaching an RPU
/// to the wrong frame.
class staged_rpu_queue_t { ... };
```

#### Minimum RPU content

The initial implementation emits only L1 with per-frame dynamic minimum,
average, and maximum PQ; L5 with an all-zero active area by default; L6 with
source mastering luminance and MaxCLL/MaxFALL; and the required version and
compatibility data.

Do not emit L2, L3, L8, authored trims, an enhancement layer, or a non-zero
reshaping residual.

Fixed parameters aligned with dovi_tool's `p8_default()` and
`Profile81::dm_data()`:

```text
rpu_type = 2, rpu_format = 18
vdr_rpu_profile = 1, vdr_rpu_level = 0
coefficient_log2_denom = 23, vdr_rpu_normalized_idc = 1
bl_bit_depth_minus8 = 2, el_bit_depth_minus8 = 2, vdr_bit_depth_minus8 = 4
disable_residual_flag = 1        -> EL present = 0
vdr_dm_metadata_present_flag = 1 -> RPU present = 1
mapping: one polynomial segment per component, poly_coef_int = {0, 1},
         poly_coef = {0, 0}
         -> identity mapping; removing the RPU leaves a normal HDR10 base layer
signal_eotf = 65535, signal_bit_depth = 12, signal_full_range_flag = 1
source_diagonal = 42
```

The CRC is CRC-32/MPEG-2 with polynomial 0x04C11DB7, initial value 0xFFFFFFFF,
no reflection, and no output XOR. It covers every byte after the `0x19` prefix
and before the CRC. Append a `0x80` terminating byte afterward.

**Why a template needs almost only an L1 patch:** L1 has a fixed length in the
extension-block list: five bytes, with 36 meaningful bits plus four alignment
bits. Blocks are sorted by level, and all fields written before L1 have lengths
that do not depend on L1 values. The bit offset of L1's 36 bits is therefore
constant. After patching it, only the CRC must be recomputed.

The only exception is `scene_refresh_flag`, which is `ue(v)`: 0 encodes in one
bit and 1 in three bits. Byte-alignment padding after `num_ext_blocks` happens
to absorb the two-bit difference, so both variants have the same total length
and L1 offset. That is incidental and must not become an assumption.
`configure()` still builds **two templates**, one for each scene-refresh value,
and precomputes the L1 bit offset for each.

### 3.2 Deriving L1 metadata

Current statistics cannot be copied mechanically into DV semantics:

- **Do not use the absolute minimum pixel for `min_pq`.** One black UI pixel,
  letterbox bar, or transparent composition region would pin the whole frame to
  zero. The analyzer provides both the first PQ percentile and coverage of the
  first PQ histogram bin. Report zero only when coverage reaches 1%; otherwise
  use the first percentile. Fall back to P10 for old analyzer results without
  extended statistics.
- **`avg_pq` must be a PQ-domain mean, not `average_maxrgb`.** PQ is concave, so
  `PQ(mean(nits)) >= mean(PQ(nits))`. In a dark scene with highlights, the
  difference can span much of the dynamic range. `stats.avg_maxrgb_pq` is the
  required per-pixel PQ-domain average.
- **Retain current outlier protection for `max_pq`.** Use the 99th percentile,
  `stats.percentile_99`, rather than the true peak. Specular overshoot on a
  Windows compositor scRGB surface is unbounded; a few pixels could otherwise
  pin an entire frame to 10,000 nits. Keep the true peak for diagnostics.

Implement the legal domain from dovi_tool's `clamp_values_int()`:

```text
min_pq in [0, 12]
max_pq in [2081, 4095]
avg_pq in [819 for CM v2.9, or 1229 for CM v4.0, max_pq - 1]
```

### 3.3 Do not write client peak luminance into L1

The current HDR10+ path carries
`targeted_system_display_maximum_luminance`, which belongs to the ST 2094-40
model. Do not copy that behavior into DV. The RPU describes the content; the
endpoint Dolby engine already knows the display's capabilities.

```text
source_mastering_peak -> content and host metadata, written to L6
client_display_peak   -> capability selection and diagnostics only
```

### 3.4 HEVC RPU injection

Add an UNSPEC 62 injection path to `video_hdr_bitstream`. Unlike the current
T.35 prefix-SEI path, which inserts before the first VCL NAL, place the RPU as
the final NAL for the picture. dovi_tool deliberately moves RPUs there for
better player and device compatibility.

Processing rules:

1. Parse the complete access unit.
2. Remove any existing UNSPEC 62 RPU from the AU for idempotence.
3. Preserve VPS, SPS, PPS, AUD, prefix SEI, and VCL NALs.
4. Insert exactly one RPU for each coded picture, including pictures with
   multiple slices.
5. Bind the RPU to the same `frame_id` that generated it.
6. Preserve the Annex-B start-code style.
7. Fail on malformed NALs, truncated input, or an oversized RPU instead of
   emitting a damaged bitstream.

### 3.5 Strict RPU-to-encoded-frame binding

Build an explicit mapping. **Do not assume that the Nth capture corresponds to
the Nth encoder callback:**

```text
capture frame_id -> GPU analysis frame_id -> encoder opaque/timestamp
                 -> output packet frame_id -> matching RPU -> matching AU
```

An encoder may drop frames, delay output, change output cadence around an IDR
rebuild, flush its queue during bitrate reconfiguration, or emit several
packets for one frame.

Reuse existing mechanisms. The AMF path's `staged_metadata_units`, a
`frame_index`-keyed map with a `MAX_TRACKED_FRAMES` limit, already follows this
pattern. NVENC round-trips the frame number through
`pic_params.inputTimeStamp = frame_index` and
`lock_bitstream.outputTimeStamp`.

If an item remains unmatched beyond the maximum in-flight frame count, log an
error and stop Dolby Vision. **Never attach an old RPU to a new picture.** A
one-frame mismatch is worse than omitting dynamic metadata: explosions,
highlight flashes, and fast cuts would cause visible luminance pumping.

### 3.6 Encoding-session integration, implemented

`rpu_injector_t` in `video_dolby_vision.h` is the session-level DV state
machine:

```text
make_nvenc/amf_encode_session
    `- when DV is negotiated, the analyzer is available, and the final
       colorspace is PQ, configure L6 from the host display's original
       mastering metadata before any client-peak adjustment
encode_frame(frame_index)                         submission side
    `- stage(frame_index, hdr_luminance_stats)
       reuse the last valid values when statistics are missing; before the
       first valid sample, omit the RPU, matching the HDR10+ cold-start behavior
encode_nvenc/amf output side
    `- inject(encoded_frame.frame_index, data)
       retrieve by the encoder's output frame number and append one RPU as the
       AU's final NAL
in-flight queue overflow
    `- stop DV for this session and clear the queue; no RPU is safer than a
       mismatched RPU
```

Limit support to the native NVENC and AMF paths. The avcodec software/QSV path
cannot expand an AVPacket in place, so injection is not viable. If negotiation
selects DV but encoding falls back to an avcodec encoder, log a warning and
continue sending HDR10; the base layer is already compatible. Also omit the RPU
when the analyzer is unavailable or the final colorspace is not PQ.

---

## 4. Protocol negotiation

### 4.1 Client capabilities

Use a bit mask in the existing extension protocol rather than adding a field
for every format:

```text
dynamic_hdr_caps:
  bit 0 = HDR10+
  bit 1 = HDR Vivid PQ
  bit 2 = HDR Vivid HLG
  bit 3 = Dolby Vision Profile 8.1

also send:
  dolby_vision_native_surface   (client confirms Direct Surface output)
```

The proposed `dolby_vision_max_level` field was removed. The host does not gate
on level, the client's `isFormatSupported` probe at the actual resolution and
frame rate is stronger, and device level reporting is unreliable.

**Implemented wire format**, carried in RTSP ANNOUNCE SDP by
`src/hdr/dynamic_hdr_selection.h`:

```text
client -> host, ANNOUNCE SDP a= lines; omitted fields keep legacy semantics:
  x-ss-video[0].dynamicHdrCaps             bit mask above; host masks unknown bits
  x-ss-video[0].dolbyVisionDirectSurface   0/1
  x-ss-video[0].dynamicHdrPreference       0 automatic / 1 DV / 2 HDR10+ / 3 HDR10 only

host -> client, ANNOUNCE 200 response headers:
  X-SS-Dynamic-HDR:            <0-4>  selected format: 0 none, 1 HDR10+,
                                        2 vivid_pq, 3 vivid_hlg,
                                        4 dolby_vision_profile_81
  X-SS-Dynamic-HDR-Fallback:   <enum name>  present only when a DV-capable or
                                             DV-requesting client did not get DV

There is no host-side switch. Negotiation always runs when the client reports
capabilities. The switch used during validation was removed after end-to-end
activation on a Sony TV. The negotiation model is the safety boundary: a
client that does not advertise DV can never negotiate DV, and every failed
gate falls directly to HDR10.
```

Compatibility rule: **old clients that do not report capability bits retain
existing behavior**. Sunshine continues to send HDR10+ unconditionally, as
previous releases did. Only a client that explicitly reports capability bits
can be negotiated down; for example, a mask without the HDR10+ bit selects
plain HDR10. Malformed parameters as a whole fall back to legacy semantics so
one bad field cannot silently downgrade a client.

### 4.2 Host selection logic

```cpp
if (client.dv81 && codec == HEVC && colorspace == BT2020_PQ && bit_depth == 10 &&
    client.direct_surface && !client.frame_generation &&
    host.rpu_generator_available && host.hevc_packet_injection_available) {
  selected = DOLBY_VISION_PROFILE_81;
} else if (client.hdr10plus && host.hdr10plus) {
  selected = HDR10_PLUS;
} else {
  selected = NONE;  // plain HDR10
}
```

Priority for `automatic` is DV 8.1, then HDR10+, then HDR10. An explicit HDR10+
preference uses HDR10+ then HDR10. An explicit Dolby Vision preference uses
**DV 8.1 then HDR10 directly**. The client's DV request carries only the DV
capability bit; any failed host gate selects plain HDR10 without an HDR10+
intermediate step.

The server must return the final `selected_dynamic_hdr_format`,
`selected_dolby_vision_profile`, `selected_dolby_vision_level`, and
`fallback_reason`.

---

## 5. Client changes in Moonlight V+, outside this repository

### 5.1 Capability probing

**Display capability:** inspect the active output display rather than whether
the device has ever supported DV. Combine this with the HDR types supported by
the current display mode so an internal phone panel cannot incorrectly enable
DV for an unsupported external display.

**Decoder capability:** find
`MediaFormat.MIMETYPE_VIDEO_DOLBY_VISION`, or `video/dolby-vision`. Profile 8
uses `MediaCodecInfo.CodecProfileLevel.DolbyVisionProfileDvheSt`.
**Do not use `DolbyVisionProfileDvheDtb`; that is Profile 7.**

Reuse the strict probe from `HdrDecoderProfileSelector`, testing resolution,
frame rate, profile, and colorspace together. Support for 4K120 in an ordinary
HEVC decoder does not prove that the DV decoder supports it.

### 5.2 The Dolby Vision configuration record is the first technical gate

Do not assume every vendor decoder recognizes Profile 8.1 solely from
`KEY_PROFILE = DolbyVisionProfileDvheSt`. The configuration record must at
least express:

```text
dv_profile = 8, dv_level = value appropriate for the resolution and frame rate
rpu_present_flag = 1, el_present_flag = 0, bl_present_flag = 1
dv_bl_signal_compatibility_id = 1
```

Phase 0 must test all three initialization forms:

1. `video/dolby-vision` with Profile/Level, receiving Annex-B input with RPUs.
2. Profile/Level with an explicit Dolby Vision configuration record.
3. Vendor-required combinations of VPS/SPS/PPS CSD and the configuration record.

**Do not begin broad changes to Sunshine's dynamic RPU generation until this
gate passes.**

### 5.3 Disable conflicting features

When DV 8.1 is active, disable FrameGen so `framegenSurface` is null, ImageReader
relay, client HDR tone mapping, Vulkan/GL color remapping, client-side sharpening
post-processing, and post-decode screenshot analysis.

Keep precise PTS synchronization, MediaCodec timed release, Surface frame-rate
matching, system-composited control menus, a separate performance overlay,
input, and haptics. The overlay must be a separate Surface or UI layer; do not
burn UI into the video and re-output it.

### 5.4 Runtime state and fallback

```text
CAPABILITY_CONFIRMED -> CODEC_CONFIGURING -> WAITING_FIRST_FRAME -> DOLBY_VISION_ACTIVE
```

On configure/start failure, a CodecException after RPU input, first-frame
timeout, unexpected output format, or repeated decode failure, record a device
failure signature, end the video session, and reconnect with HDR10+.

The signature includes `Build.FINGERPRINT`, codec name, display ID, display-mode
ID, resolution, fps, profile, level, and error code. Cache it and do not retry
DV during the current application lifetime, avoiding a reconnect loop.

### 5.5 Diagnostics

DV should **not be assumed** to expose a per-frame output query equivalent to
HDR10+'s `KEY_HDR10_PLUS_INFO`. Instead record the requested and negotiated
formats, display support, codec name, profile, level, whether a configuration
record was supplied, Direct Surface state, first-frame success, output Format
summary, codec error code, and enumerated fallback reason.

---

## 6. Implementation phases and pull-request split

### Phase 0: device feasibility, first Go/No-Go gate

Build a known-good Profile 8.1 test stream with an HDR10 HEVC Main10 base layer
and a fixed valid RPU. Play it locally through the same Moonlight V+ MediaCodec
input code, without networking. Verify that `video/dolby-vision` configures,
`DvheSt` is accepted, RPU NALs are accepted, the first frame appears, the
display actually enters DV, Direct Surface works, 1080p60 and 4K60 both work,
an external display is probed independently, and removing RPUs leaves a valid
HDR10 stream.

### Phase 1: Sunshine RPU transport proof, host side complete

Start with a fixed valid RPU instead of dynamic quality. Add the RPU writer,
HEVC injector, feature flag, and golden-bitstream tests. Acceptance requires
exactly one RPU per AU, successful parsing of every RPU by `dovi_tool info`, a
valid HDR10 base layer after stripping RPUs, continued decoding after network
fragmentation, retransmission, and IDRs, and no added frame of buffering.

Implemented: native RPU writer cross-validated with dovi_tool, HEVC injector and
golden tests, and NVENC/AMF encoding-session integration from section 3.6.
**Networking and the client path were validated end to end** on 2026-08-25 with
an OPPO device decoder path and a Sony TV entering Dolby Vision. The host-side
staging switch was removed after validation, and negotiation now always runs.

### Phase 2: real-time L1 metadata, implemented with encoder integration

Connect GPU analysis data and add robust dark-region statistics, scene-cut
detection, peak-outlier suppression, first-frame warm-up, conservative behavior
when statistics are absent, strict RPU/`frame_id` matching, and zero-allocation
optimization.

Implemented: derivation and clamping of average, maximum, and minimum as in
section 3.2; reuse of the last valid values when statistics are missing;
warm-up omission before the first sample; strict `frame_index` binding;
shutdown on queue overflow; and steady-state zero allocation. A dedicated
temporal filter smooths L1 min/average/max. Independent GPU-sample PQ means,
P10/P90, and the HDR10+ percentile distribution determine `scene_refresh`.
The filter clears history before a cut, and reusing one analysis sample does
not produce another refresh.

Remaining device tuning: calibrate scene-cut and near-black coverage thresholds
by game type.

### Phase 3: formal negotiation and fallback, negotiation layer complete

Add client DV capabilities, host dynamic-HDR selection, final-selection
reporting, automatic HDR10+ fallback, device-failure caching, performance
overlay state, logs, and diagnostic export.

Implemented: the wire format and selection logic in section 4.1. Client work
remaining: device-failure caching and reconnect fallback from section 5.4, and
the diagnostics UI from section 5.5.

### Phase 4: quality tuning and device matrix

Compare DV 8.1, HDR10+, and HDR10 on **the same device**. Do not infer format
quality by visually comparing different devices.

### Recommended pull-request order

1. Protocol: dynamic-HDR capabilities and selection result.
2. Sunshine: RPU writer, half of host Phase 1.
3. Sunshine: HEVC RPU injector and golden tests.
4. V+: Dolby Vision capability probe.
5. V+: `video/dolby-vision` Direct Surface decoding.
6. End-to-end fixed-RPU experiment mode.
7. Sunshine: real-time L1 RPU.
8. Runtime fallback, failure cache, and diagnostics.
9. Device compatibility and image-quality tuning.
10. Licensing, naming, and release gate.

---

## 7. Testing and acceptance

### 7.1 Bitstream correctness

Profile 8; BL compatibility ID 1; RPU present 1; EL present 0; BL present 1;
one RPU per picture; no duplicate NAL 62; complete libdovi/`dovi_tool` parsing;
idempotent injector output; valid HDR10 decoding after stripping every RPU;
correct behavior after VPS/SPS/PPS repetition and IDR; no repeated injection for
a multi-slice frame; and no RPU mismatch when the encoder drops a frame.

### 7.2 Image-quality test patterns

Use a 0-5 nit near-black staircase; 1%, 5%, 10%, and 25% white windows;
100/400/600/1000/2000/4000 nit highlights; a 0-10,000 nit PQ ramp; a few
extremely bright pixels; a large bright area; night scenes and neon; rapid
bright/dark cuts; a static HUD over dynamic gameplay; Windows SDR UI over an
HDR game; letterbox bars; subtitles; and transparent windows.

Check for raised black levels, crushed shadows, early highlight roll-off,
flicker at scene cuts, breathing HUD brightness, unexplained global darkening,
and instability in long static scenes.

### 7.3 Internal performance gates

These are engineering acceptance targets, not Dolby requirements:

```text
additional full-frame GPU passes             0
additional post-decode image copies          0
additional video-frame queues                0
steady-state heap allocations per frame      0
RPU generation + injection P95               < 0.2 ms
RPU/frame mismatch count                     0
host latency increase relative to HDR10      <= 0.2 ms
```

### 7.4 Device compatibility matrix

Cover DV phones using two different SoCs; one Android TV or Google TV device;
a phone's internal display; a USB-C/HDMI external display; a device that
supports DV but not HDR10+; a device that supports both; an HDR10-only device;
a device that incorrectly advertises a DV profile; and 1080p60, 1440p60, and
4K60. Probe 120 Hz capability but do not promise it in the initial release.

---

## 8. Risk order

1. **Android MediaCodec initialization, highest risk.** A device may advertise
   DV and expose `video/dolby-vision` in CodecList while still failing to start
   a raw Annex-B Profile 8.1 stream. Validate the configuration record, CSD,
   and Surface path first.
2. **Current statistics do not exactly match DV L1 semantics.** HDR10+
   `average_maxrgb` and percentile peaks cannot be copied mechanically into
   `avg_pq` and `max_pq`. Cross-check golden vectors, offline tools, and real
   device output.
3. **RPU-to-picture mismatch.** A one-frame error is worse than no dynamic
   metadata.
4. **Post-processing conflicts.** FrameGen, ImageReader, client tone mapping,
   and native endpoint DV conflict architecturally. Make them mutually
   exclusive for the first release instead of trying to combine them.
5. **Technical operation is not certification.** Generating a valid RPU does
   not grant Dolby product authorization or certification. Before public use of
   the Dolby Vision name or a claim of official support, complete Dolby product
   licensing, technical delivery, and test-approval requirements.

---

## 9. Initial release specification

```text
platform: Windows Sunshine -> Android Moonlight V+
encoding: HEVC Main10 / BT.2020 / PQ / 4:2:0 10-bit
Dolby: Profile 8.1, single-layer HDR10-compatible BL, dynamic L1, static L5/L6,
       no EL, no authored trim
display: MediaCodec video/dolby-vision, Direct Surface
resolution: 1080p60 / 1440p60 / 4K60
fallback: Dolby Vision 8.1 -> HDR10 directly; a client DV request advertises only DV
```

Product setting in the **Moonlight V+ client UI**: format selection is a
device-specific decision. One host can serve clients with different
capabilities, and each client reports its preference. `dynamicHdrPreference`
is the wire representation of this UI:

```text
Dynamic HDR
  [x] Automatic             Prefer the device's native dynamic HDR
  [ ] Dolby Vision 8.1      Use native device Dolby Vision mapping (experimental)
                            Incompatible with frame generation and client post-processing
  [ ] HDR10+                Use HDR10+ dynamic metadata
  [ ] HDR10                 Use static HDR
```

Host UI decision: **do not provide a format-selection screen**. The
`dolby_vision` staging switch was removed after activation on a Sony TV. The
negotiation model is the safety boundary: a client that does not advertise DV
can never negotiate it. The client shows fallback reasons, failure cache, and
compatibility messages as described in sections 5.4 and 5.5.

**The correct first step is not to write dynamic RPUs in Sunshine.** First use
a known-good Profile 8.1 stream to validate Moonlight V+'s
`video/dolby-vision -> Direct Surface` path. After that gate passes, reuse the
existing HDR10+ GPU analysis and HEVC injection architecture for real-time L1
RPUs.

The host foundation in section 3 is the part this repository can complete
independently, is a prerequisite for every later pull request, and defines the
scope of this implementation round.
