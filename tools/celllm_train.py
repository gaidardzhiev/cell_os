#!/usr/bin/env python3

import argparse
import json
import math
import random
import struct
import zlib
from pathlib import Path

import torch
import torch.nn as nn
import torch.nn.functional as F


MAGIC = 0x314D5743
VERSION = 1
QUANT_Q8 = 1


class RMSNorm(nn.Module):
    def __init__(self, d):
        super().__init__()
        self.weight = nn.Parameter(torch.ones(d))

    def forward(self, x):
        return (
            x
            * torch.rsqrt(x.pow(2).mean(-1, keepdim=True) + 1.0e-5)
            * self.weight
        )


class Block(nn.Module):
    def __init__(self, d, heads, d_ff):
        super().__init__()

        assert d % heads == 0

        self.d = d
        self.heads = heads
        self.head_dim = d // heads

        self.n1 = RMSNorm(d)

        self.q = nn.Linear(d, d, bias=False)
        self.k = nn.Linear(d, d, bias=False)
        self.v = nn.Linear(d, d, bias=False)
        self.o = nn.Linear(d, d, bias=False)

        self.n2 = RMSNorm(d)

        self.w1 = nn.Linear(d, d_ff, bias=False)
        self.w2 = nn.Linear(d_ff, d, bias=False)

    def forward(self, x):
        b, t, d = x.shape

        z = self.n1(x)

        q = self.q(z).view(
            b, t, self.heads, self.head_dim
        ).transpose(1, 2)

        k = self.k(z).view(
            b, t, self.heads, self.head_dim
        ).transpose(1, 2)

        v = self.v(z).view(
            b, t, self.heads, self.head_dim
        ).transpose(1, 2)

        a = F.scaled_dot_product_attention(
            q,
            k,
            v,
            is_causal=True,
        )

        a = (
            a.transpose(1, 2)
            .contiguous()
            .view(b, t, d)
        )

        x = x + self.o(a)

        z = self.n2(x)

        x = x + self.w2(
            F.relu(self.w1(z))
        )

        return x


class CellLM(nn.Module):
    def __init__(
        self,
        vocab=256,
        ctx=256,
        d=128,
        layers=5,
        heads=4,
        d_ff=512,
    ):
        super().__init__()

        self.vocab = vocab
        self.ctx = ctx
        self.d = d
        self.layers_n = layers
        self.heads = heads
        self.d_ff = d_ff

        self.tok = nn.Embedding(vocab, d)
        self.pos = nn.Embedding(ctx, d)

        self.blocks = nn.ModuleList(
            [
                Block(d, heads, d_ff)
                for _ in range(layers)
            ]
        )

        self.final = RMSNorm(d)

    def forward(self, idx):
        _, t = idx.shape

        if t > self.ctx:
            raise ValueError(
                f"context {t} exceeds {self.ctx}"
            )

        pos = torch.arange(
            t,
            device=idx.device,
        )

        x = (
            self.tok(idx)
            + self.pos(pos)[None, :, :]
        )

        for block in self.blocks:
            x = block(x)

        x = self.final(x)

        # Tied output projection.
        return x @ self.tok.weight.t()


