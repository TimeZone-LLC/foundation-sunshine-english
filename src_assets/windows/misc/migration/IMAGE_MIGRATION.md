# Image path migration

The Windows migration scripts move existing application images into the current `config\covers` directory and update their `apps.json` references.

## Files that are migrated

For each application, `migrate-images.ps1` checks the `image-path` value.

It leaves these values unchanged:

- `desktop`, which selects the built-in desktop image.
- A plain file name such as `app_Steam_1234567890.png`, which is already a box-art reference.

It attempts to migrate absolute paths, relative paths, and legacy `covers` paths. Relative files are searched in this order:

1. The path as supplied.
2. Relative to the configuration directory.
3. Relative to the previous Sunshine installation directory.

## Result

When the source image exists, the script copies it into `config\covers` under a generated name and replaces `image-path` with that file name.

```text
Source: C:\Users\example\Pictures\game.png
Application: Steam
Destination: config\covers\app_Steam_1234567890.png
Stored image-path: app_Steam_1234567890.png
```

`migrate-config.bat` performs the surrounding configuration migration and then invokes the image migration script before the Sunshine service starts.

## Failure behavior

- A missing source file leaves the original `image-path` unchanged.
- A copy failure leaves the original reference unchanged and records an error.
- Invalid JSON stops the image migration instead of rewriting `apps.json`.
- The script writes changes through a temporary file so an interrupted write does not leave a partial configuration.

After installation, migrated images are served through `/boxart/<filename>` from the configured covers directory.
