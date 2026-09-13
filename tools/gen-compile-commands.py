#!/usr/bin/env python3
"""Escreve compile_commands.json a partir do que o make FARIA.

O clangd (o language server do editor) nao le Makefile. Sem este arquivo ele
chuta as flags, nao acha -I nem -D, e marca include valido como erro.
"""
import json, os, shlex, subprocess, sys

root = os.getcwd()
dry = subprocess.run(["make", "--always-make", "--dry-run"],
                     capture_output=True, text=True).stdout

entries = []
for line in dry.splitlines():
    line = line.strip()
    if not line.startswith("cc ") or " -c " not in line:
        continue
    parts = shlex.split(line)
    src = parts[parts.index("-c") + 1]
    entries.append({"directory": root, "command": line,
                    "file": os.path.join(root, src)})

# o alvo de teste compila tudo numa chamada so, sem -c, entao nao aparece acima.
# as flags sao as mesmas mais -Isrc.
base = entries[0]["command"].split(" -c ")[0] if entries else "cc -std=c11"
for name in sorted(os.listdir("test")):
    if not name.endswith(".c"):
        continue
    path = os.path.join(root, "test", name)
    entries.append({
        "directory": root,
        "command": f"{base} -Isrc -c test/{name} -o /dev/null",
        "file": path,
    })

json.dump(entries, open("compile_commands.json", "w"), indent=2)
print(f"compile_commands.json: {len(entries)} entradas")
