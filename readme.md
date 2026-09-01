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
- Reuse established Unix/POSIX command names and C/POSIX-shell syntax whenever those semantics already exist; Cell-specific terminology is reserved for mechanisms that do not have a suitable conventional equivalent.

## System Overview

The x86_64 boot chain begins with a 512-byte BIOS stage that loads the second stage and transfers control to it. Stage 2 enables the required processor state, enters long mode, prepares the memory mappings, loads the packed kernel image, and transfers execution to the kernel entry stub.

The kernel receives hardware and boot metadata through the fixed-size `handoff_t` ABI. Substrate probing records the execution environment and emits the E0 classification data. The runtime then constructs the channel graph, initializes QoS state, seeds the Golgi router and organelle set, prepares observability, and executes the proof path enabled for that build.

Scheduler ticks run gene tasks through VM hooks, account CPU, I/O, and memory consumption, and apply routing and QoS policy before actions are admitted. IRQ sources enter the event bridge, which turns bounded hardware events into parcel-ready frames. Observability records use the same channel machinery and can be replayed by host tooling. Update and migration logic uses dual genome strands and validates state before preference is changed.

The proof ledger `#E1` through `#E10` is intended to make the system inspectable. A mechanism is not treated as established merely because source code exists for it; the reference builds emit deterministic evidence that the expected path executed.

## Cortex: Cognitive Mediation Layer

Cell Cortex is an experimental native inference subsystem added to the x86_64 Cell OS path. Its role is semantic mediation: it receives a natural-language request, evaluates it with a compact causal language model, and either produces ordinary text or requests a named Cell capability. The capability request is intercepted by a deterministic dispatcher rather than exposed as a privileged machine operation. The dispatcher executes only registered operations, serializes the result internally, and renders the verified result into a bounded human-readable response. The present milestone deliberately does not retrain CellLM to narrate capability results: the learned component performs intent routing, while factual system state remains under deterministic control.

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


## Capability Dispatcher and Closed Loop

The first capability-dispatch milestone implements a small, explicit registry rather than a generic command interpreter. A model-produced call is accepted only when the complete generated line matches one of the registered canonical forms. There is no prefix matching, arbitrary argument parsing, shell expansion, MMIO expression evaluation, or runtime driver synthesis.

The current registry contains:

```text
system.status
cpu.info
memory.status
storage.list
storage.list_dir
storage.pwd
network.list
gpu.info
usb.list
display.info
power.status
```

The presence of a name in the registry does not imply that its hardware backend exists. The current deterministic implementations are deliberately limited:

- `memory.status` derives memory information from the boot handoff, E820 map, and current Cortex memory arena;
- `cpu.info` reads the x86 CPUID vendor/family/model/logical-processor fields;
- `system.status` reports Cortex readiness, E820-derived usable memory, and whether the current ATA boot/model path is active;
- `storage.list` reports the verified `ata0` path after the Cortex model has been successfully read;
- `storage.list_dir` and `storage.pwd` are connected to the Cell VFS when CellFS is mounted;
- network, GPU, USB, display, and power requests currently return explicit `status=unsupported` results because those hardware backends are not yet implemented.

Capability results are serialized into a bounded ASCII representation, for example:

```text
<RESULT memory.status status=ok top_mib=512 usable_mib=511 free_mib=486>
<RESULT storage.list_dir status=unsupported reason=no_vfs>
```

These result records are internal mediation data. Normal interactive use does not expose the intermediate `<CALL>` or `<RESULT>` records. The current session layer converts the verified result into a deterministic English response. This keeps dynamic hardware facts outside the language model until enough real execution traces exist to justify a later training pass.

The current session layer permits at most one capability execution per human request. There is no recursive planner and no capability chain in this milestone. This is a deliberate bound, not a claim that one-shot routing is the final interaction architecture.

## Cell VFS and CellFS-1

The second Cortex substrate milestone adds a Cell-native virtual filesystem and a small persistent filesystem rather than importing a POSIX filesystem as an architectural dependency. The user-facing path syntax is intentionally familiar, but the namespace is not merely a disk directory tree.