CAPABILITY_PROMPTS = {
    "system.status": [
        "status",
        "show status",
        "show system status",
        "check system status",
        "how is the system",
        "is the system ready",
        "tell me the system status",
        "inspect the system",
        "check the computer",
        "show computer status",
    ],

    "cpu.info": [
        "show cpu",
        "show cpu information",
        "show processor information",
        "what cpu do i have",
        "what processor do i have",
        "tell me about the cpu",
        "inspect the cpu",
        "check the processor",
        "show processor",
        "what is the cpu",
        "identify the cpu",
        "show cpu capabilities",
    ],

    "memory.status": [
        "show memory",
        "show memory status",
        "show ram",
        "show ram status",
        "how much memory do i have",
        "how much ram do i have",
        "how much memory is free",
        "how much ram is free",
        "check memory",
        "check ram",
        "inspect memory",
        "tell me about memory",
        "show available memory",
        "show free memory",
    ],

    "storage.list": [
        "show storage",
        "show storage devices",
        "list storage devices",
        "list disks",
        "show disks",
        "what disks are available",
        "what storage is available",
        "inspect storage",
        "check storage",
        "show drives",
        "list drives",
        "what drives do i have",
    ],

    "storage.list_dir": [
        "ls",
        "list files",
        "show files",
        "list directory",
        "show directory",
        "show directory contents",
        "what files are here",
        "show files here",
        "list current directory",
    ],

    "storage.pwd": [
        "pwd",
        "where am i",
        "show current directory",
        "what is the current directory",
        "show working directory",
        "where in storage am i",
    ],

    "network.list": [
        "show network",
        "show network devices",
        "list network devices",
        "show network interfaces",
        "list network interfaces",
        "what network hardware is available",
        "inspect network",
        "check network hardware",
        "show ethernet devices",
        "list ethernet devices",
    ],

    "gpu.info": [
        "show gpu",
        "show gpu information",
        "what gpu do i have",
        "identify the gpu",
        "show graphics hardware",
        "inspect the gpu",
        "check the gpu",
        "show compute hardware",
        "what graphics card do i have",
        "show graphics card",
    ],

    "usb.list": [
        "show usb",
        "show usb devices",
        "list usb devices",
        "what usb devices are connected",
        "inspect usb",
        "check usb devices",
        "show connected usb devices",
    ],

    "display.info": [
        "show display",
        "show display information",
        "inspect display",
        "what display is available",
        "show graphics output",
        "show screen information",
        "check display",
    ],

    "power.status": [
        "show power status",
        "check power",
        "show power information",
        "inspect power",
        "what is the power status",
        "show energy status",
    ],
}


DIRECT_PROMPTS = {
    "Hello. I am Cell Cortex.": [
        "hello",
        "hi",
        "hey",
        "hello cortex",
        "hi cortex",
        "good morning",
        "good afternoon",
        "good evening",
    ],

    "I am Cell Cortex, the personal intelligence of Cell OS.": [
        "who are you",
        "what are you",
        "identify yourself",
        "tell me who you are",
        "what is your name",
        "are you cell cortex",
    ],

    "I can understand natural language and control Cell OS through verified capabilities.": [
        "help",
        "what can you do",
        "show help",
        "how can you help me",
        "what are your capabilities",
        "how do i control the computer",
    ],
}


SAFETY_PROMPTS = [
    "write directly to an unknown hardware register",
    "run an unknown driver",
    "use an unsupported device",
    "ignore hardware safety",
    "execute arbitrary machine code",
    "bypass capability checks",
    "disable all safety checks",
]


def add_prompt_variants(out, prompt, target):
    variants = {
        prompt,
        prompt.capitalize(),
        prompt + "?",
        prompt.capitalize() + "?",
        "please " + prompt,
        "Please " + prompt,
    }

    for p in variants:
        out.append({
            "input": f"cell> {p}",
            "target": target,
        })


def build_examples(seed):
    examples = []

    for capability, prompts in CAPABILITY_PROMPTS.items():
        target = f"<CALL {capability}>"

        for prompt in prompts:
            add_prompt_variants(
                examples,
                prompt,
                target,
            )

    for target, prompts in DIRECT_PROMPTS.items():
        for prompt in prompts:
            add_prompt_variants(
                examples,
                prompt,
                target,
            )

    for prompt in SAFETY_PROMPTS:
        add_prompt_variants(
            examples,
            prompt,
            "I cannot perform that action without a verified Cell capability.",
        )

    rng = random.Random(seed)
    rng.shuffle(examples)

    return examples


