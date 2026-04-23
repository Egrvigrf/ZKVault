# Halo Integration

This directory is a boundary marker for the future Halo adapter.

It exists to keep one rule explicit: Halo-specific product code should be
treated as an integration layer, not folded back into the native vault runtime
under `src/`.

## What Belongs Here

- Halo plugin packaging notes
- article field mapping
- locked-page rendering contracts
- browser-side unlock flow notes
- release and marketplace assets

## What Does Not Belong Here

- core encryption primitives
- native vault session state
- local secure storage logic
- terminal interaction code

Those stay in the native codebase:

- `src/app/`
- `src/crypto/`
- `src/service/`
- `src/storage/`

## Recommended Product Shape

- `ZKVault` native runtime remains the author-side secure core.
- `Halo` becomes the first distribution adapter for private posts.
- If Halo marketplace submission is blocked, the same adapter can still be
  distributed through GitHub Releases, forum posts, or other supported plugin
  channels.