The current root is:

```text
/
├── home/          persistent CellFS-1
├── programs/      persistent CellFS-1
├── models/        live virtual nodes
├── system/        live virtual nodes
└── devices/       live capability projections
```

`/home` and `/programs` are backed by the writable CellFS-1 volume. `/system`, `/devices`, and `/models` are generated from live Cell state. For example, reading `/devices/cpu` invokes the same deterministic CPU capability used by Cortex; no copy of the CPU description is stored as a disk file.

The deterministic console surface intentionally reuses Unix/POSIX names instead of introducing Cell-specific synonyms:

```sh
pwd
ls [path]
cd [path]
cat <path>
stat <path>
mkdir <path> [...]
touch <path> [...]
rm <path> [...]
rmdir <path> [...]
echo [text ...]
echo "text" > <path>
echo "text" >> <path>
```

The current shell lexer supports ordinary words, backslash quoting, single and double quotes, and stdout redirection with `>` and `>>`. Pipes, variables, pathname expansion, command lists, input redirection, background jobs, and the rest of the POSIX shell grammar are not implemented yet. The syntax is intentionally being extended in the POSIX-shell direction rather than by adding parallel Cell-only commands.

These exact shell operations bypass language-model inference. Natural-language requests that CellLM-1M already knows, such as directory listing and working-directory queries, route through `storage.list_dir` and `storage.pwd` onto the same VFS state. There is no separate AI filesystem. Shell command recognition takes precedence over CellLM inference, while unrecognized natural-language input falls through to Cortex.

CellFS-1 uses a deliberately small on-disk ABI: a 512-byte CRC-protected superblock, 64 fixed-size CRC-protected inode records, a parent/name directory namespace, and an append-only data allocator. The current maximum file size is 16 KiB. The writable CellFS image is kept independently from build scratch state, so `make clean` does not destroy persistent files. The QEMU run path synchronizes the writable CellFS tail back to `state/cellfs.img` after the reference machine exits.

CellFS-1 is not presented as a mature general-purpose filesystem. It currently has no free-space reclamation, journal, transactional update protocol, permissions, links, sparse files, or large-file extent tree. Those are explicit later concerns. Corrupt structural metadata and CRC failures are rejected during mount rather than accepted silently.

Text and binary access are also separated. `cat` accepts printable text files only; binary payloads such as CellExec images are loaded through the binary VFS path and are not emitted as control bytes to the console.

## CellExec-1 and Task Execution

The third substrate milestone establishes a runnable program boundary. Cell OS still does **not** execute arbitrary native x86 machine code from writable storage. Programs use a compact verified format named **CellExec-1** and run inside a bounded deterministic bytecode executor.

A CellExec-1 image contains a fixed 64-byte header followed by fixed-width 8-byte instructions and an optional read-only data section. The header records code/data sizes, entry point, task-memory requirement, gas limit, declared capability mask, total image size, and CRC32 over the executable payload. The loader validates the complete image before creating a runnable task.

The current verifier rejects at least:

- invalid magic/version/header fields or inconsistent sizes;
- payload CRC mismatch;
- unknown opcodes or invalid registers;
- entry points and branch targets outside the code section;
- constant-data references outside the data section;
- unknown capability bits;
- capability instructions not explicitly declared by the executable;
- task-memory requests above 4096 bytes;
- zero or excessive gas budgets.

The first instruction set is intentionally small: halt, immediate/register moves, integer add/subtract/multiply, bounded relative branches, constant text output, explicit capability calls, and byte load/store operations in task-local memory. The purpose is to establish the program ABI and execution boundary, not to claim a finished programming environment.

Every program execution creates an internal task record with an explicit lifecycle:

```text
ready -> running -> exited
                 -> faulted
```

The shell exposes that record through a process-style PID rather than requiring a separate Cell command vocabulary. Records retain exit/fault state, gas consumption, program path, and declared/granted capability masks. The current implementation stores the most recent eight records. Execution is synchronous in this milestone: task-local registers and memory are reused only after the previous task has stopped. There is no claim of preemptive multitasking yet.

