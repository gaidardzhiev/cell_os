# cell_os

Cell OS is a boot-to-proof microkernel and experimental computational substrate. It exercises its own BIOS-stage loaders, handoff ABI, channel graph, organelles, resource accounting, observability path, update mechanisms, and proof chain on x86_64 PC BIOS and arm64 QEMU virt targets. The same source tree builds boot media, phenotypes, host tools, validation artifacts, and reproducible release packages.

The project is organized around a biological vocabulary, but the implementation is intentionally concrete. A substrate describes the machine on which the cell executes; genes describe executable behaviors; organelles provide bounded services; channels carry parcels under explicit routing and QoS rules; mitochondria account for resource consumption; the Golgi layer mediates routing; and the proof ledger records whether the expected mechanisms actually executed.

A recent experimental extension adds **Cell Cortex**, a small native neural inference subsystem intended to provide a cognitive mediation layer between natural-language intent and Cell OS capabilities. Cortex does not replace the deterministic substrate. It runs above it and is constrained by it. The present model, **CellLM-1M**, is a deliberately small English-language causal decoder trained to map a bounded set of natural-language computer-control requests into symbolic Cell capability calls.

The current work should therefore be understood as two interacting layers:

- a deterministic Cell OS substrate responsible for boot, memory, devices, routing, accounting, and proof;
- an experimental cognitive layer responsible for interpreting human language and proposing symbolic actions within the capabilities exposed by the substrate.

The long-term objective is not to place an unrestricted language model in the hardware control path. It is to investigate whether a compact learned system can serve as a semantic interface to a small, explicit, and verifiable machine substrate.

Cell OS has now crossed an important implementation boundary. The running x86_64 reference system can accept C source from its own persistent filesystem, compile that source with the native `cc` command, emit a verified CellExec-1 program into `/programs`, pass real `argc/argv` process input, execute the result as a bounded task, retain its exit status and gas usage, and execute the persisted program again on later runs. The same system simultaneously hosts the CellLM-1M inference path, deterministic capability mediation, CellFS storage, VFS namespaces, and the CellExec verifier. This is not yet a self-hosting C system, but it is already a complete source-to-execution programming loop inside Cell OS itself.

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

The instruction set remains intentionally small but now supports the process and C-runtime work required by the current programming milestones. In addition to halt, register/immediate moves, arithmetic, branches, constant output, explicit capability calls, and byte load/store operations, CellExec-1 now provides the bounded operations required for 64-bit pointer loads, division and modulo, comparisons, dynamic string/character output, and the initial C runtime primitives used by compiled programs. The purpose remains a verified program ABI and execution boundary rather than unrestricted native-code execution.

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

The fourth substrate milestone adds a bootstrap C programming path without changing the CellExec-1 execution boundary. `cc` is both a host build tool and a deterministic shell builtin inside Cell OS. Both forms use the same parser and code generator and emit verified CellExec-1 images; compilation does not involve CellLM.

The current process/runtime milestone extends that source contract with `int main(int argc, char **argv)`, task-local argument vectors, `char *` and `char **` access required for process arguments, `argv[i]`, dynamic `puts`, `putchar`, `strlen`, `strcmp`, `atoi`, integer division and modulo, comparison operators, unary logical negation, and expression-valued `return`. The compiler remains deliberately bounded. Unsupported C surface is rejected with source line/column diagnostics rather than silently reinterpreted.

The initial SDK lives under `sdk/include/`. It now provides the current contracts for `<stdio.h>`, `<string.h>`, `<stdlib.h>`, and `<cell.h>`. The compact frontend recognizes the supported runtime operations directly and still does not claim a general preprocessor, object-file toolchain, or full libc.

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

