# Dolby Vision Profile 8.4 streaming implementation plan

## 1. Positioning

DV Profile 8.4 has the same structure as the implemented 8.1 path
(`docs/dolby_vision_profile81.md`): a single BL plus RPU, an UNSPEC 62 NAL
injected into the HEVC AU, and direct rendering through the client's
`video/dolby-vision` MediaCodec. The only difference is the base-layer EOTF:

| | 8.1 | 8.4 |
|---|---|---|
| Base layer | PQ (ST 2084) | HLG (ARIB STD-B67) |
| Client `dynamicRangeMode` | 1 | 2 |
| Target devices | Mainstream DV devices | Devices that reject a PQ-BL RPU and accept only an HLG BL, including some LG devices |

Reusable components include the `video_dolby_vision` RPU writer,
`staged_rpu_queue_t`, `video_hdr_bitstream` strip/inject logic, the
`dynamic_hdr_selection` negotiation framework, the NVENC/AMF injection path,
and the existing HLG encoding path (`bt2020hlg` P010). **No new encoder code is
required.**

Confirmed priority rule: when a client advertises both 8.1 and 8.4, negotiate
**8.1 only**. Negotiate 8.4 only when the client advertises only 8.4 and
requests HLG.

## 2. Mutual exclusion with RTX HDR

The RTX HDR pipeline always produces PQ-encoded output. The TrueHDR filter
outputs FP16 linear scRGB, and the later RGB-to-PQ/P010 encoding stage produces
the PQ bitstream. HLG sessions do not activate the filter because `rtsp.cpp`
guards it with `dynamicRange == 1`. Therefore:

- When an app enables `rtx-hdr`, 8.4 **does not participate in negotiation**.
  This does not affect 8.1, whose sessions already require PQ.
- Implement this in the `select_dynamic_hdr` host gate
  `synthetic_hdr_enabled`. The gate must follow the **application feature**
  (`session.synthetic_hdr.enabled`), not the session's filter state. The filter
  is always disabled for an HLG request, so gating on filter state would fail
  to exclude RTX HDR apps on the HLG path. The gate can return an explicit
  `colorspace_unsupported` fallback reason in the ANNOUNCE response header.
- `rtx_hdr`, `synthetic_hdr`, and `pre_encode_filter` represent application
  policy, the session snapshot, and the actual pre-encode processing state,
  respectively. State from a later stage must not change an earlier
  negotiation result.

## 3. Host changes

### 3.1 Negotiation layer (`src/hdr/dynamic_hdr_selection.h/.cpp`)

- Add capability bit `DYNAMIC_HDR_CAPS_DOLBY_VISION_84 = 1u << 4`. Its wire
  value is stable, and old clients will not advertise it.
- Add `dynamic_hdr_format_e::dolby_vision_profile_84 = 5`. The
  `X-SS-Dynamic-HDR` wire value is 5; do not renumber the existing values 0-4.
  Add `"dolby_vision_profile_84"` to `to_string`.
- Do not add a `dynamic_hdr_fallback_e` value. Extend
  `colorspace_unsupported` to mean "PQ required for an HLG session, or HLG
  required for a PQ session."
- For `automatic` or `dolby_vision` preference, evaluate these selection rules
  in order:
  1. A failed HEVC gate returns `codec_unsupported`, unchanged.
  2. `dynamic_range_mode == 1` with the 8.1 capability selects **8.1**,
     preserving current priority.
  3. `dynamic_range_mode == 2` with the 8.4 capability and without the 8.1
     capability selects **8.4**. Advertising both 8.1 and 8.4 while requesting
     HLG is contradictory and falls back with `colorspace_unsupported`: the
     8.1 bit means the client should expect PQ. A client requesting HLG must
     omit the 8.1 bit.
  4. Active per-app RTX HDR fails the 8.4 gate with
     `colorspace_unsupported`.
  5. The direct-surface gate is unchanged.
- Make `dynamic_hdr_selection_t::dolby_vision_active()` true for either 8.1 or
  8.4.
- Add `1u << 4` to the `known_bits` mask.

### 3.2 Encoding session (`video.cpp` / `video_dolby_vision`)

- The current injection gate requires negotiated DV, an available analyzer,
  and PQ. Dispatch by negotiated format instead:
  - 8.1 requires a PQ colorspace, as it does now.
  - 8.4 requires an HLG colorspace: `dynamicRange=2` with `bt2020hlg` P010.
- The RPU writer does not change. L1 scene-light min/mid/max, L5, and L6 are
  independent of the base-layer EOTF. Reuse the path that configures L6 from
  the host display's original mastering metadata.
- Keep the injection position unchanged: inject the UNSPEC 62 NAL on the
  encoder output side using the encoder `frame_index`.
- Session log example:
  `NVENC: Dolby Vision Profile 8.4 active (HLG base layer, mastering peak N nits)`.

### 3.3 Signaling