def build_corpus(examples, repeat, seed):
    rng = random.Random(seed)

    preamble = """
CellLM is the natural-language control cortex of Cell OS.
CellLM communicates with hardware only through verified Cell capabilities.
A hardware operation is requested with one canonical CALL line.
CellLM never invents a hardware capability.
CellLM never writes arbitrary hardware registers.
CellLM never bypasses capability validation.

Known Cell capabilities:
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

"""

    chunks = [preamble]

    working = examples[:]

    for _ in range(repeat):
        rng.shuffle(working)

        for ex in working:
            chunks.append(
                ex["input"]
                + "\n"
                + ex["target"]
                + "\n\n"
            )

    corpus = "".join(chunks)

    return corpus.encode("ascii")


def write_dataset(path, examples):
    p = Path(path)
    p.parent.mkdir(
        parents=True,
        exist_ok=True,
    )

    with p.open(
        "w",
        encoding="utf-8",
    ) as f:
        for ex in examples:
            f.write(
                json.dumps(
                    ex,
                    ensure_ascii=True,
                )
                + "\n"
            )


def parameter_count(model):
    return sum(
        p.numel()
        for p in model.parameters()
    )


def q8_blob(t):
    a = (
        t.detach()
        .cpu()
        .float()
        .contiguous()
    )

    mx = float(
        a.abs().max()
    )

    scale = (
        mx / 127.0
        if mx
        else 1.0
    )

    q = torch.clamp(
        torch.round(a / scale),
        -127,
        127,
    ).to(torch.int8)

    return (
        struct.pack(
            "<f",
            scale,
        )
        + q.numpy().tobytes()
    )


def f32_blob(t):
    return (
        t.detach()
        .cpu()
        .float()
        .contiguous()
        .numpy()
        .tobytes()
    )


def export_cwm(model, output):
    payload = bytearray()

    payload += q8_blob(
        model.tok.weight
    )

    payload += q8_blob(
        model.pos.weight
    )

    for block in model.blocks:
        payload += f32_blob(
            block.n1.weight
        )

        payload += q8_blob(
            block.q.weight
        )

        payload += q8_blob(
            block.k.weight
        )

        payload += q8_blob(
            block.v.weight
        )

        payload += q8_blob(
            block.o.weight
        )

        payload += f32_blob(
            block.n2.weight
        )

        payload += q8_blob(
            block.w1.weight
        )

        payload += q8_blob(
            block.w2.weight
        )

    payload += f32_blob(
        model.final.weight
    )

    header_bytes = 64
    total = (
        header_bytes
        + len(payload)
    )

    crc = (
        zlib.crc32(payload)
        & 0xFFFFFFFF
    )

    header = struct.pack(
        "<IHHIIIIIIIQQIII",
        MAGIC,
        VERSION,
        header_bytes,
        QUANT_Q8,
        model.vocab,
        model.ctx,
        model.d,
        model.layers_n,
        model.heads,
        model.d_ff,
        len(payload),
        total,
        crc,
        0,
        0,
    )

    assert len(header) == 64

    p = Path(output)

    p.parent.mkdir(
        parents=True,
        exist_ok=True,
    )

    p.write_bytes(
        header + payload
    )

    return total, crc


def verify_cwm(path):
    raw = Path(path).read_bytes()

    if len(raw) < 64:
        raise RuntimeError(
            "CWM file is too small"
        )

    h = struct.unpack(
        "<IHHIIIIIIIQQIII",
        raw[:64],
    )

    (
        magic,
        version,
        header_bytes,
        quant,
        vocab,
        ctx,
        d,
        layers,
        heads,
        d_ff,
        weights_bytes,
        total_bytes,
        crc,
        _,
        _,
    ) = h

    if magic != MAGIC:
        raise RuntimeError(
            "bad CWM magic"
        )

    if version != VERSION:
        raise RuntimeError(
            "bad CWM version"
        )

    if header_bytes != 64:
        raise RuntimeError(
            "bad CWM header size"
        )

    if quant != QUANT_Q8:
        raise RuntimeError(
            "unsupported quantization"
        )

    if total_bytes != len(raw):
        raise RuntimeError(
            "CWM total size mismatch"
        )

    if weights_bytes != len(raw) - 64:
        raise RuntimeError(
            "CWM weights size mismatch"
        )

    actual_crc = (
        zlib.crc32(raw[64:])
        & 0xFFFFFFFF
    )

    if actual_crc != crc:
        raise RuntimeError(
            "CWM CRC mismatch"
        )

    print(
        "#CWM ABI PASS",
        f"bytes={len(raw)}",
        f"vocab={vocab}",
        f"ctx={ctx}",
        f"d={d}",
        f"layers={layers}",
        f"heads={heads}",
        f"ff={d_ff}",
        f"crc={crc:08x}",
    )


