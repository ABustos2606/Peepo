# Ingen engine adapter

**Implemented:** 13 October 2026, on the Windows PC. **Scope:** P4.1 (protocol adapter), P4.2 (communication threading) and P4.3 (state synchronization), and, since 14 October 2026, the rest of P4.6 and patches on disk ([patch storage](17-patch-storage.md)). **Verified against:** the patched Ingen of [the engine evidence](14-ingen-engine-evidence.md), running under WSL2 on JACK's dummy backend, with the LV2 example amplifier. **Not covered:** the Pi, an ARM build, a real audio interface and physical controls.

`IngenEngine` implements the same `Engine` contract as the simulated engine, so the interface code is unchanged. The interface now runs against a real engine: `peepo.exe --engine tcp://127.0.0.1:16180` shows and edits the graph of an Ingen started under WSL.

![The Windows interface showing a live engine graph](images/ui-ingen-engine.png)

## What was verified

`bash tools/ingen-spike/07-adapter.sh`, as root inside the Debian of the engine evidence, builds the adapter, runs its tests and then runs it against a live engine. The recorded output is in [evidence/ingen-adapter-results.txt](evidence/ingen-adapter-results.txt): 62 checks passed, none failed (39 on 13 October; the others are the storage checks of [patch storage](17-patch-storage.md) and the assignment checks of [physical controls](18-physical-controls.md)). The same checks pass against the arm64 build of the engine ([Linux and Pi](19-linux-and-pi.md)).

| Check | Result |
| --- | --- |
| Commands | New patch, add, cable, uncable, set value, bypass, move, duplicate, rename, session save and restore all change the engine and read back correctly. Run over a Unix socket and again over TCP. |
| Audio | The patch the adapter built was measured through JACK: 0.25 in gave 0.5000 out through the amplifier at +6.02 dB on the left, and 0.2500 on the direct right cable. |
| Acknowledgment | Nothing appears in the graph before the engine answers. An edit round trip takes a median 5.8 ms (p95 6.2 ms) over the Unix socket. |
| Two clients | A second adapter reads the same graph, and each hears the other's edits and removals. This is the arrangement a separate control daemon would use. |
| Interface restart | An adapter destroyed without any farewell is replaced by a new one that finds the same modules under the same identities. |
| Engine restart | With the engine stopped under a connected adapter: the loss is reported, edits are refused, the last graph stays on screen, the adapter reconnects by itself, and it then shows the new engine's graph. |
| Refusals | Feedback cables, duplicate cables, out-of-range and non-finite values, and plugins the engine does not have are refused before any request is sent. |
| Windows | The same checks pass from the Windows build over TCP to the engine in WSL. All four Windows CTest suites pass. |

The stand-in engine in `tests/ingen_tests.cpp` covers what a live run cannot provoke on demand: replies split across reads, an unanswered request, unreadable input, a block made by another client under a foreign name, and values outside a control's range.

## How it maps

| Interface | Engine |
| --- | --- |
| IN and OUT modules | The graph's own audio ports `in_l`, `in_r`, `out_l`, `out_r`, created on first connection. They appear to JACK as `ingen:in_l` and so on. |
| Module with ID *n* | Block `ingen:/main/m`*n*. A block with any other name gets an unused number for as long as it exists. |
| Module name, position | `urn:peepo:ns#displayName`, `ingen:canvasX`, `ingen:canvasY` on the block |
| Software bypass | `ingen:enabled false` on the block, with a plugin's own on/off control kept at 1; for a plugin the overlay marks `"bypass": "port"`, that control at 0 and the block left enabled (see [catalogue export](16-catalogue-export.md)). A plugin marked `"bypass": "spill"` has `urn:peepo:ns#spillBypass true` on its block from the moment it is added, restored or loaded, and the engine's bypass then lets its tail ring on (engine patch 0013) |
| Parameter | `ingen:value` on a control input port. Name, unit and range come from the catalogue; the engine supplies only the value. |
| Cable | One `ingen:Arc` between two ports |
| Patch name | `urn:peepo:ns#patchName` on the graph, set with `patch:context ingen:internalContext` |
| Duplicate | `patch:Copy` inside the engine, then a `Get` of the copy |
| CPU reading | `ingen:meanRunLoad`: the share of the audio period spent processing, averaged since the engine started. Unknown until the engine first reports it. No tempo reading yet. |

