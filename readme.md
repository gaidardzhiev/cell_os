# cell_os

Cell OS is a boot-to-proof microkernel and experimental computational substrate. It exercises its own BIOS-stage loaders, handoff ABI, channel graph, organelles, resource accounting, observability path, update mechanisms, and proof chain on x86_64 PC BIOS and arm64 QEMU virt targets. The same source tree builds boot media, phenotypes, host tools, validation artifacts, and reproducible release packages.

The project is organized around a biological vocabulary, but the implementation is intentionally concrete. A substrate describes the machine on which the cell executes; genes describe executable behaviors; organelles provide bounded services; channels carry parcels under explicit routing and QoS rules; mitochondria account for resource consumption; the Golgi layer mediates routing; and the proof ledger records whether the expected mechanisms actually executed.

A recent experimental extension adds **Cell Cortex**, a small native neural inference subsystem intended to provide a cognitive mediation layer between natural-language intent and Cell OS capabilities. Cortex does not replace the deterministic substrate. It runs above it and is constrained by it. The present model, **CellLM-1M**, is a deliberately small English-language causal decoder trained to map a bounded set of natural-language computer-control requests into symbolic Cell capability calls.

The current work should therefore be understood as two interacting layers:

- a deterministic Cell OS substrate responsible for boot, memory, devices, routing, accounting, and proof;
- an experimental cognitive layer responsible for interpreting human language and proposing symbolic actions within the capabilities exposed by the substrate.

The long-term objective is not to place an unrestricted language model in the hardware control path. It is to investigate whether a compact learned system can serve as a semantic interface to a small, explicit, and verifiable machine substrate.

## Design Goals

- Deterministic execution from the first boot sector through the `#E10` release proof on the supported reference targets.
- Minimal self-hosted mechanisms built around pinned ABIs such as `handoff_t`, parcels, the channel graph, and static-size kernel structures.
- Precise observability through channel-0 parcels, replay/index tooling, and explicit proof verification.
- Reproducible artifacts using fixed build metadata and manifests rather than hidden build steps.
- A simple extension surface based on genes, organelles, phenotypes, channels, and explicit resource accounting.
- Clear separation between deterministic machine control and learned cognitive interpretation.
- A compact native inference path that does not depend on Linux, libc, llama.cpp, GGML, or a conventional userspace runtime on the target system.
- A capability-mediated interface in which learned components request named operations rather than issuing arbitrary MMIO, DMA, or privileged hardware operations.

## System Overview

The x86_64 boot chain begins with a 512-byte BIOS stage that loads the second stage and transfers control to it. Stage 2 enables the required processor state, enters long mode, prepares the memory mappings, loads the packed kernel image, and transfers execution to the kernel entry stub.

The kernel receives hardware and boot metadata through the fixed-size `handoff_t` ABI. Substrate probing records the execution environment and emits the E0 classification data. The runtime then constructs the channel graph, initializes QoS state, seeds the Golgi router and organelle set, prepares observability, and executes the proof path enabled for that build.

Scheduler ticks run gene tasks through VM hooks, account CPU, I/O, and memory consumption, and apply routing and QoS policy before actions are admitted. IRQ sources enter the event bridge, which turns bounded hardware events into parcel-ready frames. Observability records use the same channel machinery and can be replayed by host tooling. Update and migration logic uses dual genome strands and validates state before preference is changed.

The proof ledger `#E1` through `#E10` is intended to make the system inspectable. A mechanism is not treated as established merely because source code exists for it; the reference builds emit deterministic evidence that the expected path executed.

## Cortex: Cognitive Mediation Layer

Cell Cortex is an experimental native inference subsystem added to the x86_64 Cell OS path. Its role is semantic mediation: it receives a natural-language request, evaluates it with a compact causal language model, and emits either ordinary text or a symbolic request for a Cell capability.

For example, the trained CellLM-1M model currently learns mappings of the following form:

