# English resource tools

Foundation Sunshine has one Web UI resource file:

`src_assets/common/assets/web/public/assets/locale/en.json`

After changing it, run:

```text
npm run i18n:format
npm run i18n:validate
```

The formatter keeps the JSON stable for review. The validator rejects malformed resources and verifies the English resource structure. Do not add locale negotiation, translated resource files, or locale synchronization workflows.
