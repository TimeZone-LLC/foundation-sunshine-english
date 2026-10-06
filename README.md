# Foundation Sunshine

Foundation Sunshine is a self-hosted, Moonlight-compatible game-streaming host. This repository is a hard fork of Sunshine focused on reliable streaming infrastructure and a dense, English-only administration experience.

## Capabilities

- Hardware-accelerated video streaming with HDR, Dolby Vision, adaptive bitrate, and optional NVIDIA RTX video enhancements.
- Windows virtual-display management, audio routing, remote microphone support, and virtual input devices.
- Controller profiles, DualSense audio and haptics integration, keyboard, mouse, pen, touch, clipboard, and remote USB support.
- Authenticated pairing, per-client settings, session management, diagnostics, and a native control panel.
- Windows, Linux, and macOS host support, subject to the platform-specific capabilities available on each operating system.

## Build and test

The primary host uses CMake and Ninja. The Web UI is built with npm, and the native control panel has separate npm and Cargo builds.

```text
ninja -C build
ctest --test-dir build --output-on-failure

npm run lint:webui
npm run test:webui
npm run build

npm --prefix src_assets/common/sunshine-control-panel run build:renderer
cargo build --manifest-path src_assets/common/sunshine-control-panel/src-tauri/Cargo.toml --release
```

See `AGENTS.md` before changing the UI or integrating upstream work. It defines the fork's product, visual, language, and merge policies.

## License

Foundation Sunshine retains the licenses and notices of the projects and dependencies from which it is derived. See `LICENSE` and the third-party source directories for details.