- `src/boot/`: BIOS first and second stages, long-mode transition, substrate preparation, and Cortex boot metadata.
- `src/kernel/`: kernel entry, runtime support, Cortex bootstrap, console path, and linker scripts.
- `include/core/handoff.h` and related substrate headers: pinned boot and substrate metadata.
- `src/core/channel_graph.c` and `include/core/channel.h`: channel QoS and adjacency graph.
- `src/core/qos.c` and `src/core/scheduler.c`: token-bucket QoS, GAS accounting, and gene scheduling hooks.
- `src/core/organelles.c` and `src/genes/`: organelle services and gene behaviors.
- `src/core/irq_bridge.c` and `src/core/events.c`: interrupt-to-event conversion.
- `src/core/log_ring.c`, `src/core/ch0_log.c`, `src/core/obs.c`, and `src/core/obs_trace.c`: observability and replay-oriented logging.
- `src/core/update.c`: dual-strand update and migration logic.
- `libparcel/parcel.c`: parcel framing shared by target and host tooling.
- `src/core/mem_arena.c`: bounded memory arena used by Cortex and other native allocations.
- `src/core/cellfs.c` and `src/core/vfs.c`: persistent CellFS-1 storage and the unified live/persistent namespace.
- `src/core/cellexec.c` and `src/core/task.c`: CellExec-1 verifier plus synchronous bounded task execution, file descriptors, syscall dispatch, errno, bounded allocation, and the bounded function call stack.
- `src/core/cc.c` and `include/core/cc.h`: bootstrap C parser/code generator shared by host `cc` and the target shell builtin.
- `tools/cc.c`: host command-line frontend for the same bootstrap C compiler.
- `sdk/include/`: initial C-facing SDK declarations for standard output, strings, integer conversion, file descriptors, file I/O, errno, bounded allocation, and explicit Cell capabilities.
- `src/core/shell.c`: deliberately small POSIX-shell-compatible command surface, quoting/redirection parser, `/programs` lookup, `cc`, and `ps` interface.
- `programs/`: C proof programs including `hello`, `observe`, and the argument/runtime proof, compiled into CellExec-1 and installed persistently into `/programs`.
- `src/drivers/x86/ata_pio.c`: current x86 ATA PIO path used to load the model payload.
- `src/cortex/cwm.c`: CWM1 parser and model validation.
- `src/cortex/cortex.c`: native causal transformer inference implementation.
- `src/kernel/cortex_boot.c`: Cortex model loading and interactive execution path.
- `tools/celllm_train.py`: CellLM-1M PyTorch training and CWM1 export tool.
- `tests/test_cortex_host.c`: native host-side CWM1/Cortex inference test.
- `tests/test_cellexec.c`, `tests/test_task.c`, `tests/test_c_runtime.c`, and `tests/test_program_image.c`: executable ABI, task-policy, process ABI, C runtime, gas/memory, and persistent-program execution tests.
- `models/`: exported CWM checkpoints and model metadata.
- `data/`: current CellLM training corpus.
- `checkpoints/`: development training checkpoints where retained intentionally.
- `tools/phenotype_compile.py`, `tools/gen_cgf.py`, `tools/obs_replay.c`, and `tools/verify_proofs.sh`: original phenotype, CGF, observability, and proof tooling.
- `phenotypes/`, `schemas/`, `proofs/`, `out/`, and `build/`: original configuration, validation, release, and scratch paths.

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

Compiled programs can receive ordinary shell arguments:

```text
cell$ cc /home/argv.c -o /programs/argv
cell$ argv hello
hello
cell$ argv test
test
cell$ ps
PID STATE EXIT GAS COMMAND
1 exited 2 13 /programs/argv
2 exited 2 13 /programs/argv
```

After boot, the bundled proof programs can be inspected and executed with the shell interface:

