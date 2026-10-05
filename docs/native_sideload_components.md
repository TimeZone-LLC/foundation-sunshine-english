# Native sideloaded component contract

## Ownership

The control panel owns user operations, file import, privileged installation when required,
recovery, and status display. Core owns the trusted versions, loading, usage rights, and
shutdown of its functional components. The control panel does not execute Core vendor
runtimes, and a successful installation does not imply that the feature ran successfully.

Project adapter layers are part of the host implementation, not user-sideloaded files. Vendor
code is integrated through explicit interfaces. Only C ABI data and opaque handles cross
compiler boundaries; allocation, destruction, and exception handling remain on the same side.

## Files and versions

Code registers component IDs, file roles, target filenames, and permitted sources. The host's
build inputs determine which runtime digests it supports, and those digests are compiled into
the host binary. The control panel queries supported versions from the running host; local
installation records cannot expand that trust set.

Configuration and transaction journals may use JSON. They record selections and installation
results; they are not allowlists for code execution. Vendor-file identity is separate from the
adapter build identity: rebuilding the host does not require a vendor file to be imported again
while the new host still supports it.

## Installation and runtime

1. The control panel queries host capabilities and supported file digests, then preflights the
   files selected by the user.
2. It obtains a maintenance operation from the host. A component in use cannot be replaced or
   removed.
3. When a protected directory must be written, it starts the installation helper through UAC;
   the helper queries the host again and validates the staged files.
4. It commits the files and host configuration, then ends maintenance. If the response is lost,
   it queries the effective configuration to decide which way recovery must proceed.
5. After the feature is enabled, the host validates the adapter component and vendor files again
   before loading, and retains file and module references for the lifetime of the instance.
6. After the final user exits, the host shuts down vendor shared state only when that operation
   is safe. Runtimes that cannot be unloaded safely remain loaded until the host exits.

Digest checks, installation probes, and management API requests must not enter per-frame
processing paths. Disabled components do not initialize vendor runtimes. A single session must
not shut down shared library global state. Closing the management page is not equivalent to
ending a Core feature session.

The DLSS NR NGX runtime remains mapped in the Sunshine process after a session ends and cannot
be forcibly unloaded safely. If Windows refuses to remove the mapped DLL, the control panel
writes the removal intent to Core's maintenance journal. On its next start, Core deletes the
runtime and manifest before validating or loading enhancement components, then clears the
configured version and maintenance journal. A failure at any step preserves the journal and
prevents the component from being used again until the user retries from the management UI.
Configuration and files for other components are unaffected.

## Build and distribution

The host's compilation method does not change this ownership model. The SDK, adapter layer, and
host toolchain must pass real linking and lifecycle validation together. Installers include the
dependencies needed for the host to start. Optional adapters may depend on system runtimes that
the user installs separately; a missing runtime must be reported clearly by the management UI
and must not prevent the host from starting. Whether sideloaded files may be distributed follows
the sources and licenses declared by that component. Operations that modify files, such as code
signing, must happen before their final digests are fixed.

These checks prevent an unknown file from being trusted merely by replacing a runtime or local
installation record. They do not claim to protect against replacement of the host binary itself.
