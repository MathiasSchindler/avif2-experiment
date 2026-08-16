#!/usr/bin/env python3
"""Private complete-stream model for luma 8x8 AC in I420/I444."""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import struct
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
ENCODER = ROOT / "experimental/encoder"
TOOLS = ENCODER / "tools"
DRIVER = ENCODER / "build/ac8x8-color-syntax-driver"
AVMDEC = ROOT / "build/reference/avm/avmdec"
PROJECT = ROOT / "build/host-cmake/av2heif-decoder"
VECTORS = ENCODER / "tests/ac8x8_color_syntax_vectors.json"
PROBES = ENCODER / "tests/ac8x8_color_syntax_probes.json"
PINNED_AVMDEC_SHA256 = (
    "c247cbb4e71e48bdec89f3c8b2b8efdce17f673aeaa39e3c116a276d16286aa0"
)
PINNED_PROJECT_SHA256 = (
    "7d17f598f2cdffe7dfb4ab46982cf5c4822215a5f429cb39e7748a60ce5ef535"
)


def _load(name: str, filename: str):
    spec = importlib.util.spec_from_file_location(name, TOOLS / filename)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


AC = _load("_color_ac8x8", "ac8x8_model.py")
DC = _load("_color_dc", "m3_dc_model.py")
CHROMA = _load("_color_chroma_geometry", "m3_chroma_boundary_model.py")

