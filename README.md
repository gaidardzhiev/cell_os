# cell_os

Cell OS is a boot-to-proof microkernel that exercises its own BIOS-stage loaders, handoff ABI, and kernel genes on x86_64 (PC BIOS) and arm64 (QEMU virt). It emits a deterministic chain of `#E0…#E10` log markers, proving the substrate classification, channel graph, organelles, IRQ bridge, update path, observability feed, and release manifest. The same source tree builds the boot media, phenotypes, host tools, and reproducible release tarballs.

## Design Goals
- Deterministic execution from first sector through the `#E10` release proof on both supported architectures.
- Minimal, self-hosted stack using only pinned ABIs (`handoff_t`, parcels, channel graph) and static-size data structures.
- Precise observability with channel-0 parcels, replay/index tooling, and strict proof verification.
- Reproducible artifacts (`SOURCE_DATE_EPOCH`, manifest, phenotype hashes) without hidden build steps.
- Simple extension surface (“genes”, “organelles”, phenotypes) that carries resource accounting and routing policies.

## System Overview
The boot chain starts with a 512-byte BIOS stage that reads the second stage via INT13h and jumps to 0x8000 (`src/boot/stage1.asm:10`). Stage 2 switches to 64-bit long mode, loads the packed kernel image, optionally verifies the CGF blob, and jumps into the kernel entry stub (`src/boot/stage2.asm:12`). The kernel entry transitions to `kernel64_main()` (`src/kernel/kernel64_main.c:52`), which logs the substrate classification, prints the `handoff_t`, optionally enables the log ring and organelle KV store, arms ARM IRQs, and finally halts or waits for interrupts.

Hardware metadata flows through the fixed-size `handoff_t` (`include/core/handoff.h:13`) that embeds a 128-byte `cgf_substrate_info_t` (`include/spec/cgf_substrate_e0.h:84`). The E0 probe code emits the canonical log lines described in `docs/spec/substrate_e0.md:1` and archived in `CAPABILITIES.md:1`. Genes are described by phenotype binaries compiled from YAML (`tools/phenotype_compile.py:1`, `schemas/phenotype.schema.yaml:1`), which provide channel QoS tuples and adjacency matrices. The runtime rebuilds the channel graph (`include/core/channel.h:14`, `src/core/channel_graph.c:16`), programs the QoS token buckets (`src/core/qos.c:29`), and seeds the Golgi router and organelle set (`src/core/organelles.c:75`, `src/genes/mito_energy.c:24`).

Scheduler ticks (`src/core/scheduler.c:34`) run gene tasks via VM hooks, charge CPU/IO/memory budgets into mitochondria, and gate syscalls through `qos_try_enqueue()` before allowing routes. IRQ sources enter the event bridge, which debounces and token-buckets hardware lines into parcel-ready frames (`src/core/irq_bridge.c:12`, `src/core/events.c:19`). Diagnostic output is framed through the parcel codec (`libparcel/parcel.c:21`) and optionally buffered in the log ring before hitting channel 0. Observability records are packed into the same channel and indexed with the simple replay hash for proofing (`src/core/obs.c:19`, `src/core/obs_trace.c:13`, `tools/obs_replay.c:16`). Updates and migrations write dual genome strands, verify hashes, and pick the preferred strand (`src/core/update.c:31`), while the optional organelle KV store tracks short-lived state (`src/core/kv.c:15`). The proof ledger (`#E1…#E10`) is validated post-boot by the host tooling and captured in `proofs/x86.log` and `proofs/arm.log`, ensuring every mechanism has quantitative evidence before packaging.