```text
cell$ ls /programs
hello  observe  args
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

The top-level `Makefile` now carries both the active Cortex/CellFS/CellExec/C programming path and the consolidated historical Cell OS proof/subsystem graph. Host tests and proof tooling remain available for parcels, channel QoS, scheduler behavior, organelles, IRQ bridging, update and migration logic, observability, trust/MAC placeholders, KV storage, phenotype loading, and the x86/arm64 proof paths without requiring the former milestone-specific `e*.mk` fragments.

The Cortex capability loop adds a deterministic regression target before image construction:

```bash
make test-caploop CORTEX_MODEL=models/celllm_1m.cwm
```

`test-capability` verifies exact call parsing, rejection of unknown names, E820/memory result serialization, CPU result structure, system status, and deterministic rendering. `test-cellfs`/`test-vfs` verify persistent remount, namespace semantics, live capability nodes, and the binary/text boundary. `test-cellexec` validates the executable ABI and static verifier. `test-task` exercises lifecycle, gas exhaustion, task-local memory, capability policy, and the `/programs` execution boundary. `test-cc` verifies the supported C parser/code generator, CellExec validation, capability inference, unsupported-surface rejection, and the current `argc/argv` and pointer/runtime subset. `test-c-runtime` verifies the process ABI, character-pointer and argument indexing, `puts`/`putchar`/`strlen`/`strcmp`/`atoi`, arithmetic comparisons, and expression returns. `test-c-system` verifies file descriptors, `open`/`close`/`read`/`write`/`lseek`, standard descriptors, `errno`, bounded allocation, writable namespace policy, live VFS reads, user-defined functions, bounded recursion, and pointer-indexed byte stores. `test-shell` verifies Unix/POSIX command names, quoting, `>`/`>>` redirection, `/programs` lookup, `cc`, persistent compiled programs, process argument forwarding, target-compiled descriptor I/O, `ps`, and absence of the earlier proof command vocabulary. `test-program-image` executes compiled programs after installation into persistent CellFS. `test-cortex-session` drives the same shell, VFS, process/task, compiler, and learned capability-routing paths used by the bare-metal interactive session.

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

The repository currently establishes the following Cortex and native-programming milestones:

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
- explicit safe failure for registered capabilities whose deterministic backends are not yet present;
- a Cell-native VFS combining persistent `/home` and `/programs` with live `/system`, `/devices`, and `/models` nodes;
- persistent CellFS-1 storage with CRC-protected metadata and successful remount/rebuild persistence;
- a verified CellExec-1 executable ABI with fixed-width bytecode, CRC, declared capabilities, bounded task memory, and gas;
- a synchronous task lifecycle/history layer with explicit task capability policy and execution restricted to `/programs`;
- a bootstrap `cc` frontend shared between the development host and Cell OS itself;
- target-side C source compilation directly inside the running Cell OS reference system;
- persistent installation of compiler output into `/programs` and later execution through the normal task verifier;
- a real task-local C process ABI with `argc/argv`;
- a task-local file-descriptor ABI with conventional descriptors `0`, `1`, and `2`;
- C-facing `open`, `close`, `read`, `write`, and `lseek` operations routed through the Cell VFS;
- POSIX-style `errno` propagation and a bounded task-local `malloc`/`free` allocator;
- user-defined integer functions with bounded call depth, including forward calls and recursion;
- pointer-indexed byte stores sufficient for mutable task-local C buffers;
- the initial pointer/runtime surface required for command-line C programs, including `char *`, `char **`, `argv[i]`, `puts`, `putchar`, `strlen`, `strcmp`, and `atoi`;
- integer division, modulo, comparison operators, logical negation, and expression-valued returns in the bootstrap compiler;
- shell argument forwarding from quoted POSIX-oriented command input into compiled CellExec tasks;
- process-history inspection through `ps`, including exit status, gas usage, and executable path;
- a consolidated top-level build graph that retains the historical Cell OS proof targets without separate `e*.mk` fragments;
- a freestanding x86 Cortex kernel measured at 59,852 bytes with the native C system interface included, remaining beneath the unchanged 65,024-byte conservative BIOS staging limit.

The important result is no longer only that CellLM-1M can run natively. Cell OS now joins learned semantic mediation, deterministic capabilities, persistent storage, a verified executable format, a task process ABI, and a native C compilation path in one bootable system. C source entered at the `cell$` prompt can become a persistent verified program and execute without Linux, libc, GCC, Python, or another conventional operating system existing beneath the target runtime.

That does not make Cell OS self-hosting yet. It does establish the substrate needed to pursue self-hosting as an engineering problem rather than as a hypothetical future architecture.

## Validated Interactive Milestone

The current x86_64 QEMU reference path has been exercised end to end with the unchanged CellLM-1M CWM1/Q8 checkpoint, persistent CellFS state, the POSIX-oriented command surface, the bootstrap C compiler, and the current C process runtime.

The deterministic and learned paths coexist in one interactive system:

```text
cell$ ls
home/  programs/  models/  system/  devices/

cell$ hello
Hello from C on Cell OS.

cell$ show memory
Memory: 511 MiB usable; ... MiB remain available to Cell OS.