Capability ownership is explicit. A CellExec image must declare every capability it can invoke, and the declaration is still insufficient on its own: the task manager intersects it with the Cell OS task policy. The current default program policy permits read-only system/CPU/memory/storage observation capabilities. A program that requests another registered capability is rejected before execution even if the bytecode itself is otherwise valid. CellExec execution is also restricted to the `/programs` namespace.

Program invocation follows ordinary shell expectations. `/programs` is the current executable search path, so a program can be invoked by name or explicit pathname:

```sh
hello
/programs/observe
ps
ps -p 1
```

The persistent filesystem exposes the installed names `hello` and `observe`; the `.cellx` suffix is an internal build artifact, not part of the user-facing command name. The installer migrates the earlier proof-era `/programs/hello.cellx` and `/programs/observe.cellx` inode names in place when the payloads match. `ps` reports retained execution records and `ps -p PID` reports one record. The older proof commands `run`, `tasks`, `task`, `write`, and `append` are not part of the shell interface.

The fourth substrate milestone adds a bootstrap C programming path without changing the CellExec-1 execution boundary. `cc` is now both a host build tool and a deterministic shell builtin inside Cell OS. Both forms use the same parser and code generator and emit verified CellExec-1 images; compilation does not involve CellLM.

The current C subset is deliberately small and explicit. It accepts `int main(void)`, local `int` variables, assignment, parentheses, unary minus, integer `+`, `-`, and `*`, blocks, `if`/`else`, `while`, `puts("literal")`, `cell_capability("name")`, and a constant integer `return`. It accepts the project SDK include lines for `<stdio.h>` and `<cell.h>`. Unsupported C surface is rejected with source line/column diagnostics rather than silently reinterpreted.

The initial SDK lives under `sdk/include/`. `stdio.h` declares `puts`, and `cell.h` declares the explicit capability-call interface used by the bootstrap compiler. These headers define the intended source contract; the current compact frontend recognizes the supported builtins directly and does not yet implement a general preprocessor or linker.

The build now compiles the proof programs from C:

- `hello`, built from `programs/hello.c`, writes a constant message and exits;
- `observe`, built from `programs/observe.c`, invokes the declared `system.status` and `memory.status` capabilities and renders their deterministic results.

The low-level `tools/cellasm.py` assembler remains available as a bootstrap, debugging, and backend tool; it is not a separate high-level Cell language. The source-level direction remains C and the scripting direction remains POSIX shell.

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
- `src/core/cellfs.c` and `src/core/vfs.c`  persistent CellFS-1 storage and the unified live/persistent namespace.
- `src/core/cellexec.c` and `src/core/task.c`  CellExec-1 verifier plus synchronous bounded task execution.
- `src/core/cc.c` and `include/core/cc.h`  bootstrap C parser/code generator shared by host `cc` and the target shell builtin.
- `tools/cc.c`  host command-line frontend for the same bootstrap C compiler.
- `sdk/include/`  initial C-facing SDK declarations for standard output and explicit Cell capabilities.
- `src/core/shell.c`  deliberately small POSIX-shell-compatible command surface, quoting/redirection parser, `/programs` lookup, `cc`, and `ps` interface.
- `programs/`  C proof programs compiled into CellExec-1 and installed persistently into `/programs`.
- `src/drivers/x86/ata_pio.c`  current x86 ATA PIO path used to load the model payload.
- `src/cortex/cwm.c`  CWM1 parser and model validation.
- `src/cortex/cortex.c`  native causal transformer inference implementation.
- `src/kernel/cortex_boot.c`  Cortex model loading and interactive execution path.
- `tools/celllm_train.py`  CellLM-1M PyTorch training and CWM1 export tool.
- `tests/test_cortex_host.c`  native host-side CWM1/Cortex inference test.
- `tests/test_cellexec.c`, `tests/test_task.c`, and `tests/test_program_image.c`  executable ABI, task-policy, gas/memory, and persistent-program execution tests.
- `models/`  exported CWM checkpoints and model metadata.
- `data/`  current CellLM training corpus.
- `checkpoints/`  development training checkpoints where retained intentionally.
- `tools/phenotype_compile.py`, `tools/gen_cgf.py`, `tools/obs_replay.c`, and `tools/verify_proofs.sh`  original phenotype, CGF, observability, and proof tooling.
- `phenotypes/`, `schemas/`, `proofs/`, `out/`, and `build/`  original configuration, validation, release, and scratch paths.