An 8.4 session uses the existing 10-bit `bt2020hlg` encoding path. HDR10+ and
Vivid SEI handling differs: HDR10+ requires PQ and must not be sent in parallel
for 8.4. The Dual Carry decision applies only to 8.1/PQ. Vivid HLG may remain
available through the existing `vivid_hlg` enum when the analyzer is available.

## 4. Client side (`moonlight-common-c` fork and `moonlight-vplus`)

- In the continuing common-c microphone branch, pass through the capability
  bit `DYNAMIC_HDR_CAPS_DOLBY_VISION_84 = 1 << 4`.
  `LiGetNegotiatedDynamicHdrFormat()` can continue passing the integer without
  interpreting it.
- In V+:
  - Extend the strict existing `DvheSt` capability probe with an HLG-BL check:
    verify that the device's DV decoder accepts `dvhe.08.04` through
    `video/dolby-vision` with HLG-signaled content. The result determines
    whether the client advertises only 8.4 or both 8.1 and 8.4.
  - When the user selects the HLG DV option, request `dynamicRangeMode=2` and
    advertise only the 8.4 bit.
  - Add "DV 8.4" beside DV 8.1 in the HDR-mode UI, visible only when the probe
    succeeds. Reuse the existing `StreamSettings` logic that rebuilds
    `list_hdr_mode`, including the `foundDolbyVision` branch.
  - Reuse the decoder routing from issue 538: negotiated format 5 also uses
    `video/dolby-vision` plus `DvheSt`.
- Merge in the same order as 8.1: common-c, then V+.

## 5. Phases and gates

| Phase | Work | Go/No-Go gate |
|---|---|---|
| 0 | Validate an RPU 8.4 variant with dovi_tool 2.3.3: generate an RPU binary for HLG-BL session parameters, parse it with `info -s` and `info -f`, verify CRC round-trip, and confirm that the RPU header does not require base-layer-specific rewriting | Proceed to Phase 1 only if this passes |
| 1 | Host negotiation layer and unit tests for priority, contradictory advertisements, RTX HDR exclusion, and old-client compatibility | CI passes |
| 2 | Host encoder injection and full `test_sunshine` suite | CI passes |
| 3 | common-c and V+ client changes | Build succeeds and probe logs are correct |
| 4 | Device end-to-end test: activate DV on an 8.4-only device, observe `negotiated:5` in logcat and the DV decoder route, and verify no regression on 8.1 devices | Milestone |

### Phase 0 result (2026-09-01: Go)

The existing writer generated RPU binaries with the golden configuration
(peak 1000, min 1, CLL 800, FALL 320, L5 16/16, and L1 9/3200/1500) for both
`scene_refresh` variants. dovi_tool 2.3.3 verified the following:

- `info -s` parsed both variants as complete Profile 8 / CM v2.9 RPUs with
  L1, L5, and L6. The RPU itself contains no base-layer EOTF information;
  "Profile: 8" does not distinguish 8.1 from 8.4.
- A field-by-field comparison after `editor` mode 4, dovi_tool's 8.1-to-8.4
  conversion, showed that it **only rewrites the `rpu_data_mapping` curve**
  from a PQ identity mapping to an eight-segment HLG-domain compensation curve,
  plus the CRC. L1, L5, and L6 remain bit-identical. That conversion targets
  PQ-source transcoding to HLG and compensates for the pixel-domain conversion.
- This implementation uses **native HLG encoding**: analyzer statistics pass
  directly through the HLG OETF. There is no domain conversion to compensate,
  so the identity mapping plus L1, L5, and L6 is the correct native 8.4 RPU.
- **Conclusion: the RPU writer does not change.** An 8.4 session reuses
  `rpu_generator_t` and the injection path directly. Phase 4 device testing
  closes final correctness by observing actual DV-engine mapping behavior.

## 6. Risks

- **RPU compatibility with an HLG BL**, resolved by Phase 0: the handwritten
  template was based on a verified 8.1 golden stream. If dovi_tool had shown
  that an HLG source required a different mapping header, Phase 0 would have
  stopped and forced a redesign.
- **Contradictory client advertisement**, meaning both 8.1 and 8.4 plus an HLG
  request: the host rejects 8.4 with `colorspace_unsupported`, leaving the HLG
  session as ordinary HLG without dynamic metadata. HDR10+ requires PQ, so
  there is no intermediate fallback. This conservative behavior avoids invalid
  output.
- **Old host with a new client**: the client advertises bit `1 << 4`; an old
  host masks it off and retains current negotiation behavior.
- **Device coverage**: Phase 4 requires an 8.4-only target device as a positive
  case. Existing OPPO PKJ110 and Sony TV coverage is sufficient for the 8.1
  regression case.

## 7. Initial release specification

- Negotiated format: `dolby_vision_profile_84`, wire value 5.
- Encoding: HEVC Main10, `bt2020hlg`, P010, with the 8.1 RPU injection path.
- Exclusion: do not negotiate 8.4 when per-app `rtx-hdr=on`.
- Fallback: any failed gate falls directly to the current HLG session without
  dynamic metadata. An HLG session has no HDR10+ intermediate fallback.
