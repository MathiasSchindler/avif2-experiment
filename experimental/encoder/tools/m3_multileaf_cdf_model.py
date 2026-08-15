#!/usr/bin/env python3
"""Exact adaptive transform/coefficient transcript model for M3 multi-leaf frames."""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import re
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
TOOLS = Path(__file__).resolve().parent
if str(TOOLS) not in sys.path:
    sys.path.insert(0, str(TOOLS))
NORMATIVE = ROOT / "experimental/encoder/src/normative_cdfs.inc"
DEFAULT_AVMENC = ROOT / "build/reference/avm/avmenc"
DEFAULT_AVMDEC = ROOT / "build/reference/avm/avmdec"
DEFAULT_PROJECT_DECODER = ROOT / "build/host-cmake/av2heif-decoder"
CASES = ((17, 17), (17, 32), (32, 17), (32, 32), (48, 64), (64, 64))
Q_CONTEXT_CASES = (90, 91, 141, 191)


def _load(name: str, filename: str):
    spec = importlib.util.spec_from_file_location(name, TOOLS / filename)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


DC = _load("m3_multileaf_dc", "m3_dc_model.py")
PARTITIONS = _load("m3_multileaf_partitions", "m3_large_partition_model.py")


TABLE_SYMBOLS = {
    "tx_do_partition": 2,
    "tx_partition_type": 7,
    "txb_skip": 2,
    "eob_pt_64": 7,
    "coeff_base_lf_eob": 5,
    "coeff_br_lf": 4,
    "dc_sign": 2,
}


def parse_normative_tables(path: Path = NORMATIVE) -> dict[str, list[list[int]]]:
    text = path.read_text()
    tables: dict[str, list[list[int]]] = {}
    for name in TABLE_SYMBOLS:
        match = re.search(
            rf"static const uint16_t enc_norm_{name}\[[^;]+?= \{{(.*?)\n\}};",
            text,
            re.DOTALL,
        )
        if match is None:
            raise ValueError(f"missing normative table {name}")
        rows = []
        for row in re.findall(r"\{([^{}]+)\}", match.group(1)):
            rows.append([int(value) for value in re.findall(r"(\d+)U", row)])
        if not rows:
            raise ValueError(f"empty normative table {name}")
        tables[name] = rows
    return tables