## Build and Run

The top-level `Makefile` now contains both the current Cortex/CellFS/CellExec programming path and the consolidated historical Cell OS proof/subsystem graph. The older milestone-specific `e*.mk` fragments are no longer required. Legacy proof targets remain available alongside the active Cortex targets, while the current reference release path remains explicit about the supported x86 Cortex configuration.

The Cortex x86 path requires a POSIX development host with GCC/binutils, NASM, Python, Make, and QEMU for the reference boot test. Host-side C compilation uses the same bootstrap compiler core that is linked into the target kernel.

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

A successful Cortex boot reports the model dimensions before presenting the interactive `cell$` prompt. The user-facing prompt is shell-oriented; Cortex still feeds the historical `cell> ` prefix internally to CellLM-1M because that prefix is part of the current training corpus. The current interaction path uses the reference x86 serial-console backend in QEMU. The hardware binding is an implementation detail rather than part of the Cortex or VFS interface.

During image construction, the proof programs in `programs/` are compiled from C into CellExec-1 and installed idempotently into the existing persistent CellFS image. Existing `/home` content is not reformatted. The same steps can be invoked explicitly:

```bash
make cell-programs
make install-cell-programs
```

The host bootstrap compiler can also be used directly:

```bash
make build/cc
build/cc programs/hello.c -o build/programs/hello.cellx
python3 scripts/cellfs_install.py state/cellfs.img build/programs/hello.cellx --name hello
```

More importantly, C source can be compiled after boot using the normal `cc` command. For example:

```text
cell$ echo '#include <stdio.h>' > /home/hello.c
cell$ echo 'int main(void) { puts("Hello from target cc"); return 0; }' >> /home/hello.c
cell$ cc /home/hello.c -o /programs/hello2
cell$ hello2
Hello from target cc
```

The compiler output is a normal binary VFS file and remains executable after CellFS remount/rebuild. The low-level `tools/cellasm.py` path remains available for CellExec diagnostics and backend development.

After boot, the bundled proof programs can be inspected and executed with the shell interface:

```text
cell$ ls /programs
hello  observe
cell$ hello
Hello from C on Cell OS.
cell$ /programs/observe
C observation:
...
cell$ ps
PID STATE EXIT GAS COMMAND
...
```

File output uses normal shell redirection syntax:

```sh
echo "hello" > /home/test
echo "more" >> /home/test
cat /home/test
```

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

The original Cell OS source tree contains host tests and proof tooling for parcels, channel QoS, scheduler behavior, organelles, IRQ bridging, update and migration logic, observability, trust/MAC placeholders, KV storage, phenotype loading, and the x86/arm64 proof paths. Their previous aggregate Makefile wiring is not currently part of the Cortex-focused top-level `Makefile`; those targets should be re-integrated rather than described as already active.

The Cortex capability loop adds a deterministic regression target before image construction:

```bash
make test-caploop CORTEX_MODEL=models/celllm_1m.cwm
```

`test-capability` verifies exact call parsing, rejection of unknown names, E820/memory result serialization, CPU result structure, system status, and deterministic rendering. `test-cellfs`/`test-vfs` verify persistent remount, namespace semantics, live capability nodes, and the binary/text boundary. `test-cellexec` validates the executable ABI and static verifier. `test-task` exercises lifecycle, gas exhaustion, task-local memory, capability policy, and the `/programs` execution boundary. `test-shell` verifies Unix/POSIX command names, shell quoting, `>`/`>>` redirection, `/programs` lookup, `ps`, and absence of the earlier proof command vocabulary. `test-program-image` executes assembler-produced programs after they have been installed into a persistent CellFS image. `test-cortex-session` drives the same shell, VFS, process/task, and learned capability-routing paths used by the bare-metal interactive session.

