import re
import os

filepath = r"d:\Games\OpenMidway\hw\xbox\mcpx\apu\dsp\dsp_cpu.c"
with open(filepath, 'r') as f:
    text = f.read()

arr_pattern = re.compile(r'static const OpcodeEntry nonparallel_opcodes\[\] = \{\s*(.*?)\s*\};', re.DOTALL)
match = arr_pattern.search(text)
if not match:
    print("Could not find nonparallel_opcodes")
    exit(1)

arr_body = match.group(1)
entries = []
for line in arr_body.split('\n'):
    line = line.strip()
    if not line or line.startswith('//'):
        continue
    m = re.match(r'\{\s*("(?:[^"]|\\")*")\s*,\s*("(?:[^"]|\\")*")\s*,(.*)\},?(.*)', line)
    if m:
        template, name, rest, comment = m.groups()
        parts = rest.split(',')
        dis_f = parts[0].strip()
        emu_f = parts[1].strip() if len(parts) > 1 else "NULL"
        match_f = parts[2].strip() if len(parts) > 2 else ""
        entries.append((template, name, dis_f, emu_f, match_f, comment, line))
    else:
        print("Failed to parse line:", line)

enum_name = "DspOpcodeID"
enum_lines = ["typedef enum {"]
op_map = {}
for i, entry in enumerate(entries):
    template, name, dis_f, emu_f, match_f, comment, orig_line = entry
    safe_name = name.strip('"').replace(',', '').replace(' ', '_').replace('[', '').replace(']', '').replace('+', 'plus').replace('-', 'minus').replace(':', '_').replace('<->', 'swap').replace('#', 'hash_').upper()
    safe_name = "OP_" + safe_name + f"_{i}"
    enum_lines.append(f"    {safe_name},")
    op_map[i] = safe_name
enum_lines.append("    OP_UNDEFINED")
enum_lines.append("} DspOpcodeID;")
enum_text = "\n".join(enum_lines)

struct_pattern = re.compile(r'typedef struct OpcodeEntry \{(.*?)\} OpcodeEntry;', re.DOTALL)
new_struct = """typedef struct OpcodeEntry {
    DspOpcodeID id;
    const char* template;
    const char* name;
    dis_func_t dis_func;
    match_func_t match_func;
} OpcodeEntry;"""
text = struct_pattern.sub(enum_text + "\n\n" + new_struct, text, count=1)

new_arr_lines = ["static const OpcodeEntry nonparallel_opcodes[] = {"]
for i, entry in enumerate(entries):
    template, name, dis_f, emu_f, match_f, comment, orig_line = entry
    safe_name = op_map[i]
    m_f = f", {match_f}" if match_f else ""
    c_m = f" {comment}" if comment.strip() else ""
    new_arr_lines.append(f"    {{ {safe_name}, {template}, {name}, {dis_f}{m_f} }},{c_m}")
new_arr_lines.append("};")
text = text.replace(match.group(0), "\n".join(new_arr_lines))

exec_pattern = re.compile(r'        if \(op->emu_func\) \{(.*?)\} else \{(.*?)\}', re.DOTALL)
exec_match = exec_pattern.search(text)
switch_lines = ["        switch (op->id) {"]
for i, entry in enumerate(entries):
    template, name, dis_f, emu_f, match_f, comment, orig_line = entry
    if emu_f != "NULL":
        switch_lines.append(f"            case {op_map[i]}: {emu_f}(dsp); break;")
switch_lines.append("""            default:
                DPRINTF("%x - %s\\n", dsp->cur_inst, op->name);
                emu_undefined(dsp);
                break;
        }""")
new_exec = "\n".join(switch_lines)
text = text.replace(exec_match.group(0), new_exec)

with open(filepath, 'w') as f:
    f.write(text)

print("Refactoring complete.")