```text
cell> show memory
<CALL memory.status>

cell> what cpu do i have
<CALL cpu.info>

cell> list disks
<CALL storage.list>

cell> ls
<CALL storage.list_dir>

cell> show gpu
<CALL gpu.info>

cell> list network interfaces
<CALL network.list>

cell> list usb devices
<CALL usb.list>
```

The symbolic call is an interface boundary, not a hardware instruction. Cortex is not permitted to invent a register address and write it directly. The intended architecture is that a verified Cell capability dispatcher accepts only known calls and maps them onto deterministic organelles or device backends. Unsupported capabilities should remain unavailable rather than being synthesized speculatively at runtime.

This distinction is central to the design. The learned component interprets intent. The deterministic substrate controls the machine.

### Current CellLM-1M Model

The first trained model is intentionally small. It exists to establish the complete training, export, loading, and inference path before larger models are considered.

Current configuration:

```text
name            CellLM-1M
language        English
parameters      1,049,984
vocabulary      256 byte values
context         256 bytes/tokens
d_model         128
layers          5
attention heads 4
feed-forward    512
normalization   RMSNorm
activation      ReLU
quantization    Q8 in CWM1
model size      1,054,400 bytes
```

The model is trained outside Cell OS with PyTorch and exported into the native CWM1 representation. Training and target inference are deliberately separated: PyTorch is a development tool, not a runtime dependency of Cell OS.

The first training corpus is synthetic and narrow. It contains English requests for system status, CPU information, memory status, storage enumeration, directory listing, network enumeration, GPU information, USB enumeration, display information, and power status, together with a small number of conversational and safety-oriented examples. The corpus is not intended to create a general-purpose conversational model.

The present model should therefore be interpreted as a learned semantic controller for a bounded vocabulary of Cell operations, not as a claim of general intelligence.

## CWM1 Model Format

Cortex loads models through a compact Cell Weight Model format, currently `CWM1`. The format provides a fixed header describing the model architecture and a deterministic ordering of quantized and floating-point tensors.

The current exporter writes:

- a 64-byte CWM1 header;
- Q8 token embeddings;
- Q8 position embeddings;
- FP32 RMSNorm weights;
- Q8 attention projection matrices;
- FP32 second RMSNorm weights;
- Q8 feed-forward matrices;
- FP32 final RMSNorm weights;
- payload size and CRC32 metadata.

The CWM1 parser validates the header, model dimensions, payload size, quantization identifier, and CRC before the model is admitted by Cortex.

CWM1 is currently an internal experimental ABI. Compatibility should not be assumed across arbitrary future model families until the format is explicitly versioned for that purpose.

## Native Inference Runtime

The Cortex inference engine is written as a freestanding C subsystem and links directly into the Cell OS x86_64 kernel image. It does not require a standard C library or a conventional process environment.

The current runtime implements the operations required by CellLM-1M, including:

- byte-level token input;
- token and positional embedding lookup;
- RMS normalization;
- causal multi-head self-attention;
- feed-forward layers;
- tied output projection;
- greedy next-token selection;
- Q8 matrix storage and dequantization during computation;
- inference workspace allocation from the Cell memory arena.

The current optimization level is deliberately modest. The proof path includes scalar/SSE2-compatible CPU execution. Wider SIMD, more efficient quantized matrix kernels, KV-cache refinements, and architecture-specific acceleration remain optimization work rather than prerequisites for establishing the model/runtime boundary.

## Model Loading and Memory

The original proof kernel used a deliberately small and simple memory setup. Cortex required a larger inference workspace and therefore introduced an extended x86 boot descriptor together with E820-backed memory discovery and a Cell memory arena.

For the current x86 reference configuration, the model is stored as an independent payload in the Cell OS disk image rather than being linked into `kernel.bin`. The image packer records the model location and size. At boot, Cortex reads the CWM1 payload through the current ATA PIO path, validates it, allocates the required workspace, and initializes inference.