def materialize(row: list[int], symbol_count: int) -> list[int]:
    parameter = row[symbol_count - 1]
    count = row[symbol_count]
    adjustments = (0, -1, -2, 1, 1)
    rate_base = 2 if symbol_count < 4 else 3
    rates = (
        rate_base + adjustments[parameter // 25],
        rate_base + 1 + adjustments[(parameter // 5) % 5],
        rate_base + 2 + adjustments[parameter % 5],
    )
    return [32768 - value for value in row[:symbol_count - 1]] + [
        0, count, *rates
    ]


def update_cdf(cdf: list[int], symbol_count: int, symbol: int) -> None:
    interval = 2 if cdf[symbol_count] > 31 else (
        1 if cdf[symbol_count] > 15 else 0
    )
    rate = 2 + cdf[symbol_count + 1 + interval]
    target = 32768
    for index in range(symbol_count - 1):
        if index == symbol:
            target = 0
        if target < cdf[index]:
            cdf[index] -= (cdf[index] - target) >> rate
        else:
            cdf[index] += (target - cdf[index]) >> rate
    if cdf[symbol_count] < 32:
        cdf[symbol_count] += 1


def source_samples(width: int, height: int, flat: bool = False) -> bytes:
    if flat:
        return bytes([128]) * (width * height)
    values = (64, 160, 96, 208)
    return bytes(
        values[((y // 8) & 1) * 2 + ((x // 8) & 1)]
        for y in range(height)
        for x in range(width)
    )


def edge_extend(samples: bytes, width: int, height: int) -> tuple[bytes, int, int]:
    coded_width = ((width + 7) // 8) * 8
    coded_height = ((height + 7) // 8) * 8
    return (
        bytes(
            samples[min(y, height - 1) * width + min(x, width - 1)]
            for y in range(coded_height)
            for x in range(coded_width)
        ),
        coded_width,
        coded_height,
    )


def cul_level(level: int) -> int:
    value = min(7, abs(level))
    return value | 8 if level < 0 else value + 16 if level > 0 else 0


class AdaptiveState:
    def __init__(self, tables: dict[str, list[list[int]]]):
        self.tables = tables
        self.rows: dict[str, list[int]] = {}
        self.events: list[dict[str, object]] = []

    def symbol(
        self, name: str, row: int, symbol: int, context: int | None,
        boundary: dict[str, int],
    ) -> None:
        symbol_count = TABLE_SYMBOLS[name]
        key = f"{name}[{row}]"
        if key not in self.rows:
            self.rows[key] = materialize(self.tables[name][row], symbol_count)
        cdf = self.rows[key]
        before = list(cdf)
        update_cdf(cdf, symbol_count, symbol)
        self.events.append({
            **boundary,
            "name": name,
            "symbol": symbol,
            "context": context,
            "row": row,
            "row_key": key,
            "cdf_before": before,
            "cdf_after": list(cdf),
        })

    def implied(self, name: str, boundary: dict[str, int], detail: str) -> None:
        self.events.append({
            **boundary,
            "name": name,
            "adaptive": False,
            "detail": detail,
        })


def _spatial_context(
    above: list[int], left: list[int], mi_column: int, mi_row: int,
    coding_width_mi: int, coding_height_mi: int,
) -> tuple[int, int]:
    above_values = above[mi_column:mi_column + 2]
    left_values = left[mi_row:mi_row + 2]
    above_nonzero = 0
    left_nonzero = 0
    score = 0
    for value in above_values:
        above_nonzero |= value & 7
        score += 1 if value & 16 else -1 if value & 8 else 0
    for value in left_values:
        left_nonzero |= value & 7
        score += 1 if value & 16 else -1 if value & 8 else 0
    if coding_width_mi == 2 and coding_height_mi == 2:
        skip = 0
    else:
        skip_contexts = (
            (1, 2, 2, 3, 3), (2, 2, 3, 3, 4),
            (2, 3, 3, 4, 4), (3, 3, 4, 4, 5),
            (3, 4, 4, 5, 5),
        )
        skip = skip_contexts[min(4, above_nonzero)][min(4, left_nonzero)]
    return skip, 1 if score < 0 else 2 if score > 0 else 0


def _tx_partition(
    state: AdaptiveState, leaf: dict[str, int | str], leaf_index: int,
) -> None:
    width = int(leaf["width"])
    height = int(leaf["height"])
    group = 1 if width == height == 8 else 3 if width == height else 2
    boundary = {
        "leaf_index": leaf_index, "tx_index": -1,
        "x": int(leaf["x"]), "y": int(leaf["y"]),
    }
    split = int(width != 8 or height != 8)
    state.symbol("tx_do_partition", group, split, group, boundary)
    if not split:
        return
    type_group = 3 if width == height else 1 if width == 8 else 2
    symbol = 0 if width == height else 1 if width == 8 else 2
    state.symbol(
        "tx_partition_type", type_group, symbol, type_group, boundary
    )


def _coefficient_symbols(
    state: AdaptiveState, level: int, q_context: int, skip_context: int,
    sign_context: int, boundary: dict[str, int],
) -> None:
    magnitude = abs(level)
    txb_row = (((q_context * 2) * 5 + 1) * 10 + skip_context)
    state.symbol(
        "txb_skip", txb_row, int(magnitude == 0), skip_context, boundary
    )
    if magnitude == 0:
        state.implied(
            "primary_tx_type", boundary,
            "all-zero transform: DCT_DCT and coefficient suffix omitted",
        )
        return
    state.symbol("eob_pt_64", q_context * 3, 0, 0, boundary)
    state.implied(
        "primary_tx_type", boundary,
        "DC intra with eob=1: DCT_DCT, no transform-type symbol",
    )
    state.symbol(
        "coeff_base_lf_eob", (q_context * 5 + 1) * 4,
        min(magnitude - 1, 4), 0, boundary,
    )
    if magnitude >= 5:
        state.symbol(
            "coeff_br_lf", q_context * 14,
            min(magnitude - 5, 3), 0, boundary,
        )
    state.symbol(
        "dc_sign", ((q_context * 2) * 2) * 3 + sign_context,
        int(level < 0), sign_context, boundary,
    )
    if magnitude >= 8:
        state.events.append({
            **boundary,
            "name": "coeff_remainder",
            "adaptive": False,
            "value": magnitude - 8,
            **DC._remainder_literals(magnitude - 8),
        })


def model(width: int, height: int, qindex: int = 100,
          flat: bool = False) -> dict[str, object]:
    if not 17 <= width <= 64 or not 17 <= height <= 64:
        raise ValueError("dimensions must both be in 17..64")
    tables = parse_normative_tables()
    state = AdaptiveState(tables)
    source = source_samples(width, height, flat)
    padded, coded_width, coded_height = edge_extend(source, width, height)
    reconstruction = bytearray(coded_width * coded_height)
    above = [0] * (coded_width // 4)
    left = [0] * (coded_height // 4)
    q_context = DC.coeff_q_context(qindex)
    leaves = PARTITIONS.model(width, height)["leaves"]
    transforms: list[dict[str, object]] = []
    for leaf_index, leaf in enumerate(leaves):
        _tx_partition(state, leaf, leaf_index)
        leaf_width = int(leaf["width"])
        leaf_height = int(leaf["height"])
        tx_index = 0
        for local_y in range(0, leaf_height, 8):
            for local_x in range(0, leaf_width, 8):
                x = int(leaf["x"]) + local_x
                y = int(leaf["y"]) + local_y
                if x >= coded_width or y >= coded_height:
                    continue
                above_edge = (
                    reconstruction[(y - 1) * coded_width + x:
                                   (y - 1) * coded_width + x + 8]
                    if y else None
                )
                left_edge = (
                    [reconstruction[(y + offset) * coded_width + x - 1]
                     for offset in range(8)]
                    if x else None
                )
                prediction = DC.dc_prediction(above_edge, left_edge, 8)
                samples = [
                    padded[(y + row) * coded_width + x + column]
                    for row in range(8) for column in range(8)
                ]
                vector = DC.choose_dc_level(samples, prediction, 8, qindex)
                mi_column, mi_row = x // 4, y // 4
                skip_context, sign_context = _spatial_context(
                    above, left, mi_column, mi_row,
                    leaf_width // 4, leaf_height // 4,
                )
                boundary = {
                    "leaf_index": leaf_index, "tx_index": tx_index,
                    "x": x, "y": y,
                }
                _coefficient_symbols(
                    state, vector.quantized_dc, q_context,
                    skip_context, sign_context, boundary,
                )
                context_value = cul_level(vector.quantized_dc)
                above[mi_column:mi_column + 2] = [context_value] * 2
                left[mi_row:mi_row + 2] = [context_value] * 2
                for row in range(8):
                    start = (y + row) * coded_width + x
                    reconstruction[start:start + 8] = bytes(
                        [vector.reconstruction]
                    ) * 8
                transforms.append({
                    **boundary,
                    "prediction_block_width": leaf_width,
                    "prediction_block_height": leaf_height,
                    "prediction": prediction,
                    "level": vector.quantized_dc,
                    "reconstruction": vector.reconstruction,
                    "txb_skip_context": skip_context,
                    "dc_sign_context": sign_context,
                    "cul_level": context_value,
                })
                tx_index += 1
    cropped = b"".join(
        reconstruction[y * coded_width:y * coded_width + width]
        for y in range(height)
    )
    return {
        "width": width,
        "height": height,
        "qindex": qindex,
        "q_context": q_context,
        "source_profile": "flat128" if flat else "alternating-8x8-dc",
        "source_sha256": hashlib.sha256(source).hexdigest(),
        "reconstruction_sha256": hashlib.sha256(cropped).hexdigest(),
        "leaf_count": len(leaves),
        "transform_count": len(transforms),
        "leaves": leaves,
        "transforms": transforms,
        "events": state.events,
        "final_cdf_rows": dict(sorted(state.rows.items())),
    }


def vectors() -> dict[str, object]:
    return {
        "schema": "av2-m3-multileaf-cdf-v1",
        "normative_cdf_sha256": hashlib.sha256(NORMATIVE.read_bytes()).hexdigest(),
        "persistence": {
            "cdf_reset": "tile initialization only",
            "prediction_leaf": "persists",
            "transform_block": "persists",
            "prediction_block": "persists",
            "superblock_position": "persists within the tile",
            "coefficient_spatial_context": (
                "updated after every transform; above persists and left is "
                "overwritten/reset by superblock-row traversal"
            ),
            "q_context": (
                "selected once from frame base_q_index; block delta-q does "
                "not select a different coefficient CDF bank"
            ),
        },
        "cases": [model(width, height) for width, height in CASES],
        "q_context_cases": [
            model(32, 32, qindex) for qindex in Q_CONTEXT_CASES
        ],
        "flat_reproducer": model(32, 32, 100, flat=True),
    }


def avm_command(
    avmenc: Path, width: int, height: int, source: Path, output: Path,
) -> list[str]:
    return [
        str(avmenc), "--codec=av2", "--obu", "--good", "--quiet", "--debug",
        "--passes=1", "--limit=1", f"--width={width}", f"--height={height}",
        "--bit-depth=8", "--input-bit-depth=8", "--monochrome",
        "--threads=1", "--row-mt=0", "--lag-in-frames=0", "--end-usage=q",
        "--qp=100", "--enable-tcq=0", "--enable-cdef=0",
        "--enable-restoration=0", "--enable-deblocking=0", "--enable-gdf=0",
        "--enable-qm=0", "--enable-palette=0", "--enable-intrabc=0",
        "--enable-ist=0", "--enable-fsc=0", "--enable-sdp=0",
        "--enable-ext-partitions=0", "--enable-uneven-4way-partitions=0",
        "--enable-angle-delta=0", "--enable-smooth-intra=0",
        "--enable-paeth-intra=0", "--enable-mrls=0", "--enable-intra-dip=0",
        "--enable-tx-partition=1", "--enable-flip-idtx=0",
        "--max-partition-size=16", "--min-partition-size=16",
        "-o", str(output), str(source),
    ]


def run(command: list[str]) -> subprocess.CompletedProcess[bytes]:
    return subprocess.run(
        command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False
    )


def probe(
    output: Path, avmenc: Path = DEFAULT_AVMENC,
    avmdec: Path = DEFAULT_AVMDEC,
    project_decoder: Path = DEFAULT_PROJECT_DECODER,
) -> dict[str, object]:
    output.mkdir(parents=True, exist_ok=True)
    records = []
    for width, height in CASES:
        name = f"{width}x{height}"
        y = source_samples(width, height)
        chroma = 2 * ((width + 1) // 2) * ((height + 1) // 2)
        source = output / f"{name}.i420"
        obu = output / f"{name}.obu"
        avm_raw = output / f"{name}.avm.i400"
        project_raw = output / f"{name}.project.i400"
        source.write_bytes(y + bytes([128]) * chroma)
        encoded = run(avm_command(avmenc, width, height, source, obu))
        decoded = run([
            str(avmdec), "--codec=av2", "--rawvideo", "--limit=1",
            "--output-bit-depth=8", "-o", str(avm_raw), str(obu),
        ]) if encoded.returncode == 0 else None
        project = run([
            str(project_decoder), str(obu), "-o", str(project_raw)
        ]) if encoded.returncode == 0 else None
        avm_bytes = avm_raw.read_bytes() if avm_raw.is_file() else b""
        project_bytes = (
            project_raw.read_bytes() if project_raw.is_file() else b""
        )
        records.append({
            "name": name,
            "encoder_returncode": encoded.returncode,
            "avmdec_returncode": None if decoded is None else decoded.returncode,
            "project_decoder_returncode": (
                None if project is None else project.returncode
            ),
            "obu_bytes": obu.stat().st_size if obu.is_file() else 0,
            "obu_sha256": (
                hashlib.sha256(obu.read_bytes()).hexdigest()
                if obu.is_file() else None
            ),
            "output_sha256": hashlib.sha256(avm_bytes).hexdigest(),
            "validators_exact_equal": avm_bytes == project_bytes,
        })
        for path in (source, obu, avm_raw, project_raw):
            path.unlink(missing_ok=True)
    report = {
        "schema": "av2-m3-multileaf-avm-probes-v1",
        "avm_revision": "966a7d7cd6fcf60360caf5dc413b2aeeb65e144d",
        "avmenc_sha256": hashlib.sha256(avmenc.read_bytes()).hexdigest(),
        "avmdec_sha256": hashlib.sha256(avmdec.read_bytes()).hexdigest(),
        "project_decoder_sha256": hashlib.sha256(
            project_decoder.read_bytes()
        ).hexdigest(),
        "cases": records,
        "all_avm_pass": all(
            case["encoder_returncode"] == 0
            and case["avmdec_returncode"] == 0
            for case in records
        ),
        "project_decoder_all_pass": all(
            case["project_decoder_returncode"] == 0
            and case["validators_exact_equal"]
            for case in records
        ),
    }
    (output / "report.json").write_text(
        json.dumps(report, indent=2, sort_keys=True) + "\n"
    )
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--width", type=int, default=32)
    parser.add_argument("--height", type=int, default=32)
    parser.add_argument("--qindex", type=int, default=100)
    parser.add_argument("--vectors", action="store_true")
    parser.add_argument("--probe", type=Path)
    parser.add_argument("--avmenc", type=Path, default=DEFAULT_AVMENC)
    parser.add_argument("--avmdec", type=Path, default=DEFAULT_AVMDEC)
    parser.add_argument(
        "--project-decoder", type=Path, default=DEFAULT_PROJECT_DECODER
    )
    args = parser.parse_args()
    if args.probe:
        result = probe(
            args.probe, args.avmenc, args.avmdec, args.project_decoder
        )
    elif args.vectors:
        result = vectors()
    else:
        result = model(args.width, args.height, args.qindex)
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0 if not args.probe or result["all_avm_pass"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
