from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
SPEC = ROOT / "docs" / "spec" / "av2-v1.0.0"
OUTPUT = ROOT / "src" / "decoder" / "coeff_cdf_all.inc"


def values(name):
    text = (SPEC / name).read_text(encoding="ascii")
    return [int(value) for value in re.findall(r"-?\d+", text[text.index("=") :])]


def initialized(row, symbols):
    parameter = row[-2]
    adjustments = (0, -1, -2, 1, 1)
    rate = 2 if symbols < 4 else 3
    return [32768 - value for value in row[: symbols - 1]] + [
        0,
        0,
        rate + adjustments[parameter // 25],
        rate + 1 + adjustments[(parameter // 5) % 5],
        rate + 2 + adjustments[parameter % 5],
    ]


def rows(source, dimensions, input_words, symbols, tcq=False):
    expected = input_words
    for dimension in dimensions:
        expected *= dimension
    if tcq:
        expected *= 2
    if len(source) != expected:
        raise ValueError(f"expected {expected} values, found {len(source)}")
    output = []
    step = input_words * (2 if tcq else 1)
    for offset in range(0, len(source), step):
        output.append(initialized(source[offset : offset + input_words], symbols))
    return output


def emit(name, dimensions, data):
    words = len(data[0])
    declaration = "".join(f"[{dimension}U]" for dimension in dimensions)
    lines = [f"static const uint16_t {name}{declaration}[{words}U] = {{"]
    index = 0

    def level(depth):
        nonlocal index
        indent = "    " * (depth + 1)
        lines.append(indent + "{")
        if depth + 1 == len(dimensions):
            for item in range(dimensions[depth]):
                suffix = "," if item + 1 != dimensions[depth] else ""
                row = ", ".join(f"{value}U" for value in data[index])
                lines.append(indent + "    { " + row + " }" + suffix)
                index += 1
        else:
            for item in range(dimensions[depth]):
                level(depth + 1)
                if item + 1 != dimensions[depth]:
                    lines[-1] += ","
        lines.append(indent + "}")

    for item in range(dimensions[0]):
        level(1) if len(dimensions) > 1 else None
        if item + 1 != dimensions[0]:
            lines[-1] += ","
    lines.append("};")
    return lines


tables = [
    ("av2h_coeff_eob_base", "default_coeff_base_eob_cdf.h", (4, 5, 4), 4, 3, False),
    ("av2h_coeff_eob_base_lf", "default_coeff_base_lf_eob_cdf.h", (4, 5, 4), 6, 5, False),
    ("av2h_coeff_base", "default_coeff_base_cdf.h", (4, 5, 20), 5, 4, True),
    ("av2h_coeff_base_lf", "default_coeff_base_lf_cdf.h", (4, 5, 33), 7, 6, True),
    ("av2h_coeff_br", "default_coeff_br_cdf.h", (4, 7), 5, 4, False),
    ("av2h_coeff_br_lf", "default_coeff_br_lf_cdf.h", (4, 14), 5, 4, False),
    ("av2h_coeff_eob_base_uv", "default_coeff_base_eob_uv_cdf.h", (4, 4), 4, 3, False),
    ("av2h_coeff_eob_base_lf_uv", "default_coeff_base_lf_eob_uv_cdf.h", (4, 4), 6, 5, False),
    ("av2h_coeff_base_uv", "default_coeff_base_uv_cdf.h", (4, 12), 5, 4, False),
    ("av2h_coeff_base_lf_uv", "default_coeff_base_lf_uv_cdf.h", (4, 12), 7, 6, False),
    ("av2h_coeff_br_uv", "default_coeff_br_uv_cdf.h", (4, 4), 5, 4, False),
]

output = ["/* Generated from docs/spec/av2-v1.0.0 coefficient CDF tables. */", ""]
for table, filename, dimensions, input_words, symbols, tcq in tables:
    output.extend(emit(table, dimensions, rows(
        values(filename), dimensions, input_words, symbols, tcq)))
    output.append("")
OUTPUT.write_text("\n".join(output), encoding="ascii", newline="\n")