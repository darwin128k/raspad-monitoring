# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed

- Built on Authum 3.0.0 (module ABI 1.0.0); `lib/authum` tracks branch
  `3.0.0`. Needs `authum_mm` 3.0.0 — older hosts refuse the module.
- Logging is Authum's: lines go to `addons/authum/logs/monitoring/YYYY-MM-DD.log`,
  warnings and errors also to `logs/authum/`, levels in
  `addons/authum/authum.cfg` (`log_level`, `log_console`,
  `monitoring.log_level`). `logging_mode` and `logging_split` are no longer read
  from `monitoring.cfg`.
- The host is kept with `authum_host_copy`.
- Follows Authum's layout: config in `addons/authum/configs/monitoring.cfg`, `monitorings.ini` in
  `configs/monitoring/`;
  an old copy in `modules/monitoring/` is still read, with a warning.

### Added

- `info_answer_scope = all|list`: with `list`, only the monitoring pollers in
  `monitorings.ini` (IP or CIDR per line) get direct answers; everyone else
  goes through Steam's challenge, so the server cannot be used as a reflector
- Console commands `authum monitoring reload` (re-read the list, no restart)
  and `authum monitoring list`
- `monitorings.ini` sample in the release zip

### Changed

- `appid` and `version` default to empty and are read where Steam reads them:
  `steam_appid.txt` in the HLDS root and `PatchVersion` in `<gamedir>/steam.inf`
  (+ `/Stdio`); set them in `monitoring.cfg` only to override
- `public_ip` empty and no `ip` cvar: the machine's one external IPv4 from its
  interfaces (`<authum/net.h>`, Authum 66836ca); the start log lists the
  internal and external addresses it saw
- The module version comes from the `VERSION` file (CMake passes it to
  `info.c`), so the version Authum logs can no longer drift from the release

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
