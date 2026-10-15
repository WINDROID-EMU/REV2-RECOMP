#!/usr/bin/env python3
import re, glob, os

PAT = re.compile(r'DEFINE_REX_FUNC\((sub_83(?:4[EF]|5[01])[0-9A-F]{4})\) \{')

def add_trace(m):
    return m.group(0) + ' REV2_TRACE(0x' + m.group(1)[4:] + ');'

script_dir = os.path.dirname(os.path.abspath(__file__))
project_dir = os.path.dirname(script_dir)
gen_pattern = os.path.join(project_dir, 'generated', 'default', 'RERev2_recomp.*.cpp')

patched_files = 0
total_matches = 0

for path in sorted(glob.glob(gen_pattern)):
    with open(path, 'r', encoding='utf-8') as f:
        src = f.read()
    
    # Se já foi patcheado, pula
    if 'REV2_TRACE(0x83' in src:
        continue

    matches = len(PAT.findall(src))
    if matches > 0:
        out = PAT.sub(add_trace, src)
        with open(path, 'w', encoding='utf-8') as f:
            f.write(out)
        patched_files += 1
        total_matches += matches
        print(f"Patcheado {os.path.basename(path)}: {matches} funcoes D3D instrumentadas")

print(f"Total: {total_matches} funcoes instrumentadas em {patched_files} arquivos.")