Ports are taken from the engine, not from the catalogue. A block whose plugin has no catalogue entry is shown as a missing-module placeholder: it can be moved or deleted, and its values and cables are kept.

## How it behaves

- **Structure.** `turtle` reads the Turtle subset Ingen writes, as a tokenizer, so punctuation inside a string never ends a statement. `protocol` decodes patch messages and writes requests. `session` is the state machine: bytes in, bytes out, events; it holds no socket and is what the tests drive. `socket_transport` does the blocking work on its own threads. `IngenEngine` joins them and owns reconnection.
- **Threads.** One thread reads the socket and one writes it. The interface thread never waits on the network; it collects received bytes and parses them inside `advance()`. If the interface falls 8 MiB behind, the connection is replaced instead of growing without bound.
- **Synchronization.** After connecting, the adapter asks for the plugin list and the whole graph. The mirrored graph is replaced at the moment the answer starts, so the previous graph stays visible until then. An edit completes when every one of its requests has been answered and the bundle carrying the answers has ended.
- **Correlation.** Replies are matched on `patch:Response` only. The engine numbers its own notifications with the same predicate; the tests make those numbers collide on purpose.
- **Values the engine does not echo.** A client that sets a property gets a reply but no notification, so the adapter applies its own accepted `Set` to the mirror.
- **Failure.** A request unanswered after 3 s fails, and the graph is then read again in full. A multi-step edit that fails part-way also triggers a full read. An unreadable message replaces the connection. A lost engine is retried every second until `disconnect()`; pending edits fail and are never replayed.
- **What is refused locally.** Cycles, duplicate cables, values outside the catalogue range, discrete values that are not whole, text that could not be a safe address, and any plugin absent from the engine's own list. A patch to load is checked completely before the first request, so a refused patch leaves the current one untouched.

## Session snapshot

**Save snapshot / Load** works as in the simulator, but the snapshot is held by the interface: routing, values, bypass, names and layout. Restoring it deletes the engine's blocks and builds the saved ones again. It does not capture plugin state and is lost when the interface closes. Patches saved to disk, with plugin state, go through the engine's own bundle format; see [patch storage](17-patch-storage.md).

## Run it

Inside Debian (WSL), start an engine and leave it running:

```bash
. tools/ingen-spike/env.sh && start_engine
```

Then on Windows:

```powershell
& '.\build\Debug\peepo.exe' --engine tcp://127.0.0.1:16180
```

**+ Add** lists the modules exported from that engine's installed plugins; see [catalogue export](16-catalogue-export.md). No sound is produced; the dummy backend has no audio device. Without `--engine` the application runs the simulation as before.

A Unix socket address (`unix:///tmp/peepo-ingen.sock`) is the route on Linux and the one intended for the Pi. TCP exists so the Windows build can reach an engine in WSL.

## Limits and open points

- **The engine's TCP port is open to the network** when it is started with one: Ingen listens on every interface with no authentication. Since 14 October 2026 engine patch 0004 lets `--engine-port 0` turn it off, and the Pi's service does ([Linux and Pi](19-linux-and-pi.md)). The development engine in WSL keeps port 16180 for the Windows build.
- **Catalogue.** Exported from the installed plugins since 13 October 2026 ([record](16-catalogue-export.md)); 58 modules are offered on the WSL engine. A control the catalogue leaves out is not shown.
- **Unsaved-changes marker.** Since 14 October 2026 it is a property of the engine's graph (`peepo:modified`), so a restarted interface and a second client see it. Another client's edits set it only if that client sets it too.
- **Control assignments** stored on ports are not read yet; that belongs with Phase 7.
- **Merge level.** The engine still sums several cables into one input at unity. The adapter passes them through; the merge rule of P3.7 is undecided.
- **Windows timing.** Round trips measured from Windows (16 ms to the WSL address, 56 ms through `127.0.0.1`) are dominated by the Windows timer and the WSL relay, not by the adapter; the same code measures 5.9 ms over TCP inside Linux.
- **Still to do on the Pi:** an ARM build of the same code, the native display and touch backend (P4.5), and the live touch workflows (P4.7).
