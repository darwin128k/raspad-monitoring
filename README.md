<p align="center">
  <img src="assets/logo.png" alt="monitoring" width="220">
</p>

<h1 align="center">monitoring</h1>

<p align="center">
  <b>Authum module</b> for classic HLDS<br>
  Monitorings and server browsers see your server again.
</p>

<p align="center">
  <a href="LICENSE">Apache-2.0</a> ·
  <a href="https://github.com/darwin128k/raspad-monitoring">GitHub</a> ·
  <a href="https://github.com/darwin128k/raspad-authum">Authum</a>
</p>

---

## Why install it

On current HLDS (build 10211) server queries are answered by Steam, not by the
engine. Steam replies to a plain `TSource Engine Query` with an `A` challenge
and only sends the real info (`I`) when the query is repeated with that
challenge. The old text queries (`info`, `details`, `players`, `rules`) get no
answer at all.

Modern monitorings handle that. Older ones send one query, get 9 bytes of
challenge, and list your server as **offline** — with whatever map and name
they saw last time.

monitoring answers those queries the way pre-Steam-challenge servers did:

- bare A2S_INFO → `I` right away (or GoldSrc `m`, or both);
- `info` / `details` / `players` / `rules` → `C` / `m` / `D` / `E`;
- queries that already carry a challenge, and everything else, go to Steam unchanged.

Player and bot count, password and VAC flag come from the engine's own
`SVC_Info`, so the answer matches what the server really is.

## How it works

Every connectionless packet goes through `Steam_HandleIncomingPacket` (after the
engine's `max_queries_*` check) and then `SV_ConnectionlessPacket`. monitoring
puts a detour on the first one:

| Query | Answer | Switch |
|-------|--------|--------|
| `TSource Engine Query`, no challenge (or `-1`) | `SVC_Info(1)` captured as `m`, rewritten to `I` | `info_answer` |
| `info` | `SVC_Info(0)` → `C` | `legacy_info` |
| `details` | `SVC_Info(1)` → `m` | `legacy_details` |
| `players` | `SVC_PlayerInfo` → `D` | `legacy_players` |
| `rules` | `SVC_RuleInfo` → `E` | `legacy_rules` (off by default) |
| anything else | Steam, unchanged | — |

`C` and `m` carry the server address as their first field; HLDS bound to every
interface writes `0.0.0.0` there, so monitoring puts `public_ip` (or the `ip`
cvar) in instead.

None of these answers needs a challenge, so each can be aimed at a spoofed
address. Direct answers are rate-limited per source IP (`ratelimit_per_ip`);
over the budget the query falls back to Steam's challenge. `rules` is off by
default because its reply is several KB for a 10-byte query.

## Install

1. Download **`monitoring-linux32-authum.zip`** from [Releases](https://github.com/darwin128k/raspad-monitoring/releases) (or build below).
2. Unpack into the game dir so you get:
   - `addons/authum/modules/monitoring/monitoring.so`
   - `addons/authum/modules/monitoring/monitoring.cfg`
3. Add `monitoring` to `addons/authum/modules.list`.
4. Restart HLDS. `authum.log` should say
   `hook installed on Steam_HandleIncomingPacket`.

Settings: [`monitoring.cfg`](monitoring.cfg).

## Check it

From any machine:

```bash
python query.py <ip>:<port>
```

(`query.py` from `hlds-query` sends every query style monitorings use; any
A2S tool will do). With the module on, the first `TSource Engine Query`
gets `I`, not `A`, and `info` / `details` / `players` / `rules` get replies.

## Build

Linux i386 (`-m32`), same as other Authum modules.

```bash
git submodule update --init --recursive
cmake -S . -B build-linux32 -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-linux32 --target monitoring
# → build-linux32/modules/monitoring/monitoring.so
```

```bash
sudo dpkg --add-architecture i386
sudo apt-get update
sudo apt-get install -y gcc-multilib cmake ninja-build \
  libc6-dev:i386
```

## Related

- [Authum](https://github.com/darwin128k/raspad-authum) — module host
- [qproto](https://github.com/darwin128k/raspad-qproto) — non-Steam authorization
- [gatekeeper](https://github.com/darwin128k/raspad-gatekeeper) — geo / proxy filter