## Code Layout
- `src/boot/` — BIOS stage (`stage1.asm:10`), second stage (`stage2.asm:12`), and ancillary stubs for cpuid/midr probing and optional CGF verification. `scripts/pack_disk.py` stitches stages + kernel + CGF into `build/disk.img`.
- `src/kernel/kernel64_main.c:52` — architecture-neutral kernel entry, console routines, optional IRQ/timer enablement, proof dispatch, and idle loops. Linker scripts live beside it for x86_64 and arm64.
- `include/core/handoff.h:13`, `include/spec/cgf_substrate_e0.h:84`, and `docs/spec/substrate_e0.md:1` — pinned ABI for the substrate report, enums, and documentation of the E0 log contract.
- `libparcel/parcel.c:21` — 16-byte parcel header codec, MAC stub hooks, and helpers for channel-0 log framing; the same code runs inside the kernel, host tests, and OBS tooling for bit-identical framing.
- `src/core/channel_graph.c:16` + `include/core/channel.h:14` — packed QoS structs and bitset adjacency; `cg_build_full()` synthesizes deterministic graphs during tests.
- `src/core/qos.c:29` and `src/core/scheduler.c:34` — token-bucket QoS engine, GAS reservoir, and round-robin gene scheduler that calls out to the VM hooks.
- `src/core/organelles.c:75`, `src/genes/mito_energy.c:24`, `src/genes/golgi_route.c`, `src/genes/lyso_cleanup.c`, `src/genes/peroxi_sanitize.c`, `src/genes/testgene.c` — resource charge accounting, routing verdicts, liveness map, sanitizer, and example expression path.
- `src/core/kv.c:15` — fixed-capacity namespace/key/value store gated by `CFG_ENABLE_ORG_PKV`.
- `src/core/irq_bridge.c:12` + `src/core/events.c:19` — interrupt-to-event bridge, debounce policy, and packed event frames.
- `src/core/log_ring.c`, `src/core/ch0_log.c`, `src/core/obs.c:19`, `src/core/obs_trace.c:13` — logging path, optional non-blocking ring, observability framing, and replay hash accounting.
- `src/core/update.c:31`, `include/core/update.h`, and `tools/mock_storage.h` — dual-strand updater, metadata checks, and mock storage for host tests.
- `scripts/build_release.sh:8` — reproducible release pipeline (cleans, rebuilds, runs tests, copies artifacts, emits MANIFEST + BUILDINFO). `scripts/run_x86_qemu.sh:17` and `scripts/run_arm64_qemu.sh:13` run the built images under QEMU.
- `tools/phenotype_compile.py:1`, `tools/gen_cgf.py:1`, `tools/obs_replay.c:16`, `tools/verify_proofs.sh:15` — phenotype compiler, CGF generator, OBS replay/indexer, and proof checker.
- `tests/` — host tests for parcels, CG/QoS, scheduler, organelles, IRQ, update/migrate, observability, trust/MAC, KV, phenotype loading (`test-all` covers them). `tests/e0/test_x86_qemu.sh:12` and `tests/e0/test_arm64_qemu.sh:12` boot QEMU targets and grep the substrate logs.
- `phenotypes/` (YAML), `schemas/` (JSON schema), `proofs/` (recorded logs), `out/` (release artifacts), and `build/` (scratch tree) round out the repo. The proof mapping narrative lives in `CAPABILITIES.md:1`.

## Build and Run
Requires a POSIX host with `gcc`, `aarch64-linux-gnu-gcc`, `ld`, `nasm`, `make`, `python3` (with PyYAML), and QEMU (`qemu-system-x86_64`, `qemu-system-aarch64`). The release pipeline assumes `LC_ALL=C`, `TZ=UTC`, and `SOURCE_DATE_EPOCH=1700000000`.

```bash
cd cell_os
make all phenotypes            # builds host libs, tests, phenotype .pbin blobs
make kernel kernel-arm         # emits build/kernel.bin and build/arm64_kernel.elf
make pack-disk                 # packs stage1/2 + kernel into build/disk.img
scripts/run_x86_qemu.sh build/disk.img   | tee /tmp/x86.log
scripts/run_arm64_qemu.sh build/arm64_kernel.elf | tee /tmp/arm.log
```

For a reproducible drop with manifest and proofs:

```bash
cd cell_os
make release                   # runs scripts/build_release.sh:8 via e10.mk
ls ../out                      # cellos-x86_bios.img, cellos-arm64.elf, phenos/, MANIFEST.txt, BUILDINFO.json
```

Optional flags such as `CFG_ENABLE_CGF_VERIFY=1`, `CFG_ENABLE_NONBLOCK_LOG=1`, or `CFG_ENABLE_ORG_PKV=1` may be passed to `make` to exercise additional kernel paths.

## Testing
`make test-all` runs every host unit (parcel, CG/QoS, scheduler, log ring, phenotype loader, organelles, IRQ bridge, update + migrate, observability, crypto/trust fuzz). Successful runs print each `#E*` tag reported by the individual make fragments.

To validate end-to-end proofs:

```bash
make run-x86-proof             # boots ../out/cellos-x86_bios.img, writes TMPDIR/x86.log, runs tools/verify_proofs.sh:15
make run-arm-proof             # same for arm64
tools/verify_proofs.sh build/pf12_x86.log build/pf12_arm.log
```

`tests/e0/test_x86_qemu.sh:12` and `tests/e0/test_arm64_qemu.sh:12` re-run the proof harness with greps that pin the substrate tuple. Expect each log to contain `#E1 handoff …` through `#E10 RELEASE proof:` followed by `FULL PROOF SUCCESS` and `# proofs ok (N log(s))`. Reference logs in `proofs/` and the ledger in `CAPABILITIES.md:1` provide known-good baselines.

