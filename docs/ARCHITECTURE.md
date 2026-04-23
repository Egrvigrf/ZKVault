# Architecture

## Current Direction

ZKVault currently keeps one native codebase but separates it into three runtime
layers:

- `zkvault_core`
  - `src/app/`
  - `src/content/`
  - `src/crypto/`
  - `src/model/`
  - `src/storage/`
- `zkvault_service`
  - `src/service/`
- `zkvault_terminal_frontends`
  - `src/shell/`
  - `src/terminal/`
  - `src/tui/`

This split is intentional:

- `core` owns vault semantics, encryption, persistence, and validation.
- `content` owns private-post models and portable encrypted content bundles.
- `service` owns reusable local runtime/session behavior.
- terminal frontends own prompts, terminal rendering, and TUI flow only.

`src/main.cpp` remains a thin CLI entrypoint.

## Repository Boundary

This repository remains the native side of the project:

- local secure storage
- local runtime/session handling
- native CLI and TUI
- future local API daemon if needed

This repository should not become a mixed host for unrelated platform UI code.
In particular, Halo plugin code and browser-side decryption UI should not be
added under `src/`.

## Private Posts Product Split

To turn ZKVault into something usable for private blog content, the project
should evolve into two cooperating products:

1. Native author-side tooling
   - vault management
   - content encryption tooling
   - optional local preview/publish helpers
2. Platform adapters
   - Halo plugin
   - future VitePress/Astro/Hexo adapters

The native repository should stay focused on `1`.

Platform-specific code should live in adapter packages or sibling repositories,
not inside the native runtime tree.

## Halo Integration Boundary

For Halo specifically, the recommended split is:

- native side
  - article bundle format
  - encryption/decryption primitives
  - optional local author tooling
- Halo adapter side
  - editor UI
  - article metadata flags
  - locked-page rendering
  - browser-side unlock and auto-relock flow

If a future local API is introduced, it should sit on top of `src/service/`
rather than bypassing it.

## Near-Term Plan

- keep the native vault runtime stable
- add content-oriented models without breaking password-entry behavior
- introduce a minimal local API only when the adapter boundary is clear
- ship Halo as the first platform adapter, but keep the content format portable