The Cortex work also retains three model/runtime validation boundaries.

First, the PyTorch model is evaluated after training to verify that representative English requests produce the intended symbolic actions.

Second, the exported Q8 CWM1 model is loaded by the independent C Cortex runtime on the host. This checks that quantization and export preserve the intended behavior rather than relying only on the PyTorch checkpoint.

Third, the same CWM1 payload is packed into the x86 Cell OS image and executed through the bare-metal Cortex runtime under QEMU.

Representative model-only host mappings for CellLM-1M include:

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

These are model-runtime validation examples, not the final shell dispatch order. In the interactive system, established shell commands such as `ls` and installed executable names such as `hello` are handled deterministically before any input is offered to CellLM. Unrecognized natural-language text falls through to Cortex. This validation demonstrates semantic mapping and inference consistency for the tested model prompts; it does not establish general language competence outside the narrow training distribution.

## Implementation Notes

The BIOS chain remains intentionally small. Stage 1 loads stage 2; stage 2 establishes the execution mode and page tables, loads the kernel, prepares boot metadata, and transfers control into the 64-bit kernel.

`handoff_t` remains the core substrate ABI for the original Cell OS path. Cortex-specific boot information is carried through an extension mechanism rather than casually changing the established proof structure.

Parcels retain their fixed framing and continue to serve as the common transport representation for Cell channels and observability.

The QoS subsystem maintains per-channel token buckets and GAS accounting. The scheduler still depends on external VM hooks for gene execution; Cortex is not presented as a replacement for that VM interface.

Organelles continue to provide bounded deterministic services. The intended capability-dispatch architecture for Cortex should connect learned symbolic requests to such bounded services rather than bypassing them.

The current ATA PIO path and reference serial-console interaction are reference mechanisms for establishing the inference path under QEMU. They are not intended to define the final portable hardware layer.

CellLM model training remains external because training and inference have different requirements. Cell OS needs only the compact model representation and deterministic inference implementation; it does not need PyTorch, CUDA, Python, or the optimizer state used during model development.


### Source and License Policy

Existing source-file license and copyright headers are part of the source and
must be preserved when files are modified. New source files should use the
project's corresponding license header. Refactoring or regeneration must not
silently strip those headers.

User-facing commands should reuse established Unix/POSIX names and semantics
where they already exist. Compiled source-language work is intended to follow
C syntax, while command scripting is intended to grow in the POSIX-shell
direction. Cell-specific names are reserved for genuinely Cell-specific
mechanisms such as CWM1, CellFS, CellExec, capability mediation, and the
biological substrate vocabulary.

## Current Status

The repository currently establishes the following Cortex milestones:

- a native freestanding transformer inference implementation integrated with Cell OS;
- a compact CWM1 model representation with Q8 weights and integrity checks;
- loading of model weights as an independent disk payload;
- E820-backed memory discovery and an inference arena for the x86 Cortex path;
- an English-only 1,049,984-parameter CellLM-1M training pipeline;
- export from PyTorch into CWM1/Q8;
- host-side agreement for the tested semantic commands between the trained model and the independent C runtime;
- interactive Cell Cortex execution through the x86 QEMU reference environment;
- a strict capability dispatcher with deterministic implementations for memory, CPU, system status, and current ATA storage enumeration;
- structured capability results and deterministic human-readable responses without retraining CellLM;
- CellLM-1M weights are unchanged by this milestone; no result-grounding retraining is required for the present deterministic loop;
- explicit safe failure for registered capabilities whose deterministic backends are not yet present;
- a Cell-native VFS combining persistent `/home` and `/programs` with live `/system`, `/devices`, and `/models` nodes;
- persistent CellFS-1 storage with CRC-protected metadata and successful remount/rebuild persistence;
- a verified CellExec-1 executable ABI with fixed-width bytecode, CRC, declared capabilities, bounded task memory, and gas;
- a synchronous task lifecycle/history layer with explicit task capability policy and execution restricted to `/programs`;
- C-to-CellExec-to-CellFS integration tests for persistent Cell programs, while retaining the low-level assembler as a backend/debugging path;
- a bootstrap `cc` frontend shared between the development host and Cell OS itself, with target-side compile/install/run and remount persistence tests;
- a POSIX-oriented shell surface with conventional command names, quoting, `>`/`>>` redirection, `/programs` lookup, `cc`, and `ps` process-history inspection.