## Implementation Notes
- The BIOS chain is minimal by design: stage 1 reads a configurable number of sectors and jumps to 0x8000 (`src/boot/stage1.asm:10`), while stage 2 installs page tables, loads the kernel, and optionally validates the CGF blob before entering long mode (`src/boot/stage2.asm:12`).
- `handoff_t` is 168 bytes and embeds the 128-byte `cgf_substrate_info_t` defined in `include/spec/cgf_substrate_e0.h:84`; `_Static_assert` guards keep both ABIs pinned across releases (`include/core/handoff.h:13`).
- Parcels enforce a 16-byte header with little-endian fields, zero-copy payload pointers, and deterministic MAC slots (`libparcel/parcel.c:21`); the same code runs inside the kernel, host tests, and OBS tooling for bit-identical framing.
- Channel graphs store QoS tuples and adjacency bitsets (`include/core/channel.h:14`, `src/core/channel_graph.c:16`), hashed later for the `#E3/E4` proof. A static assertion freezes the QoS struct size at 36 bytes.
- The QoS subsystem keeps per-channel buckets plus a GAS counter that is decremented on every enqueue attempt (`src/core/qos.c:29`). `qos_tick()` refills buckets and drains queue depth, ensuring deterministic replenishment rates that map directly into the proof hash.
- Scheduler ticks call `vm_run_quantum()` and record resource usage (`src/core/scheduler.c:34`). The external VM hooks mean that no interpreter is baked into the kernel; kill switches (`organelles_mark_dead()`) enforce routing or budget violations.
- Organelles bundle four independent subsystems—mitochondria energy tables, Golgi routing, lysosome dead-map, and peroxisome sanitizer—glued together in `src/core/organelles.c:75`; each gene handler (`src/genes/*.c`) sticks to fixed-size tables for deterministic hashing.
- The IRQ bridge keeps per-source rate-limit buckets and a ring buffer (`src/core/irq_bridge.c:12`), emits packed events via `event_pack()` (`src/core/events.c:19`), and logs counts for the `#E6` proof.
- Observability records are channel-0 parcels with a bounded payload (`src/core/obs.c:19`). Indexing uses a deliberate non-cryptographic hash (`src/core/obs_trace.c:13`) solely for change detection; replay tooling (`tools/obs_replay.c:16`) prints back the marked lines.
- Dual-strand updates write into whichever genome strand is not preferred, hash each half, and only flip the preference bit when hashes match the manifest (`src/core/update.c:31`). `tools/mock_storage.h` feeds ephemeral buffers to the tests.
- `scripts/build_release.sh:8` enforces locale/timezone/SDE, re-runs all tests, copies artifacts into `../out/`, and emits MANIFEST + BUILDINFO so the proof hash chain (`#E10`) can be rederived from published bits.

## Limitations and Future Work
- Cryptography and trust roots are placeholders (`src/core/crypto.c:11`, `src/core/trust.c:11`); real MAC16 implementations, key derivation, and key storage are still TODO.
- Observability hashing uses a simple mixer rather than a collision-resistant digest (`src/core/obs_trace.c:13`); swap in a pinned cryptographic hash when hardware entropy and code size allow.
- The scheduler depends on external VM hooks (`src/core/scheduler.c:21`) and does not yet ship a concrete VM, so the shipped kernels only exercise the control plane, not real guest code.
- The organelle KV store is an in-memory ring with 64 entries and no persistence (`src/core/kv.c:15`); backing it with flash or host storage would make `CFG_ENABLE_ORG_PKV` more meaningful.
- Only the PC BIOS and QEMU virt-PL011 phenotypes ship in `phenotypes/`; more substrates (UEFI, other ARM SoCs, MCUs) should extend the YAML/spec coverage and expand the CGF table.
- Security proofs currently rely on host-side verification scripts and recorded logs; integrating proof checks into CI and hardware farms plus expanding `proofs/` beyond QEMU would increase confidence.

## License
The project is released under the MIT License (`LICENSE:1`). VERSION is tracked at `1.0.0-eukaryote-spine` (`VERSION:1`).

## Further reading: [The Cell OS Book](https://www.amazon.com/dp/B0GFD4PVH2/ref=mp_s_a_1_1?crid=OK7WJ6J0L33J&dib=eyJ2IjoiMSJ9.GIdeOEKSNrU21Yv-NV-udQ.382EYb6APk73zzOBvxGsoBqTxkIt4GnpAIHYOX-8e-E&dib_tag=se&keywords=ivan+gaydardzhiev&qid=1767777093&sprefix=ivan+gaydardzh%2Caps%2C723&sr=8-1)