Keeping model weights separate from the kernel is intentional. The kernel remains a relatively small deterministic substrate, while different model checkpoints can be packed into the same execution environment without rebuilding the inference implementation.

## Code Layout

The original Cell OS subsystems remain in place. The Cortex work extends the tree rather than replacing it.

- `src/boot/`  BIOS first and second stages, long-mode transition, substrate preparation, and Cortex boot metadata.
- `src/kernel/`  kernel entry, runtime support, Cortex bootstrap, console path, and linker scripts.
- `include/core/handoff.h` and related substrate headers  pinned boot and substrate metadata.
- `src/core/channel_graph.c` and `include/core/channel.h`  channel QoS and adjacency graph.
- `src/core/qos.c` and `src/core/scheduler.c`  token-bucket QoS, GAS accounting, and gene scheduling hooks.
- `src/core/organelles.c` and `src/genes/`  organelle services and gene behaviors.
- `src/core/irq_bridge.c` and `src/core/events.c`  interrupt-to-event conversion.
- `src/core/log_ring.c`, `src/core/ch0_log.c`, `src/core/obs.c`, and `src/core/obs_trace.c`  observability and replay-oriented logging.
- `src/core/update.c`  dual-strand update and migration logic.
- `libparcel/parcel.c`  parcel framing shared by target and host tooling.
- `src/core/mem_arena.c`  bounded memory arena used by Cortex and other native allocations.
- `src/drivers/x86/ata_pio.c`  current x86 ATA PIO path used to load the model payload.
- `src/cortex/cwm.c`  CWM1 parser and model validation.
- `src/cortex/cortex.c`  native causal transformer inference implementation.
- `src/kernel/cortex_boot.c`  Cortex model loading and interactive execution path.
- `tools/celllm_train.py`  CellLM-1M PyTorch training and CWM1 export tool.
- `tests/test_cortex_host.c`  native host-side CWM1/Cortex inference test.
- `models/`  exported CWM checkpoints and model metadata.
- `data/`  current CellLM training corpus.
- `checkpoints/`  development training checkpoints where retained intentionally.
- `tools/phenotype_compile.py`, `tools/gen_cgf.py`, `tools/obs_replay.c`, and `tools/verify_proofs.sh`  original phenotype, CGF, observability, and proof tooling.
- `phenotypes/`, `schemas/`, `proofs/`, `out/`, and `build/`  original configuration, validation, release, and scratch paths.

## Build and Run

The original Cell OS build requires a POSIX host with the corresponding cross-toolchains, NASM, Python, Make, and QEMU for the desired targets.

The original proof-oriented build path remains available:

```bash
cd cell_os
make all phenotypes
make kernel kernel-arm
make pack-disk
scripts/run_x86_qemu.sh build/disk.img | tee /tmp/x86.log
scripts/run_arm64_qemu.sh build/arm64_kernel.elf | tee /tmp/arm.log
```

For a reproducible release drop:

```bash
cd cell_os
make release
ls ../out
```

### Cortex host inference

The native C runtime can be tested on the development host before booting the same CWM model under Cell OS:

```bash
make build/test_cortex_host
./build/test_cortex_host models/celllm_1m.cwm $'cell> show memory\n'
```

Expected semantic result:

```text
<CALL memory.status>
```

### Cortex x86 image

Build an x86_64 Cell OS image containing CellLM-1M as the Cortex payload:

```bash
make clean
make cortex-x86 CORTEX_MODEL=models/celllm_1m.cwm
```

Run it under QEMU:

```bash
make run-cortex-x86 CORTEX_MODEL=models/celllm_1m.cwm
```

A successful Cortex boot reports the model dimensions before presenting the interactive `cell>` prompt. The current interaction path is through COM1 in the reference QEMU configuration.

### Training CellLM-1M

Training is performed on a normal development host and is not part of the bare-metal runtime.

A Python virtual environment with CUDA-enabled PyTorch and NumPy is sufficient for the current model:

```bash
python3 -m venv .venv-celllm
source .venv-celllm/bin/activate
python -m pip install --upgrade pip
python -m pip install torch --index-url https://download.pytorch.org/whl/cu128
python -m pip install numpy
```

A short smoke training run can be used to validate the pipeline:

```bash
python tools/celllm_train.py \
    --steps 300 \
    --batch 64 \
    --output models/celllm_1m_smoke.cwm \
    --checkpoint checkpoints/celllm_1m_smoke.pt \
    --info models/celllm_1m_smoke.json
```

The current full CellLM-1M training run uses:

```bash
python tools/celllm_train.py \
    --steps 5000 \
    --batch 64 \
    --output models/celllm_1m.cwm \
    --checkpoint checkpoints/celllm_1m.pt \
    --info models/celllm_1m.json
```

The exporter produces the CWM1/Q8 checkpoint used directly by Cortex.

## Testing

The original `make test-all` path continues to exercise the host-side Cell OS components such as parcels, channel QoS, scheduler behavior, organelles, IRQ bridging, update and migration logic, observability, trust/MAC placeholders, KV storage, and phenotype loading.

The end-to-end proof tools remain available:

```bash
make run-x86-proof
make run-arm-proof
tools/verify_proofs.sh build/pf12_x86.log build/pf12_arm.log
```

The Cortex work adds three separate validation boundaries.

First, the PyTorch model is evaluated after training to verify that representative English requests produce the intended symbolic actions.

Second, the exported Q8 CWM1 model is loaded by the independent C Cortex runtime on the host. This checks that quantization and export preserve the intended behavior rather than relying only on the PyTorch checkpoint.

Third, the same CWM1 payload is packed into the x86 Cell OS image and executed through the bare-metal Cortex runtime under QEMU.

Representative validated host-side mappings for CellLM-1M include:

```text
hello                   -> Hello. I am Cell Cortex.
who are you             -> I am Cell Cortex, the personal intelligence of Cell OS.
show memory             -> <CALL memory.status>
what cpu do i have      -> <CALL cpu.info>
list disks              -> <CALL storage.list>
ls                      -> <CALL storage.list_dir>
show gpu                -> <CALL gpu.info>
list network interfaces -> <CALL network.list>
list usb devices        -> <CALL usb.list>
```

This validation demonstrates semantic mapping and inference consistency for the tested commands. It does not establish general language competence outside the narrow training distribution.

## Implementation Notes

The BIOS chain remains intentionally small. Stage 1 loads stage 2; stage 2 establishes the execution mode and page tables, loads the kernel, prepares boot metadata, and transfers control into the 64-bit kernel.

`handoff_t` remains the core substrate ABI for the original Cell OS path. Cortex-specific boot information is carried through an extension mechanism rather than casually changing the established proof structure.

Parcels retain their fixed framing and continue to serve as the common transport representation for Cell channels and observability.

The QoS subsystem maintains per-channel token buckets and GAS accounting. The scheduler still depends on external VM hooks for gene execution; Cortex is not presented as a replacement for that VM interface.

Organelles continue to provide bounded deterministic services. The intended capability-dispatch architecture for Cortex should connect learned symbolic requests to such bounded services rather than bypassing them.

The current ATA PIO path and COM1 interaction are reference mechanisms for establishing the inference path under QEMU. They are not intended to define the final portable hardware layer.

CellLM model training remains external because training and inference have different requirements. Cell OS needs only the compact model representation and deterministic inference implementation; it does not need PyTorch, CUDA, Python, or the optimizer state used during model development.

## Current Status

The repository currently establishes the following Cortex milestones:

- a native freestanding transformer inference implementation integrated with Cell OS;
- a compact CWM1 model representation with Q8 weights and integrity checks;
- loading of model weights as an independent disk payload;
- E820-backed memory discovery and an inference arena for the x86 Cortex path;
- an English-only 1,049,984-parameter CellLM-1M training pipeline;
- export from PyTorch into CWM1/Q8;
- host-side agreement for the tested semantic commands between the trained model and the independent C runtime;
- interactive Cell Cortex execution through the x86 QEMU reference environment.