cell$ ps
PID STATE EXIT GAS COMMAND
...
```

Natural-language requests fall through to the unchanged CellLM-1M model only after deterministic shell-command and executable lookup. Commands such as `ls`, `echo`, `cc`, `ps`, and installed program names do not require model inference. A request such as `show memory` is interpreted by CellLM and resolved through the deterministic `memory.status` capability.

Persistent CellFS state survives the normal `make clean` and image-rebuild workflow through `state/cellfs.img`. Program installation is idempotent, and compiler output stored in `/programs` remains part of the same executable namespace used by bundled programs.

The validated current image reports:

```text
# cortex-kernel: 49568 bytes
# pack-disk: stage2=33 sectors, kernel=97 sectors, cortex=2060 sectors@lba131, cellfs=1024 sectors@lba2191, size=1646080 bytes
#CORTEX x86 image ready: build/disk.img
#E0 s2 start
#E0 a20 ok
#E0 kernel load ok
#E0 prot32 copy ok
#E0 paging on
#E0 long ok
#E1 Cell OS Cortex kernel
#CELLFS READY sectors=1024 cwd=/
#CORTEX READY vocab=256 ctx=256 d=128 layers=5
#CAPABILITY dispatcher ready
#TASK executor ready
cell$
```

This validation establishes the current reference behavior. It does not claim complete POSIX conformance, concurrent process semantics, a mature filesystem, a full ISO C implementation, or portable PC hardware support beyond the tested reference backends.

## Native Bootstrap C Programming

The bootstrap C programming milestone establishes a C-to-CellExec path on both sides of the boot boundary. The same compiler core is exercised as a host tool (`build/cc`) and linked into the Cell OS shell as the normal `cc` command. Both paths emit verified CellExec-1 images.

The target-side path is now validated, not merely host-tested:

```text
cell$ echo '#include <stdio.h>' > /home/hello.c
cell$ echo 'int main(void) { puts("Hello from target cc"); return 0; }' >> /home/hello.c
cell$ cc /home/hello.c -o /programs/hello2
cell$ hello2
Hello from target cc
```

The compiler output is written through the normal VFS binary path, persists in CellFS, is admitted by the same CellExec verifier as bundled programs, and is executed by the same bounded task manager. Compilation is deterministic and does not involve CellLM.

This closes a significant loop:

```text
C source in CellFS
        |
        v
      cc
        |
        v
   CellExec-1
        |
        v
 /programs in CellFS
        |
        v
 verifier + task policy
        |
        v
     execution
```

The low-level `tools/cellasm.py` assembler remains a bootstrap, debugging, and backend-development tool. It is not the intended user programming language. The source-level direction is C, and the scripting direction is POSIX shell.

## C Process Runtime and Real Program Arguments

The process/runtime milestone turns the bootstrap compiler from a demonstration frontend into the beginning of a real process programming environment. Compiled programs can now receive arguments from the shell through a task-local process ABI using:

```c
int main(int argc, char **argv)
```

The shell builds the argument vector, CellExec passes `argc` and `argv` into the task, and compiled code can inspect argument strings through the supported character-pointer operations.

The validated target-side interaction is:

```text
cell$ echo '#include <stdio.h>' > /home/argv.c
cell$ echo 'int main(int argc, char **argv) { if (argc > 1) puts(argv[1]); return argc; }' >> /home/argv.c
cell$ cc /home/argv.c -o /programs/argv

cell$ argv hello
hello

cell$ argv test
test

cell$ ps
PID STATE EXIT GAS COMMAND
1 exited 2 13 /programs/argv
2 exited 2 13 /programs/argv
```

That transcript matters because the source is created inside Cell OS, compiled inside Cell OS, persisted by CellFS, executed through CellExec, receives real shell process input, returns an exit value, consumes a bounded gas budget, and leaves an inspectable process record.

The current C surface includes:

- `int main(void)` and `int main(int argc, char **argv)`;
- local integer variables and assignment;
- `char *` and `char **` access required for process arguments;
- `argv[i]` and character-pointer reads;
- integer `+`, `-`, `*`, `/`, and `%`;
- `==`, `!=`, `<`, `<=`, `>`, and `>=`;
- unary `-` and logical `!`;
- blocks, `if`/`else`, and `while`;
- expression-valued `return`;
- `puts` with dynamic strings;
- `putchar`;
- `strlen`;
- `strcmp`;
- `atoi`;
- explicit `cell_capability("name")` calls under the existing task capability policy.

The process/runtime regression includes explicit checks for the C parser/code generator, CellExec validation, capability inference, process ABI, character-pointer and argument indexing, libc-subset behavior, arithmetic/comparison semantics, shell argument forwarding, persistent program installation, remount execution, and the full Cortex session path.

The final validated freestanding build uses size-oriented optimization for the target kernel and produces:

```text
# cortex-kernel: 49568 bytes
```

This leaves more than 15 KiB of headroom below the current 65,024-byte conservative BIOS staging limit and avoids solving compiler/runtime growth by weakening the boot boundary.

## Native C System Interface

The native programming environment now has a process-facing system interface instead of requiring each filesystem or output operation to become a compiler-specific builtin. Compiled CellExec programs can use conventional C names for file I/O and basic process runtime services:

```c
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