The important result is not the size or sophistication of CellLM-1M. The result is that a learned model can be trained outside the system, reduced to a compact native representation, loaded by Cell OS, and used as a semantic component without introducing a conventional operating-system runtime beneath it.


## Validated Interactive Milestone

The current x86_64 QEMU reference path has been exercised end to end with the
unchanged CellLM-1M CWM1/Q8 checkpoint and the POSIX-oriented command surface.

The validated interactive path includes:

```text
cell$ ls /programs
hello  observe

cell$ hello
Hello from CellExec-1.

cell$ /programs/observe
CellExec observation:
Cell OS is running. Cortex is ready; 511 MiB usable memory; storage ata0.
Memory: 511 MiB usable; ... MiB remain available to Cell OS.

cell$ ps
PID STATE EXIT GAS COMMAND
...

cell$ echo "hello world" > /home/posix-test
cell$ echo "second line" >> /home/posix-test
cell$ cat /home/posix-test
hello world
second line
```

The persistent CellFS state survives the normal `make clean` / image rebuild
workflow through `state/cellfs.img`. The build/install path is idempotent for
the proof programs in `/programs`; rebuilding does not rewrite unchanged
program payloads or require reformatting the filesystem.

Natural-language requests continue to fall through to the unchanged CellLM-1M
model only after deterministic shell-command and executable lookup. For
example, `show memory` is interpreted by CellLM and resolved through the
deterministic `memory.status` capability, while `ls`, `echo`, `ps`, and
installed program names never require model inference.

This validation establishes the current reference behavior. It does not claim
complete POSIX conformance, concurrent process semantics, a mature filesystem,
or portable PC hardware support beyond the tested reference backends.

## Bootstrap C Programming Milestone

The fourth implementation milestone establishes a C-to-CellExec path on both sides of the boot boundary. The same compiler core is exercised as a host tool (`build/cc`) and linked into the Cell OS shell as `cc`. Host regression tests compile C source, verify the resulting CellExec-1 image, install it into persistent CellFS, execute it through the task manager, and remount the filesystem before executing the compiled program again.

The target shell interface is intentionally conventional:

```text
cell$ cc /home/program.c -o /programs/program
cell$ program
cell$ ps
```

No new Cell-specific command is introduced for compilation. The compiler is deterministic and does not use CellLM. The current compiler object is built with `-Os` inside the freestanding kernel while the rest of the reference kernel retains its existing optimization policy; this keeps the tested kernel image at 60,748 bytes and restores more than 4 KiB of margin beneath the current 65,024-byte BIOS staging limit.

Host and freestanding compilation are validated in the development environment. A target-side QEMU interaction with the new `cc` command remains the final external validation step for this milestone.

## What Is Not Yet Demonstrated

The current work is deliberately narrower than the long-term architecture.