The important result is not the size or sophistication of CellLM-1M. The result is that a learned model can be trained outside the system, reduced to a compact native representation, loaded by Cell OS, and used as a semantic component without introducing a conventional operating-system runtime beneath it.

## What Is Not Yet Demonstrated

The current work is deliberately narrower than the long-term architecture.

- CellLM-1M is not a general-purpose language model and is not an AGI system.
- Its training corpus is synthetic, small, and intentionally limited to a bounded set of English system-control expressions.
- Symbolic `<CALL ...>` outputs are not yet a complete closed-loop hardware-control mechanism. The capability dispatcher remains the next architectural step.
- Cortex does not yet provide a generalized driver layer for arbitrary Ethernet, GPU, NVMe, USB, display, or other PC hardware.
- The present x86 hardware path is validated primarily through the QEMU/PC BIOS reference environment with legacy ATA model loading and COM1 interaction.
- Real-hardware compatibility across heterogeneous PCs has not yet been established.
- The current inference engine is optimized for simplicity and inspectability rather than maximum throughput.
- There is no claim that CellLM-1M understands arbitrary natural-language requests outside its training distribution.
- No learned component is currently allowed to synthesize arbitrary privileged machine operations at runtime.
- The existing Cell proof ledger and Cortex semantic validation are related but are not yet a single unified formal proof system.

These limitations are intentional boundaries of the present milestone rather than hidden assumptions.

## Near-Term Work

The next useful steps are structural rather than simply increasing model size.

1. Implement a capability dispatcher that recognizes canonical Cortex calls such as `memory.status` and maps them to deterministic Cell services.
2. Feed structured capability results back into Cortex so a request can become a complete language → capability → result → language cycle.
3. Expand the generalized hardware layer through prevalidated backends rather than runtime-generated privileged code.
4. Extend tests so unsupported capabilities fail explicitly and safely.
5. Improve the inference kernels, beginning with wider SIMD and more efficient quantized matrix operations.
6. Add broader English paraphrase coverage and negative examples before increasing model scale.
7. Only after the semantic/control loop is stable, evaluate larger CellLM configurations such as 20M parameters.
8. Preserve the existing Cell OS proof discipline by adding measurable Cortex milestones rather than substituting model output for system evidence.

## Limitations and Future Work of the Original Cell Substrate

Several limitations of the original Cell OS remain relevant independently of Cortex.

- Cryptography and trust roots are still placeholders; production MAC implementations, key derivation, and key storage remain future work.
- Observability hashing uses a simple change-detection mixer rather than a collision-resistant digest.
- The scheduler depends on external VM hooks and still does not ship a general gene VM in the original proof path.
- The organelle KV store is bounded and volatile rather than persistent.
- The reference phenotypes cover only a small set of substrates; broader UEFI, ARM SoC, MCU, and physical PC coverage remains to be added.
- Security proofs still rely substantially on host-side verification and recorded logs. CI and physical hardware farms would materially increase confidence.

## License

The project is released under the MIT License. The original project version is tracked as `1.0.0-eukaryote-spine`; Cortex and CellLM are experimental extensions within the same repository and should not be interpreted as a change to the meaning of that historical version identifier unless the project version is explicitly advanced.

## Further reading

[The Cell OS Book](https://www.amazon.com/dp/B0GFD4PVH2/ref=mp_s_a_1_1?crid=OK7WJ6J0L33J&dib=eyJ2IjoiMSJ9.GIdeOEKSNrU21Yv-NV-udQ.382EYb6APk73zzOBvxGsoBqTxkIt4GnpAIHYOX-8e-E&dib_tag=se&keywords=ivan+gaydardzhiev&qid=1767777093&sprefix=ivan+gaydardzh%2Caps%2C723&sr=8-1)