int main(void) {
    int fd;
    char *buf;
    int n;

    fd = open("/home/native.txt", O_CREAT | O_TRUNC | O_RDWR);
    if (fd < 0) return errno;

    if (write(fd, "cell io\n", 8) != 8) return errno;
    if (lseek(fd, 0, SEEK_SET) < 0) return errno;

    buf = malloc(16);
    if (buf == 0) return errno;

    n = read(fd, buf, 8);
    if (n > 0) write(STDOUT_FILENO, buf, n);

    free(buf);
    close(fd);
    return 0;
}
```

The interface is implemented as a verified CellExec syscall instruction backed by task-local kernel state. Each task receives a bounded descriptor table, independent file offsets, an `errno` value, a bounded heap, and a bounded function call stack. File operations continue through the Cell VFS rather than bypassing it, so persistent CellFS files and live virtual nodes share the same descriptor-facing read path.

The current descriptor contract includes:

- `STDIN_FILENO`, `STDOUT_FILENO`, and `STDERR_FILENO` as descriptors `0`, `1`, and `2`;
- `open` with `O_RDONLY`, `O_WRONLY`, `O_RDWR`, `O_CREAT`, `O_TRUNC`, and `O_APPEND`;
- `close`;
- `read`;
- `write`;
- `lseek` with `SEEK_SET`, `SEEK_CUR`, and `SEEK_END`;
- POSIX-style error values exposed through `errno`;
- bounded `malloc` and `free` using task-local CellExec memory.

Programmatic writes are currently restricted to `/home`. A compiled task cannot use the new descriptor ABI to overwrite `/programs`; attempts are rejected with `EACCES`. This keeps the writable data namespace separate from the executable installation boundary while the program-management model is still evolving.

The C compiler also grows only as required by this system interface. It now supports user-defined integer functions with up to two parameters, forward calls, a bounded call stack, recursion within that bound, and pointer-indexed byte stores for mutable buffers. These additions are sufficient for functions such as recursive integer helpers and for constructing or modifying memory returned by `malloc` without claiming complete C pointer semantics.

The regression path verifies the full chain:

```text
C source
  -> target-compatible cc frontend
  -> verified CellExec syscall instruction
  -> task-local fd/errno/heap/call state
  -> Cell VFS
  -> persistent CellFS or live VFS node
