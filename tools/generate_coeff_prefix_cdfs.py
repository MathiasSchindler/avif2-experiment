from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "docs" / "spec" / "av2-v1.0.0" / "default_txb_skip_cdf.h"
OUTPUT = ROOT / "src" / "decoder" / "coeff_prefix_cdf_all.inc"

Q_CONTEXTS = 4
INTER_CONTEXTS = 2
TX_CONTEXTS = 5
SKIP_CONTEXTS = 10
INPUT_WORDS = 3


def initialized(row):
    parameter = row[1]
    adjustments = (0, -1, -2, 1, 1)
    return [
        32768 - row[0],
        0,
        row[2],
        2 + adjustments[parameter // 25],
        3 + adjustments[(parameter // 5) % 5],
        4 + adjustments[parameter % 5],
    ]


text = SOURCE.read_text(encoding="ascii")
values = [int(value) for value in re.findall(r"-?\d+", text[text.index("=") :])]
expected = Q_CONTEXTS * INTER_CONTEXTS * TX_CONTEXTS * SKIP_CONTEXTS * INPUT_WORDS
if len(values) != expected:
    raise ValueError(f"expected {expected} values, found {len(values)}")

lines = [
    "/* Generated from docs/spec/av2-v1.0.0/default_txb_skip_cdf.h. */",
    "",
    "static const uint16_t av2h_luma_txb_skip[4U][5U][10U][6U] = {",
]
offset = 0
for q_context in range(Q_CONTEXTS):
    lines.append("    {")
    for inter_context in range(INTER_CONTEXTS):
        for tx_context in range(TX_CONTEXTS):
            if inter_context == 0:
                lines.append("        {")
            for skip_context in range(SKIP_CONTEXTS):
                row = values[offset : offset + INPUT_WORDS]
                offset += INPUT_WORDS
                if inter_context != 0:
                    continue
                suffix = "," if skip_context + 1 != SKIP_CONTEXTS else ""
                words = ", ".join(f"{value}U" for value in initialized(row))
                lines.append(f"            {{ {words} }}{suffix}")
            if inter_context == 0:
                suffix = "," if tx_context + 1 != TX_CONTEXTS else ""
                lines.append(f"        }}{suffix}")
    suffix = "," if q_context + 1 != Q_CONTEXTS else ""
    lines.append(f"    }}{suffix}")
lines.append("};")
lines.append("")
OUTPUT.write_text("\n".join(lines), encoding="ascii", newline="\n")