- CellLM-1M is not a general-purpose language model and is not an AGI system.
- Its training corpus is synthetic, small, and intentionally limited to a bounded set of English system-control expressions.
- The current closed loop is intentionally single-action: CellLM routes the request once, the deterministic substrate executes at most one capability, and it does not yet plan or execute multi-step sequences.
- CellExec tasks are currently synchronous; there is no preemptive or concurrent task scheduler in the Cortex execution path yet.
- CellExec-1 intentionally does not admit arbitrary native x86 instructions from writable storage.
- CellFS-1 currently uses append-only data allocation and has no reclamation or journal.
- The current C compiler is a bootstrap subset rather than a complete ISO C implementation. It has no general preprocessor, pointers, arrays, structs/unions, user-defined functions beyond `main`, object files, linker, dynamic libraries, argv/environment model, or full libc.
- The target `cc` currently resides in the trusted kernel/shell image rather than running as an ordinary CellExec process; moving the compiler into user-space execution and eventually self-hosting it are future milestones.
- Shell operations and executable lookup are deterministic and do not require CellLM retraining; unrecognized natural-language requests continue to fall through to Cortex.
- Cortex does not yet provide verified Ethernet, GPU, NVMe, USB, display, or power backends; those registered capabilities currently report `unsupported`.
- The present x86 hardware path is validated primarily through the QEMU/PC BIOS reference environment with legacy ATA model loading and the reference serial-console backend.
- Real-hardware compatibility across heterogeneous PCs has not yet been established.
- The current inference engine is optimized for simplicity and inspectability rather than maximum throughput.
- There is no claim that CellLM-1M understands arbitrary natural-language requests outside its training distribution.
- No learned component is currently allowed to synthesize arbitrary privileged machine operations at runtime.
- The existing Cell proof ledger and Cortex semantic validation are related but are not yet a single unified formal proof system.

These limitations are intentional boundaries of the present milestone rather than hidden assumptions.

## Near-Term Work

The next useful steps remain structural rather than simply increasing model size.

1. Grow the C subset and SDK deliberately toward useful freestanding C semantics, including function calls, pointers/arrays, argv-style process input, more libc primitives, and a separable object/link stage where justified.
2. Move `cc` out of the trusted kernel path into a normal bounded program and work toward self-hosting the compiler using the same CellExec/task capability model.
3. Evolve the synchronous task runner into a scheduler with explicit task budgets, blocking/wakeup semantics, events, and capability ownership that can integrate with the older Cell scheduler/GAS concepts.
4. Add program access to carefully scoped VFS operations through task capabilities rather than exposing filesystem internals directly, and later expose process/device/system state through Cell-native synthetic `/proc`, `/dev`, and `/sys` namespaces.
5. Expand the generalized hardware layer through prevalidated backends, with AHCI/NVMe and networking ahead of more complex GPU/USB work.
6. Improve the inference kernels, beginning with wider SIMD and more efficient quantized matrix operations.
7. Accumulate real capability, process, filesystem, and compilation traces first; only then retrain CellLM for broader paraphrases, program management, or result narration.
8. Add persistent Cortex memory as substrate data rather than encoding machine state into model weights.
9. Preserve the existing Cell OS proof discipline by adding measurable Cortex/VFS/CellExec/compiler milestones rather than substituting model output for system evidence.

## Limitations and Future Work of the Original Cell Substrate

Several limitations of the original Cell OS remain relevant independently of Cortex.

- Cryptography and trust roots are still placeholders; production MAC implementations, key derivation, and key storage remain future work.
- Observability hashing uses a simple change-detection mixer rather than a collision-resistant digest.
- The scheduler depends on external VM hooks and still does not ship a general gene VM in the original proof path.
- The organelle KV store is bounded and volatile rather than persistent.
- The reference phenotypes cover only a small set of substrates; broader UEFI, ARM SoC, MCU, and physical PC coverage remains to be added.
- Security proofs still rely substantially on host-side verification and recorded logs. CI and physical hardware farms would materially increase confidence.

## License

Cell OS is released under the GNU General Public License version 3 or later (`GPL-3.0-or-later`). The complete license text is provided in `COPYING`. Source-file SPDX identifiers and copyright headers are part of the source and are preserved when files are modified. The project version is tracked separately in `VERSION`.

## Further reading

[The Cell OS Book](https://www.amazon.com/dp/B0GFD4PVH2/ref=mp_s_a_1_1?crid=OK7WJ6J0L33J&dib=eyJ2IjoiMSJ9.GIdeOEKSNrU21Yv-NV-udQ.382EYb6APk73zzOBvxGsoBqTxkIt4GnpAIHYOX-8e-E&dib_tag=se&keywords=ivan+gaydardzhiev&qid=1767777093&sprefix=ivan+gaydardzh%2Caps%2C723&sr=8-1)
