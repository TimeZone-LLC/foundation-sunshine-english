# Contributing

Read `AGENTS.md` before changing the product or integrating upstream work. It defines this hard fork's functional scope, English-only policy, and visual constraints.

## Web UI

The Web UI is implemented with Vue and Vite under `src_assets/common/assets/web`.

```text
npm run lint:webui
npm run test:webui
npm run build
```

The only user-interface resource file is `src_assets/common/assets/web/public/assets/locale/en.json`. Add or edit English strings there, then run:

```text
npm run i18n:format
npm run i18n:validate
```

Do not add locale negotiation, translated resource files, external translation services, or language selectors.

## Native host

Use the repository's CMake preset or existing build directory, then run the native test suite:

```text
ninja -C build
ctest --test-dir build --output-on-failure
```

Keep platform-specific behavior behind the existing Windows, Linux, and macOS abstractions. New asynchronous or privileged code needs explicit lifetime, cancellation, and authorization tests.

## Control panel

The control panel is a separate Git repository embedded as a submodule. Commit its changes independently from the host repository.

```text
npm --prefix src_assets/common/sunshine-control-panel run test:renderer
npm --prefix src_assets/common/sunshine-control-panel run build:renderer
cargo test --manifest-path src_assets/common/sunshine-control-panel/src-tauri/Cargo.toml
cargo build --manifest-path src_assets/common/sunshine-control-panel/src-tauri/Cargo.toml --release
```

## Formatting and review

- Keep changes scoped and preserve unrelated working-tree edits.
- Run the relevant formatter and `git diff --check` before committing.
- Add focused tests for correctness, protocol compatibility, security boundaries, and regressions.
- For visual work, inspect the running interface in both relevant states rather than relying only on a successful build.