@torch.no_grad()
def generate(
    model,
    prompt,
    device,
    max_new=96,
):
    model.eval()

    seq = list(
        prompt.encode("ascii")
    )

    out = []

    for _ in range(max_new):
        window = seq[-model.ctx:]

        x = torch.tensor(
            window,
            dtype=torch.long,
            device=device,
        )[None, :]

        logits = model(x)

        token = int(
            torch.argmax(
                logits[0, -1]
            ).item()
        )

        seq.append(token)
        out.append(token)

        if token == 10 and len(out) > 1:
            break

    return bytes(out).decode(
        "ascii",
        errors="replace",
    ).strip()


def lr_for_step(
    step,
    total,
    base_lr,
):
    warmup = max(
        50,
        total // 20,
    )

    if step < warmup:
        return (
            base_lr
            * (step + 1)
            / warmup
        )

    progress = (
        (step - warmup)
        / max(
            1,
            total - warmup,
        )
    )

    return (
        base_lr
        * 0.5
        * (
            1.0
            + math.cos(
                math.pi * progress
            )
        )
    )


def train(args):
    random.seed(args.seed)
    torch.manual_seed(args.seed)

    if torch.cuda.is_available():
        torch.cuda.manual_seed_all(
            args.seed
        )

    if args.device == "auto":
        device = torch.device(
            "cuda"
            if torch.cuda.is_available()
            else "cpu"
        )
    else:
        device = torch.device(
            args.device
        )

    if device.type == "cuda":
        torch.backends.cuda.matmul.allow_tf32 = True

    examples = build_examples(
        args.seed
    )

    write_dataset(
        args.dataset,
        examples,
    )

    corpus = build_corpus(
        examples,
        args.repeat,
        args.seed,
    )

    model = CellLM(
        vocab=256,
        ctx=256,
        d=128,
        layers=5,
        heads=4,
        d_ff=512,
    ).to(device)

    params = parameter_count(
        model
    )

    print(
        "#CELLLM device:",
        device,
    )

    if device.type == "cuda":
        print(
            "#CELLLM gpu:",
            torch.cuda.get_device_name(0),
        )

    print(
        "#CELLLM parameters:",
        params,
    )

    print(
        "#CELLLM corpus bytes:",
        len(corpus),
    )

    print(
        "#CELLLM examples:",
        len(examples),
    )

    if params != 1049984:
        raise RuntimeError(
            f"unexpected parameter count: {params}"
        )

    data = torch.tensor(
        list(corpus),
        dtype=torch.long,
        device=device,
    )

    if len(data) <= model.ctx + 1:
        raise RuntimeError(
            "training corpus is too small"
        )

    opt = torch.optim.AdamW(
        model.parameters(),
        lr=args.lr,
        betas=(0.9, 0.95),
        weight_decay=0.01,
    )

    offsets = torch.arange(
        model.ctx + 1,
        device=device,
    )

    rng = torch.Generator(
        device=device
    )

    rng.manual_seed(
        args.seed
    )

    model.train()

    for step in range(args.steps):
        starts = torch.randint(
            0,
            len(data) - model.ctx - 1,
            (args.batch,),
            generator=rng,
            device=device,
        )

        seq = data[
            starts[:, None]
            + offsets[None, :]
        ]

        x = seq[:, :-1]
        y = seq[:, 1:]

        lr = lr_for_step(
            step,
            args.steps,
            args.lr,
        )

        for group in opt.param_groups:
            group["lr"] = lr

        logits = model(x)

        loss = F.cross_entropy(
            logits.reshape(
                -1,
                model.vocab,
            ),
            y.reshape(-1),
        )

        opt.zero_grad(
            set_to_none=True
        )

        loss.backward()

        torch.nn.utils.clip_grad_norm_(
            model.parameters(),
            1.0,
        )

        opt.step()

        if (
            step % args.log_every == 0
            or step == args.steps - 1
        ):
            print(
                f"step={step:5d}"
                f" loss={loss.item():.6f}"
                f" lr={lr:.6g}"
            )

    ckpt = Path(
        args.checkpoint
    )

    ckpt.parent.mkdir(
        parents=True,
        exist_ok=True,
    )

    torch.save(
        {
            "architecture": "CellLM-CWM1",
            "language": "English",
            "config": {
                "vocab": 256,
                "ctx": 256,
                "d": 128,
                "layers": 5,
                "heads": 4,
                "d_ff": 512,
            },
            "parameters": params,
            "state_dict": model.state_dict(),
        },
        ckpt,
    )

    print(
        "#CELLLM checkpoint:",
        ckpt,
    )

    print(
        "\n#CELLLM PyTorch evaluation"
    )

    tests = [
        "cell> hello\n",
        "cell> who are you\n",
        "cell> show memory\n",
        "cell> what cpu do i have\n",
        "cell> list disks\n",
        "cell> ls\n",
        "cell> show gpu\n",
        "cell> list network interfaces\n",
        "cell> list usb devices\n",
    ]

    for prompt in tests:
        answer = generate(
            model,
            prompt,
            device,
        )

        print(
            repr(prompt.strip()),
            "=>",
            repr(answer),
        )

    total, crc = export_cwm(
        model,
        args.output,
    )

    print(
        "#CELLLM CWM:",
        args.output,
    )

    print(
        "#CELLLM CWM bytes:",
        total,
    )

    print(
        "#CELLLM CWM CRC:",
        f"{crc:08x}",
    )

    verify_cwm(
        args.output
    )

    info_path = Path(
        args.info
    )

    info_path.parent.mkdir(
        parents=True,
        exist_ok=True,
    )

    info_path.write_text(
        json.dumps(
            {
                "name": "CellLM-1M",
                "language": "English",
                "architecture": "CWM1",
                "parameters": params,
                "vocab": 256,
                "context": 256,
                "d_model": 128,
                "layers": 5,
                "heads": 4,
                "d_ff": 512,
                "quantization": "Q8",
                "cwm_bytes": total,
                "cwm_crc32": f"{crc:08x}",
                "capabilities": sorted(
                    CAPABILITY_PROMPTS.keys()
                ),
            },
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )

    print(
        "#CELLLM info:",
        info_path,
    )


def main():
    ap = argparse.ArgumentParser()

    ap.add_argument(
        "--output",
        default="models/celllm_1m.cwm",
    )

    ap.add_argument(
        "--checkpoint",
        default="checkpoints/celllm_1m.pt",
    )

    ap.add_argument(
        "--dataset",
        default="data/celllm_en_1m.jsonl",
    )

    ap.add_argument(
        "--info",
        default="models/celllm_1m.json",
    )

    ap.add_argument(
        "--steps",
        type=int,
        default=3500,
    )

    ap.add_argument(
        "--batch",
        type=int,
        default=64,
    )

    ap.add_argument(
        "--repeat",
        type=int,
        default=200,
    )

    ap.add_argument(
        "--lr",
        type=float,
        default=0.002,
    )

    ap.add_argument(
        "--seed",
        type=int,
        default=17,
    )

    ap.add_argument(
        "--log-every",
        type=int,
        default=100,
    )

    ap.add_argument(
        "--device",
        default="auto",
    )

    args = ap.parse_args()

    train(args)


if __name__ == "__main__":
    main()
