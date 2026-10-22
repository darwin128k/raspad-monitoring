# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.1.0] - 2026-10-22

### Added

- Authum client module (ABI 0.3) for classic HLDS (engine build 10211)
- Detour on `Steam_HandleIncomingPacket`: a bare A2S_INFO gets an `I` reply
  right away instead of Steam's `A` challenge (`info_answer = new|old|hybrid|steam`)
- `I` built from the engine's own `SVC_Info` (player / bot count, password, VAC)
- Old text queries `info` / `details` / `players` / `rules` answered again
  through the engine's `SVC_*` handlers, each with its own switch
  (`legacy_info` / `legacy_details` / `legacy_players` / `legacy_rules`;
  `rules` ships off — several KB of reply for a 10-byte query)
- Real address in `C` / `m` (`public_ip`, else the `ip` cvar) instead of the
  engine's `0.0.0.0` when HLDS listens on every interface
- Engine symbol lookup and detours from Authum (`<authum/symbol.h>`,
  `<authum/detour.h>`); `AUTHUM_DIR` can point a build at a local Authum
- Per-IP rate limit for direct answers (`ratelimit_per_ip`)
- Linux i386 CI + Release: Authum module zip (`addons/authum/modules/monitoring/`)
- Logo (`assets/logo.png`) in the README header

[Unreleased]: https://github.com/darwin128k/raspad-monitoring/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/darwin128k/raspad-monitoring/releases/tag/v0.1.0