```

Host integration tests establish file creation, truncation, read/write offsets, seeking, standard descriptors, `errno`, bounded allocation, the `/home` write boundary, live VFS reads, user-defined functions, recursive bounded calls, and pointer-indexed byte stores. The shell regression additionally creates C source in `/home`, compiles it to `/programs`, executes it, and verifies that the resulting task writes a persistent file through `open`/`write`/`close`.

The current freestanding kernel containing this interface measures:

```text
# cortex-kernel: 59852 bytes
```

The conservative BIOS staging limit remains unchanged at 65,024 bytes.

## Why This Milestone Matters

Cell OS is still experimental, but the system has moved beyond a boot proof, a filesystem proof, an interpreter proof, or an AI demo considered separately. The current reference image combines all of them into one operating substrate.

A human can boot Cell OS, create a C source file, invoke `cc` from the Cell OS prompt, produce a persistent executable, run it with arguments, use conventional file-descriptor I/O from that C program, allocate bounded task-local memory, inspect its exit status and gas usage, and still use natural-language requests through a native transformer whose actions are constrained by deterministic capabilities.

There is deliberately no Linux userspace underneath this path. The target does not call GCC, Clang, Python, PyTorch, llama.cpp, GGML, or a host libc to compile and execute the program after boot. The compiler frontend, VFS, persistent filesystem, executable verifier, task runtime, capability dispatcher, transformer inference engine, and shell integration are Cell OS components.

The next major boundary is self-hosting. The current `cc` frontend is still part of the trusted target image, so it does not yet compile itself as an ordinary user program. The project now has a concrete route toward that result.

## What Is Not Yet Demonstrated

The current work remains deliberately narrower than the long-term architecture.

- CellLM-1M is not a general-purpose language model and is not an AGI system.
- Its training corpus is synthetic, small, and intentionally limited to a bounded set of English system-control expressions.
- The learned closed loop is intentionally single-action: CellLM routes a request once, and the deterministic substrate executes at most one capability.
- CellExec tasks are currently synchronous; there is no preemptive or concurrent task scheduler in the Cortex execution path yet.
- CellExec-1 intentionally does not admit arbitrary native x86 instructions from writable storage.
- CellFS-1 currently uses append-only data allocation and has no reclamation or journal.
- The C compiler remains a bootstrap subset rather than a complete ISO C implementation.
- General pointer semantics, arbitrary arrays, structs/unions, global objects, a general preprocessor, object files, a linker, dynamic libraries, environment variables, and a complete libc are not implemented yet. User-defined integer functions currently support at most two parameters and a bounded call depth.
- The supported `char *` and `char **` surface is currently driven by the process/runtime needs of `argc/argv`; it should not be interpreted as complete C pointer semantics.
- File descriptors are intentionally bounded to eight per task in the current implementation.
- `stdin` currently has EOF-only semantics; interactive task input and blocking descriptor I/O are not implemented yet.
- The current allocator is a bounded task-local allocator with eight tracked blocks, not a general heap with coalescing or virtual memory.
- File writes from ordinary compiled tasks are currently restricted to `/home`; a broader permissions and executable-installation model is future work.
- The target `cc` currently resides in the trusted kernel/shell image rather than running as an ordinary CellExec process.
- The compiler does not yet compile its own source inside Cell OS, so the system is not self-hosting.
- Synthetic `/proc`, `/dev`, and `/sys` namespaces are planned but not yet implemented.
- Cortex does not yet provide verified Ethernet, GPU, NVMe, USB, display, or power backends; those registered capabilities currently report `unsupported`.
- The present x86 hardware path is validated primarily through the QEMU/PC BIOS reference environment with legacy ATA model loading and the reference serial-console backend.
- Real-hardware compatibility across heterogeneous PCs has not yet been established.
- The current inference engine is optimized for simplicity and inspectability rather than maximum throughput.
- No learned component is allowed to synthesize arbitrary privileged machine operations at runtime.
- The existing Cell proof ledger and Cortex semantic validation are related but are not yet a single unified formal proof system.

These are explicit engineering boundaries, not hidden assumptions.

## Roadmap

The next milestones continue from the working native C system interface toward synthetic Unix namespaces, a broader compiler, and eventually a self-hosting C environment without abandoning Cell OS verification and capability boundaries.


### Cell-Native Synthetic Unix Namespaces

Add familiar synthetic namespaces:

```text
/proc
/dev
/sys
```

These will be Cell-native VFS projections, not Linux ABI emulation. Familiar names are reused because the concepts are familiar; the implementation and exact data model remain Cell OS mechanisms.

The descriptor API now provides the user-program access path for these namespaces. A compiled C utility should be able to inspect them through ordinary `open`/`read`/`close` operations rather than through new compiler intrinsics.

A likely consequence is that utilities such as `ps` can eventually move out of the trusted shell path and become ordinary C programs that read process state through `/proc`. Device and system inspection can follow the same direction through `/dev` and `/sys`.

### Compiler and Language Expansion

Grow the C compiler according to the needs of real Cell OS programs and the compiler itself:

- general user-defined functions;
- globals;
- arrays;
- broader pointer arithmetic;
- structs;
- more complete declarations and expressions;
- additional preprocessing support;
- multiple translation units or an equivalent explicit linking model where justified.

The rule is demand-driven growth. Features should be added because Cell OS programs or the compiler require them, not to accumulate syntax without a system use.

### Compiler Bootstrap Inside Cell OS

Move the compiler toward ordinary program execution and perform the first bootstrap:

```text
cc source
   |
   v
host/bootstrap cc
   |
   v
/programs/cc.stage1
   |
   v
cc.stage1 running inside Cell OS
   |
   v
/programs/cc.stage2
```

At this point the compiler source itself must be expressible in the C subset supported by Cell OS.

### Self-Hosting Proof

Establish that the compiler can reproduce itself through the Cell OS execution environment.

The strongest target is deterministic equivalence:

```text
cc.c -> cc.stage1
cc.c -> cc.stage2 using cc.stage1

sha256(cc.stage1) == sha256(cc.stage2)
```

If byte-for-byte identity is not immediately achievable because of explicit build metadata or layout decisions, the intermediate proof target is semantic equivalence under a pinned test corpus and executable verifier. The long-term goal remains deterministic self-reproduction.

Beyond these stages, the existing hardware, scheduler, observability, Cortex, and proof work continues. Model growth is not the immediate priority. The near-term priority is making the operating substrate increasingly capable of building, inspecting, and reproducing itself.

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