CASES = (
    ("i420-8x8-q50", 0, 50, 8, 8, "checker"),
    ("i444-8x8-q210", 2, 210, 8, 8, "diagonal"),
    ("i420-odd-q90", 0, 90, 17, 19, "lines"),
    ("i444-odd-q91", 2, 91, 17, 19, "detail"),
    ("i420-multileaf-q140", 0, 140, 32, 24, "diagonal"),
    ("i444-multileaf-q141", 2, 141, 32, 24, "checker"),
    ("i420-multisb-q190", 0, 190, 72, 64, "lines"),
    ("i444-multisb-q191", 2, 191, 72, 64, "detail"),
    ("i420-texture-q100", 0, 100, 24, 24, "prepared"),
    ("i444-texture-q150", 2, 150, 24, 24, "prepared"),
)


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def source(width: int, height: int, profile: str) -> bytes:
    values = []
    for y in range(height):
        for x in range(width):
            if profile == "checker":
                value = 24 if (x + y) & 1 else 232
            elif profile == "diagonal":
                value = 36 if x < y else 220
            elif profile == "lines":
                value = 48 if (x // 2) & 1 else 208
            elif profile == "prepared":
                value = (
                    x * 37 + y * 73 + x * y * 5 +
                    ((x ^ y) & 7) * 19 + 11
                ) & 255
            else:
                value = (x * 29 + y * 43 + x * y * 3 + 17) & 255
            values.append(value)
    return bytes(values)


def leaves(width: int, height: int):
    if width == 8 and height == 8:
        return ((0, 0, 8, 8, 0, 0),)
    return tuple(
        (leaf.column * 4, leaf.row * 4, leaf.width * 4, leaf.height * 4,
         leaf.column // 16, leaf.row // 16)
        for leaf in CHROMA.partition_leaves(width, height)
    )


def transforms(width: int, height: int):
    out = []
    for leaf_index, (x, y, leaf_width, leaf_height, sbx, sby) in enumerate(
        leaves(width, height)
    ):
        for local_y in range(0, leaf_height, 8):
            for local_x in range(0, leaf_width, 8):
                tx_x = x + local_x
                tx_y = y + local_y
                if tx_x < ((width + 7) // 8) * 8 and \
                        tx_y < ((height + 7) // 8) * 8:
                    out.append({
                        "x": tx_x, "y": tx_y, "leaf": leaf_index,
                        "leaf_width": leaf_width, "leaf_height": leaf_height,
                        "sbx": sbx, "sby": sby,
                    })
    return out


def round_level(coefficient: int, qindex: int) -> int:
    q = AC.qlookup(qindex)
    magnitude = (abs(coefficient) * 8 + q // 2) // q
    magnitude = min(31, magnitude)
    return -magnitude if coefficient < 0 else magnitude


def dc_prediction(
    reconstruction: bytearray, coded_width: int, x: int, y: int
) -> int:
    above = (
        reconstruction[(y - 1) * coded_width + x:
                       (y - 1) * coded_width + x + 8]
        if y else None
    )
    left = (
        [reconstruction[(y + offset) * coded_width + x - 1]
         for offset in range(8)]
        if x else None
    )
    return DC.dc_prediction(above, left, 8)


def luma_model(width: int, height: int, qindex: int, profile: str):
    active = source(width, height, profile)
    coded_width = ((width + 7) // 8) * 8
    coded_height = ((height + 7) // 8) * 8
    padded = bytes(
        active[min(y, height - 1) * width + min(x, width - 1)]
        for y in range(coded_height) for x in range(coded_width)
    )
    reconstruction = bytearray(coded_width * coded_height)
    level_blocks = []
    tx_records = []
    keep = {0, 1, 2, 8, 9, 10, 16, 17}
    for tx_index, tx in enumerate(transforms(width, height)):
        x, y = tx["x"], tx["y"]
        prediction = dc_prediction(reconstruction, coded_width, x, y)
        samples = tuple(
            tuple(padded[(y + row) * coded_width + x + column]
                  for column in range(8))
            for row in range(8)
        )
        analysis = AC.forward(samples, prediction)
        levels = [
            round_level(value, qindex) if index in keep else 0
            for index, value in enumerate(analysis)
        ]
        if not any(levels[index] for index in keep - {0}):
            levels[1 if tx_index & 1 else 8] = -1 if tx_index & 2 else 1
        block = AC.reconstruct(levels, qindex, prediction)
        for row in range(8):
            start = (y + row) * coded_width + x
            reconstruction[start:start + 8] = bytes(block[row])
        level_blocks.append(tuple(levels))
        tx_records.append({
            **tx, "prediction": prediction,
            "nonzero": sum(level != 0 for level in levels),
            "eob": max(
                (index + 1 for index, position in enumerate(AC.SCAN)
                 if levels[position] != 0),
                default=0,
            ),
            "levels_sha256": sha256_bytes(
                b"".join(struct.pack("<h", level) for level in levels)
            ),
        })
    cropped = b"".join(
        reconstruction[y * coded_width:y * coded_width + width]
        for y in range(height)
    )
    return level_blocks, cropped, tx_records, active


def round_shift(value: int, bits: int) -> int:
    return (value + (1 << (bits - 1))) >> bits


def chroma_residual(level: int, qindex: int, width: int, height: int) -> int:
    dequant = AC.dequantize(level, qindex)
    if width != height:
        dequant = round_shift(2896 * dequant, 12)
    row = max(-32768, min(32767, round_shift(64 * dequant, 7)))
    shift = 12 if width >= 16 and height >= 16 else (
        11 if width >= 8 and height >= 8 else 10
    )
    return max(-256, min(255, round_shift(64 * row, shift)))


def color_dc_prediction(
    plane: bytearray, coded_width: int, x: int, y: int,
    width: int, height: int,
) -> int:
    above = (
        plane[(y - 1) * coded_width + x:
              (y - 1) * coded_width + x + width]
        if y else None
    )
    left = (
        [plane[(y + offset) * coded_width + x - 1]
         for offset in range(height)]
        if x else None
    )
    total = sum(above or ()) + sum(left or ())
    if above is not None and left is not None:
        return (total + (width + height) // 2) // (width + height)
    if above is not None:
        return (total + width // 2) // width
    if left is not None:
        return (total + height // 2) // height
    return 128


def chroma_model(
    width: int, height: int, format_id: int, qindex: int, count: int
):
    subsampling = 1 if format_id == 0 else 0
    active_width = (width + (1 << subsampling) - 1) >> subsampling
    active_height = (height + (1 << subsampling) - 1) >> subsampling
    chroma_leaves = CHROMA.partition_leaves(width, height)
    if width == 8 and height == 8:
        chroma_leaves = (CHROMA.Leaf(0, 0, 2, 2),)
    geometries = []
    for leaf in chroma_leaves:
        tx_width, tx_height, tx_size, eob_class = \
            CHROMA.transform_geometry(leaf, format_id)
        geometries.append({
            "x": leaf.column * 4 >> subsampling,
            "y": leaf.row * 4 >> subsampling,
            "width": tx_width, "height": tx_height,
            "tx_size": tx_size, "eob_class": eob_class,
            "sbx": leaf.column // 16, "sby": leaf.row // 16,
        })
    assert len(geometries) == count
    coded_width = max(item["x"] + item["width"] for item in geometries)
    coded_height = max(item["y"] + item["height"] for item in geometries)
    u_levels = tuple(
        0 if item["width"] != item["height"] else
        (0, 2, -1, 1)[index % 4]
        for index, item in enumerate(geometries)
    )
    v_levels = tuple(
        0 if item["width"] != item["height"] else
        (-1, 0, 2, -2)[index % 4]
        for index, item in enumerate(geometries)
    )
    outputs = []
    for levels in (u_levels, v_levels):
        plane = bytearray(coded_width * coded_height)
        for item, level in zip(geometries, levels):
            prediction = color_dc_prediction(
                plane, coded_width, item["x"], item["y"],
                item["width"], item["height"],
            )
            value = max(0, min(
                255,
                prediction + chroma_residual(
                    level, qindex, item["width"], item["height"]
                ),
            ))
            for row in range(item["height"]):
                start = (item["y"] + row) * coded_width + item["x"]
                plane[start:start + item["width"]] = bytes(
                    [value] * item["width"]
                )
        outputs.append(b"".join(
            plane[y * coded_width:y * coded_width + active_width]
            for y in range(active_height)
        ))
    return u_levels, v_levels, outputs, geometries


def cul_level(levels) -> int:
    total = min(7, sum(abs(level) for level in levels))
    dc = levels[0]
    return total | (8 if dc < 0 else 16 if dc > 0 else 0)


def context_transcript(
    width: int, height: int, format_id: int, qindex: int,
    level_blocks, u_levels, v_levels, tx_records, geometries,
):
    q_context = AC.qcontext(qindex)
    mi_columns = ((width + 7) // 8) * 2
    luma_above = [0] * mi_columns
    luma_left = [0] * 16
    chroma_above = [[0] * mi_columns for _ in range(2)]
    chroma_left = [[0] * 16 for _ in range(2)]
    events = []
    last_sby = 0
    tx_by_sb = {}
    for index, tx in enumerate(tx_records):
        tx_by_sb.setdefault((tx["sbx"], tx["sby"]), []).append((index, tx))
    uv_by_sb = {}
    for index, item in enumerate(geometries):
        uv_by_sb.setdefault((item["sbx"], item["sby"]), []).append(
            (index, item)
        )
    sb_order = []
    for sby in range((height + 63) // 64):
        for sbx in range((width + 63) // 64):
            sb_order.append((sbx, sby))
    for sbx, sby in sb_order:
        if sby != last_sby:
            luma_left[:] = [0] * 16
            chroma_left[0][:] = [0] * 16
            chroma_left[1][:] = [0] * 16
            last_sby = sby
        for index, tx in tx_by_sb.get((sbx, sby), ()):
            column, row = tx["x"] // 4, (tx["y"] // 4) % 16
            columns = min(2, mi_columns - column)
            rows = min(2, ((height + 7) // 8) * 2 - tx["y"] // 4)
            top = 0
            side = 0
            score = 0
            for value in luma_above[column:column + columns]:
                top |= value & 7
                score += 1 if value & 16 else -1 if value & 8 else 0
            for value in luma_left[row:row + rows]:
                side |= value & 7
                score += 1 if value & 16 else -1 if value & 8 else 0
            if tx["leaf_width"] == tx["leaf_height"] == 8:
                skip = 0
            else:
                table = (
                    (1, 2, 2, 3, 3), (2, 2, 3, 3, 4),
                    (2, 3, 3, 4, 4), (3, 3, 4, 4, 5),
                    (3, 4, 4, 5, 5),
                )
                skip = table[min(4, top)][min(4, side)]
            sign = 1 if score < 0 else 2 if score > 0 else 0
            events.append({
                "order": len(events), "plane": "Y", "transform": index,
                "x": tx["x"], "y": tx["y"], "skip": skip, "sign": sign,
                "q_context": q_context, "eob": tx["eob"],
            })
            value = cul_level(level_blocks[index])
            luma_above[column:column + columns] = [value] * columns
            luma_left[row:row + rows] = [value] * rows
        for uv_index, item in uv_by_sb.get((sbx, sby), ()):
            width_units = item["width"] // 4
            height_units = item["height"] // 4
            column = item["x"] // 4
            row = (item["y"] // 4) % 16
            u_has_eob = u_levels[uv_index] != 0
            for plane, levels in enumerate((u_levels, v_levels)):
                top = 0
                side = 0
                score = 0
                for value in chroma_above[plane][
                    column:column + width_units
                ]:
                    top |= value & 7
                    score += 1 if value & 16 else -1 if value & 8 else 0
                for value in chroma_left[plane][row:row + height_units]:
                    side |= value & 7
                    score += 1 if value & 16 else -1 if value & 8 else 0
                base_skip = int(bool(top)) + int(bool(side))
                skip = (
                    base_skip + 6 if plane == 0 else
                    base_skip + (6 if u_has_eob else 0)
                )
                sign = 1 if score < 0 else 2 if score > 0 else 0
                level = levels[uv_index]
                events.append({
                    "order": len(events),
                    "plane": "U" if plane == 0 else "V",
                    "transform": uv_index, "x": item["x"], "y": item["y"],
                    "skip": skip, "base_skip": base_skip, "sign": sign,
                    "q_context": q_context, "eob": int(level != 0),
                    "u_eob_coupling": plane == 1 and u_has_eob,
                })
                magnitude = min(7, abs(level))
                value = magnitude | (
                    8 if level < 0 else 16 if level > 0 else 0
                )
                chroma_above[plane][
                    column:column + width_units
                ] = [value] * width_units
                chroma_left[plane][
                    row:row + height_units
                ] = [value] * height_units
    return events


def case_model(case):
    name, format_id, qindex, width, height, profile = case
    level_blocks, y_raw, tx_records, active = luma_model(
        width, height, qindex, profile
    )
    uv_count = 1 if width == 8 and height == 8 else \
        ((width + 15) // 16) * ((height + 15) // 16)
    u_levels, v_levels, uv_raw, geometries = chroma_model(
        width, height, format_id, qindex, uv_count
    )
    level_bytes = b"".join(
        struct.pack("<h", level)
        for block in level_blocks for level in block
    ) + b"".join(struct.pack("<h", level) for level in u_levels) + \
        b"".join(struct.pack("<h", level) for level in v_levels)
    reconstruction = y_raw + uv_raw[0] + uv_raw[1]
    transcript = context_transcript(
        width, height, format_id, qindex, level_blocks,
        u_levels, v_levels, tx_records, geometries,
    )
    return {
        "name": name, "format": format_id, "qindex": qindex,
        "q_context": AC.qcontext(qindex), "width": width, "height": height,
        "profile": profile, "source_sha256": sha256_bytes(active),
        "luma_transform_count": len(level_blocks),
        "chroma_transform_count": uv_count,
        "luma_nonzero_ac_transform_count": sum(
            any(block[index] for index in range(1, 64))
            for block in level_blocks
        ),
        "level_bytes": len(level_bytes),
        "levels_sha256": sha256_bytes(level_bytes),
        "reconstruction_bytes": len(reconstruction),
        "reconstruction_sha256": sha256_bytes(reconstruction),
        "transcript_event_count": len(transcript),
        "transcript_sha256": sha256_bytes(json.dumps(
            transcript, sort_keys=True, separators=(",", ":")
        ).encode()),
        "first_events": transcript[:6],
        "last_events": transcript[-6:],
        "_level_bytes": level_bytes,
        "_reconstruction": reconstruction,
        "_transcript": transcript,
    }


def vectors():
    cases = []
    for definition in CASES:
        modeled = case_model(definition)
        cases.append({
            key: value for key, value in modeled.items()
            if not key.startswith("_")
        })
    return {
        "schema": "av2-ac8x8-color-syntax-v1",
        "recipe": {
            "tile_order": (
                "superblocks raster; complete Y partition/leaf/transform "
                "subtree, then U/V color leaves for that superblock"
            ),
            "luma_transform_order": (
                "leaf DFS; 8x8 transforms row-major inside each leaf"
            ),
            "cdf_lifetime": (
                "one EncSyntaxCdfs initialized per tile; all adaptive rows "
                "persist across transforms, leaves, planes and superblocks"
            ),
            "secondary_transform": (
                "after primary DCT_DCT, every intra luma eob>1 emits "
                "stx_flag=0 from the adaptive intra 8x8 sec-tx CDF"
            ),
            "luma_neighbor_state": (
                "cul_level over all 64 coefficients; above persists for the "
                "tile, left persists across superblock columns and resets at "
                "each 64-luma-row superblock band"
            ),
            "chroma_isolation": (
                "U and V own neighbor arrays; luma AC changes neither chroma "
                "skip nor chroma DC-sign context"
            ),
            "u_to_v": (
                "U is coded and committed before V; V skip adds 6 iff the "
                "corresponding U transform has nonzero EOB"
            ),
            "edges": (
                "cropped transforms retain full coded transform geometry; "
                "contexts cover only in-grid 4x4 units"
            ),
            "q_contexts": "0:1..90, 1:91..140, 2:141..190, 3:191..255",
        },
        "case_count": len(cases),
        "cases": cases,
    }


def run(command):
    return subprocess.run(
        command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False
    )


def probe(work: Path):
    work.mkdir(parents=True, exist_ok=True)
    records = []
    for definition in CASES:
        modeled = case_model(definition)
        level_path = work / f"{modeled['name']}.levels"
        obu = work / f"{modeled['name']}.obu"
        avm = work / f"{modeled['name']}.avm"
        extension = "i420" if modeled["format"] == 0 else "i444"
        project = work / f"{modeled['name']}.project.{extension}"
        level_path.write_bytes(modeled["_level_bytes"])
        encoded = run([
            str(DRIVER), str(modeled["format"]), str(modeled["qindex"]),
            str(modeled["width"]), str(modeled["height"]),
            str(level_path), str(obu),
        ])
        avm_result = run([
            str(AVMDEC), "--codec=av2", "--rawvideo", "--limit=1",
            "--output-bit-depth=8", "-o", str(avm), str(obu),
        ]) if encoded.returncode == 0 else None
        project_result = run([
            str(PROJECT), str(obu), "-o", str(project)
        ]) if encoded.returncode == 0 else None
        expected = modeled["_reconstruction"]
        avm_bytes = avm.read_bytes() if avm.is_file() else b""
        project_bytes = project.read_bytes() if project.is_file() else b""
        exact = (
            avm_result is not None and project_result is not None
            and avm_result.returncode == 0 and project_result.returncode == 0
            and expected == avm_bytes == project_bytes
        )
        records.append({
            "name": modeled["name"],
            "format": modeled["format"],
            "qindex": modeled["qindex"],
            "q_context": modeled["q_context"],
            "encoder_returncode": encoded.returncode,
            "encoder_error": encoded.stderr.decode(errors="replace").strip(),
            "avmdec_returncode": (
                None if avm_result is None else avm_result.returncode
            ),
            "project_returncode": (
                None if project_result is None else project_result.returncode
            ),
            "obu_bytes": obu.stat().st_size if obu.is_file() else 0,
            "obu_sha256": sha256_bytes(obu.read_bytes())
            if obu.is_file() else None,
            "reconstruction_bytes": len(expected),
            "local_reconstruction_sha256": sha256_bytes(expected),
            "avm_reconstruction_sha256": sha256_bytes(avm_bytes),
            "project_reconstruction_sha256": sha256_bytes(project_bytes),
            "local_avm_project_exact": exact,
        })
        for path in (level_path, obu, avm, project):
            path.unlink(missing_ok=True)
    exact_count = sum(case["local_avm_project_exact"] for case in records)
    result = {
        "schema": "av2-ac8x8-color-syntax-probes-v1",
        "case_count": len(records),
        "triple_exact_count": exact_count,
        "all_pass": exact_count == len(records),
        "avmdec_sha256": sha256_bytes(AVMDEC.read_bytes()),
        "project_decoder_sha256": sha256_bytes(PROJECT.read_bytes()),
        "avmdec_hash_matches_pin": (
            sha256_bytes(AVMDEC.read_bytes()) == PINNED_AVMDEC_SHA256
        ),
        "project_hash_matches_pin": (
            sha256_bytes(PROJECT.read_bytes()) == PINNED_PROJECT_SHA256
        ),
        "cases": records,
    }
    (work / "report.json").write_text(
        json.dumps(result, indent=2, sort_keys=True) + "\n"
    )
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--write", action="store_true")
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--probe", type=Path)
    args = parser.parse_args()
    if args.probe:
        result = probe(args.probe)
        print(json.dumps(result, indent=2, sort_keys=True))
        return 0 if (
            result["all_pass"] and result["avmdec_hash_matches_pin"]
            and result["project_hash_matches_pin"]
        ) else 1
    generated = json.dumps(vectors(), indent=2, sort_keys=True) + "\n"
    if args.write:
        VECTORS.write_text(generated)
    elif args.check:
        if not VECTORS.is_file() or VECTORS.read_text() != generated:
            raise SystemExit(f"{VECTORS}: generated evidence is stale")
    else:
        print(generated, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
