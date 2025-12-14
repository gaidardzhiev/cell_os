# Cell OS v1.0.0 (spine — eukaryote track)

Release date: 2025-12-14 (deterministic build; LC_ALL=C, TZ=UTC, SOURCE_DATE_EPOCH=1700000000)

## Highlights
- E0: Bare-metal boot bridge (x86 MBR→Stage-2→long mode; ARM64 virt + PL011).
- E1: NCI/ABI pinned (handoff_t=40B, msg_t=16B).
- E2: Deterministic parcel encode/decode.
- E3: Channel Graph + QoS model (ABI pinned).
- E4: Phenotype SDK/loader (.pbin).
- E5: Core organelles (mito/golgi/lyso/peroxi).
- E6: IRQ→Event bridge (debounce, batch, token bucket).
- E7: Dual-strand update/rollback; pluggable hash (stub/SHA-256).
- E8: Observability v2 + Replay (stable log format, index/hash).
- E9: Security hardening (MAC16 + trust rotation).
- E10: Reproducible release + self-test proofs (#E3…#E10 + FULL PROOF SUCCESS).

## Artifacts (out/)
- `cellos-x86_bios.img` — raw BIOS disk image (Stage-1/2 + kernel).
- `cellos-arm64.elf` — ARM64 virt kernel (PL011).
- `phenos/*.pbin` — compiled phenotypes.
- `MANIFEST.txt` — SHA256 + size per artifact.
- `BUILDINFO.json` — toolchain/flags and SOURCE_DATE_EPOCH.
- Proof logs produced in CI: `x86.log`, `arm.log` (see `tools/verify_proofs.sh`).

## How to reproduce
```bash
export LC_ALL=C TZ=UTC SOURCE_DATE_EPOCH=1700000000
make -C cell_os clean all
make -C cell_os release
cell_os/scripts/run_x86_qemu.sh cell_os/out/cellos-x86_bios.img | tee /tmp/x86.log
cell_os/scripts/run_arm64_qemu.sh cell_os/out/cellos-arm64.elf | tee /tmp/arm.log
cell_os/tools/verify_proofs.sh /tmp/x86.log
cell_os/tools/verify_proofs.sh /tmp/arm.log
```
