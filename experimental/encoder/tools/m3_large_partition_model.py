#!/usr/bin/env python3
"""Model and probe the rectangle-only AV2 partition path for 17..64 images."""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
from dataclasses import dataclass
from pathlib import Path

from m3_dc_model import choose_dc_level, reconstruct_dc


ROOT = Path(__file__).resolve().parents[3]
DEFAULT_AVMENC = ROOT / "build/reference/avm/avmenc"
DEFAULT_AVMDEC = ROOT / "build/reference/avm/avmdec"
DEFAULT_PROJECT_DECODER = ROOT / "build/host-cmake/av2heif-decoder"
DEFAULT_OUTPUT = ROOT / "experimental/encoder/build/m3-large-partitions"
CASES = (
    (17, 17), (17, 64), (31, 33), (32, 32), (33, 31),
    (48, 48), (63, 64), (64, 17), (64, 63), (64, 64),
)

# width_mi, height_mi, split-size adjustment, rectangular-size adjustment
GEOMETRY = {
    "8x8": (2, 2, 0, 0),
    "8x16": (2, 4, 1, 1),
    "16x8": (4, 2, 1, 2),
    "16x16": (4, 4, 1, 0),
    "16x32": (4, 8, 2, 1),
    "32x16": (8, 4, 2, 2),
    "32x32": (8, 8, 2, 3),
    "32x64": (8, 16, 3, 4),
    "64x32": (16, 8, 3, 5),
    "64x64": (16, 16, 3, 6),
}


@dataclass(frozen=True)
class Node:
    row: int
    column: int
    shape: str

    @property
    def geometry(self) -> tuple[int, int, int, int]:
        return GEOMETRY[self.shape]


def shape_name(width_mi: int, height_mi: int) -> str:
    return f"{width_mi * 4}x{height_mi * 4}"


def ordinary_children(node: Node, partition: str) -> tuple[Node, Node]:
    width, height, _, _ = node.geometry
    if partition == "HORZ":
        child_height = height // 2
        shape = shape_name(width, child_height)
        return (
            Node(node.row, node.column, shape),
            Node(node.row + child_height, node.column, shape),
        )
    child_width = width // 2
    shape = shape_name(child_width, height)
    return (
        Node(node.row, node.column, shape),
        Node(node.row, node.column + child_width, shape),
    )


def forced_partition(mi_rows: int, mi_columns: int, node: Node) -> str | None:
    width, height, _, _ = node.geometry
    has_rows = node.row + height // 2 < mi_rows
    has_columns = node.column + width // 2 < mi_columns
    if has_rows and has_columns:
        return None
    if width == height:
        return "VERT" if has_rows and not has_columns else "HORZ"
    if height > width:
        if not has_rows:
            return "HORZ"
        if (
            not has_columns
            and width >= 4
            and node.column + width // 4 >= mi_columns
        ):
            return "HORZ"
    else:
        if not has_columns:
            return "VERT"
        if (
            not has_rows
            and height >= 4
            and node.row + height // 4 >= mi_rows
        ):
            return "VERT"
    return None


def partial_boundary_axis(
    mi_rows: int, mi_columns: int, node: Node
) -> str | None:
    width, height, _, _ = node.geometry
    has_rows = node.row + height // 2 < mi_rows
    has_columns = node.column + width // 2 < mi_columns
    if height > width and has_rows and not has_columns:
        return "VERT"
    if width > height and not has_rows and has_columns:
        return "HORZ"
    return None


def context_bit(value: int, dimension: int, extension: int) -> int:
    shift = dimension.bit_length() - 1
    shift = max(0, shift - extension)
    return (value >> shift) & 1


class PartitionModel:
    def __init__(self, width: int, height: int):
        if not 17 <= width <= 64 or not 17 <= height <= 64:
            raise ValueError("dimensions must both be in 17..64")
        self.width = width
        self.height = height
        self.mi_columns = ((width + 7) // 8) * 2
        self.mi_rows = ((height + 7) // 8) * 2
        self.above = [0] * 16
        self.left = [0] * 16
        self.decisions: list[dict[str, object]] = []
        self.leaves: list[dict[str, int | str]] = []

    def contexts(self, node: Node, axis: str) -> tuple[int, int, int]:
        width, height, split_adjust, rect_adjust = node.geometry
        left_smaller = context_bit(self.left[node.row], height, 1)
        above_smaller = context_bit(self.above[node.column], width, 1)
        if axis == "HORZ":
            first = context_bit(self.left[node.row], height, 2)
            second = context_bit(
                self.left[node.row + height // 2], height, 2
            )
        else:
            first = context_bit(self.above[node.column], width, 2)
            second = context_bit(
                self.above[node.column + width // 2], width, 2
            )
        return (
            split_adjust * 4 + left_smaller * 2 + above_smaller,
            rect_adjust * 4 + left_smaller * 2 + above_smaller,
            split_adjust * 4 + first * 2 + second,
        )

    def commit_leaf(self, node: Node) -> None:
        width, height, _, _ = node.geometry
        for column in range(node.column, min(node.column + width, 16)):
            self.above[column] = 64 - width
        for row in range(node.row, min(node.row + height, 16)):
            self.left[row] = 64 - height
        self.leaves.append(
            {
                "x": node.column * 4,
                "y": node.row * 4,
                "width": width * 4,
                "height": height * 4,
            }
        )

    def visit(self, node: Node) -> None:
        if node.row >= self.mi_rows or node.column >= self.mi_columns:
            return
        width, height, _, _ = node.geometry
        forced = forced_partition(self.mi_rows, self.mi_columns, node)
        if forced is not None:
            self.decisions.append(
                {
                    "node": node.shape,
                    "x": node.column * 4,
                    "y": node.row * 4,
                    "partition": forced,
                    "boundary_implied": True,
                    "symbols": [],
                }
            )
            for child in ordinary_children(node, forced):
                self.visit(child)
            return
        if width <= 4 and height <= 4:
            split, rect, extension = self.contexts(node, "HORZ")
            self.decisions.append(
                {
                    "node": node.shape,
                    "x": node.column * 4,
                    "y": node.row * 4,
                    "partition": "NONE",
                    "boundary_implied": False,
                    "contexts": {
                        "split": split,
                        "rect_type": rect,
                        "extension": extension,
                    },
                    "symbols": [["do_split", 0]],
                }
            )
            self.commit_leaf(node)
            return

        boundary_axis = partial_boundary_axis(
            self.mi_rows, self.mi_columns, node
        )
        axis = "HORZ" if height >= width else "VERT"
        split, rect, extension = self.contexts(node, axis)
        symbols: list[list[object]] = []
        if boundary_axis is None:
            symbols.append(["do_split", 1])
        # AV2 v1.0 permits square-split syntax only at 128x128 and 256x256.
        # Every node in this bounded 64x64-superblock model therefore proceeds
        # directly to the rectangular branch.
        symbols.append(["rect_type", 0 if axis == "HORZ" else 1])
        self.decisions.append(
            {
                "node": node.shape,
                "x": node.column * 4,
                "y": node.row * 4,
                "partition": axis,
                "boundary_implied": boundary_axis is not None,
                "contexts": {
                    "split": split,
                    "rect_type": rect,
                    "extension": extension,
                },
                "symbols": symbols,
            }
        )
        for child in ordinary_children(node, axis):
            self.visit(child)

    def result(self) -> dict[str, object]:
        self.visit(Node(0, 0, "64x64"))
        covered: set[tuple[int, int]] = set()
        dependency_safe = True
        for leaf in self.leaves:
            x0 = int(leaf["x"]) // 4
            y0 = int(leaf["y"]) // 4
            width = int(leaf["width"]) // 4
            height = int(leaf["height"]) // 4
            for row in range(y0, min(y0 + height, self.mi_rows)):
                for column in range(x0, min(x0 + width, self.mi_columns)):
                    if row > 0 and row == y0 and (row - 1, column) not in covered:
                        dependency_safe = False
                    if (
                        column > 0
                        and column == x0
                        and (row, column - 1) not in covered
                    ):
                        dependency_safe = False
                    if (row, column) in covered:
                        raise AssertionError("overlapping partition leaves")
                    covered.add((row, column))
        expected = {
            (row, column)
            for row in range(self.mi_rows)
            for column in range(self.mi_columns)
        }
        return {
            "width": self.width,
            "height": self.height,
            "mi_columns": self.mi_columns,
            "mi_rows": self.mi_rows,
            "root": "64x64",
            "square_split_symbols": 0,
            "decisions": self.decisions,
            "leaves": self.leaves,
            "active_grid_exact_cover": covered == expected,
            "above_left_dependency_safe": dependency_safe,
            "global_leaf_order_is_raster": self.leaves
            == sorted(self.leaves, key=lambda leaf: (leaf["y"], leaf["x"])),
        }


def model(width: int, height: int) -> dict[str, object]:
    return PartitionModel(width, height).result()


def source_samples(width: int, height: int) -> bytes:
    return bytes(
        (
            17 * x + 29 * y + (((x // 5) ^ (y // 7)) & 1) * 61
            + (x * y) % 23
        ) & 255
        for y in range(height)
        for x in range(width)
    )


def edge_extend(samples: bytes, width: int, height: int) -> bytes:
    if len(samples) != width * height:
        raise ValueError("sample count does not match dimensions")
    coded_width = (width + 7) // 8 * 8
    coded_height = (height + 7) // 8 * 8
    return bytes(
        samples[min(y, height - 1) * width + min(x, width - 1)]
        for y in range(coded_height)
        for x in range(coded_width)
    )


def transform_nodes(leaf: dict[str, int | str]) -> list[tuple[int, int]]:
    return [
        (x, y)
        for y in range(int(leaf["y"]), int(leaf["y"]) + int(leaf["height"]), 8)
        for x in range(int(leaf["x"]), int(leaf["x"]) + int(leaf["width"]), 8)
    ]


def _prediction(
    canvas: bytearray, canvas_width: int, canvas_height: int,
    x: int, y: int
) -> int:
    edges: list[int] = []
    if x:
        edges.extend(
            canvas[min(y + row, canvas_height - 1) * canvas_width + x - 1]
            for row in range(8)
        )
    if y:
        row = (y - 1) * canvas_width
        edges.extend(
            canvas[row + min(x + column, canvas_width - 1)]
            for column in range(8)
        )
    return 128 if not edges else (sum(edges) + len(edges) // 2) // len(edges)


def _coefficient_context(
    above: list[int], left: list[int], mi_columns: int, mi_rows: int,
    x: int, y: int, coding_width: int, coding_height: int
) -> tuple[int, int]:
    column = x // 4
    row = y // 4
    width = min(2, mi_columns - column)
    height = min(2, mi_rows - row)
    above_values = above[column:column + width]
    left_values = left[row:row + height]
    dc_score = sum(
        1 if value & 16 else -1 if value & 8 else 0
        for value in above_values + left_values
    )
    sign = 1 if dc_score < 0 else 2 if dc_score > 0 else 0
    if coding_width == 8 and coding_height == 8:
        return 0, sign
    top = min(4, _or_low_levels(above_values))
    side = min(4, _or_low_levels(left_values))
    skip_contexts = (
        (1, 2, 2, 3, 3), (2, 2, 3, 3, 4),
        (2, 3, 3, 4, 4), (3, 3, 4, 4, 5),
        (3, 4, 4, 5, 5),
    )
    return skip_contexts[top][side], sign


def _or_low_levels(values: list[int]) -> int:
    result = 0
    for value in values:
        result |= value & 7
    return result


def _commit_context(
    above: list[int], left: list[int], mi_columns: int, mi_rows: int,
    x: int, y: int, level: int
) -> None:
    context = min(7, abs(level))
    context |= 8 if level < 0 else 16 if level > 0 else 0
    column = x // 4
    row = y // 4
    for index in range(column, min(column + 2, mi_columns)):
        above[index] = context
    for index in range(row, min(row + 2, mi_rows)):
        left[index] = context


def _crop(canvas: bytearray, stride: int, width: int, height: int) -> bytes:
    return b"".join(
        canvas[y * stride:y * stride + width] for y in range(height)
    )


def reconstruction_model(
    width: int, height: int, qindex: int = 100
) -> dict[str, object]:
    partition = model(width, height)
    source = source_samples(width, height)
    padded = edge_extend(source, width, height)
    coded_width = (width + 7) // 8 * 8
    coded_height = (height + 7) // 8 * 8
    local = bytearray(coded_width * coded_height)
    decoder = bytearray(width * height)
    mi_columns = int(partition["mi_columns"])
    mi_rows = int(partition["mi_rows"])
    above = [0] * mi_columns
    left = [0] * mi_rows
    transforms: list[dict[str, int]] = []
    leaf_order: list[list[int]] = []

    for leaf_index, leaf in enumerate(partition["leaves"]):
        leaf_order.append([int(leaf["x"]), int(leaf["y"])])
        for tx_index, (x, y) in enumerate(transform_nodes(leaf)):
            local_prediction = _prediction(
                local, coded_width, coded_height, x, y
            )
            decoder_prediction = _prediction(decoder, width, height, x, y)
            if local_prediction != decoder_prediction:
                raise AssertionError("local and decoder prediction diverged")
            block = [
                padded[(y + row) * coded_width + x + column]
                for row in range(8) for column in range(8)
            ]
            vector = choose_dc_level(block, local_prediction, 8, qindex)
            skip, sign = _coefficient_context(
                above, left, mi_columns, mi_rows, x, y,
                int(leaf["width"]), int(leaf["height"])
            )
            value = vector.reconstruction
            for row in range(8):
                local[(y + row) * coded_width + x:
                      (y + row) * coded_width + x + 8] = bytes([value]) * 8
            visible_width = min(8, width - x)
            visible_height = min(8, height - y)
            for row in range(visible_height):
                decoder[(y + row) * width + x:
                        (y + row) * width + x + visible_width] = (
                            bytes([value]) * visible_width
                        )
            _commit_context(
                above, left, mi_columns, mi_rows, x, y, vector.quantized_dc
            )
            transforms.append({
                "leaf": leaf_index,
                "tx": tx_index,
                "x": x,
                "y": y,
                "prediction": local_prediction,
                "level": vector.quantized_dc,
                "reconstruction": value,
                "all_zero_context": skip,
                "dc_sign_context": sign,
            })

    local_crop = _crop(local, coded_width, width, height)
    decoder_crop = bytes(decoder)
    source_error = sum(
        (sample - reconstruction) ** 2
        for sample, reconstruction in zip(source, local_crop)
    )
    decoder_order = [(item["x"], item["y"]) for item in transforms]
    legacy = _legacy_raster_reconstruction(
        source, padded, width, height, coded_width, coded_height, qindex,
        decoder_order,
    )
    legacy_order = legacy["order"]
    first_order_difference = next(
        (
            index for index, pair in enumerate(zip(decoder_order, legacy_order))
            if pair[0] != pair[1]
        ),
        None,
    )
    trace = json.dumps(
        transforms, sort_keys=True, separators=(",", ":")
    ).encode()
    return {
        "width": width,
        "height": height,
        "coded_width": coded_width,
        "coded_height": coded_height,
        "qindex": qindex,
        "source_sha256": hashlib.sha256(source).hexdigest(),
        "padded_source_sha256": hashlib.sha256(padded).hexdigest(),
        "leaf_order": leaf_order,
        "transform_order": [[x, y] for x, y in decoder_order],
        "transform_count": len(transforms),
        "context_trace_sha256": hashlib.sha256(trace).hexdigest(),
        "first_transforms": transforms[:8],
        "local_reconstruction_sha256": hashlib.sha256(local_crop).hexdigest(),
        "decoder_reconstruction_sha256": hashlib.sha256(
            decoder_crop
        ).hexdigest(),
        "local_decoder_exact": local_crop == decoder_crop,
        "active_squared_error": source_error,
        "legacy_raster_reconstruction_sha256": legacy["sha256"],
        "legacy_raster_squared_error": legacy["squared_error"],
        "legacy_stream_decoder_sha256": legacy["stream_decoder_sha256"],
        "legacy_stream_decoder_squared_error": legacy[
            "stream_decoder_squared_error"
        ],
        "legacy_local_decoder_exact": (
            legacy["sha256"] == legacy["stream_decoder_sha256"]
        ),
        "first_legacy_order_difference": first_order_difference,
        "legacy_raster_matches_decoder_order": decoder_order == legacy_order,
    }


def _legacy_raster_reconstruction(
    source: bytes, padded: bytes, width: int, height: int,
    coded_width: int, coded_height: int, qindex: int,
    decoder_order: list[tuple[int, int]],
) -> dict[str, object]:
    canvas = bytearray(coded_width * coded_height)
    order: list[tuple[int, int]] = []
    levels: list[int] = []
    for y in range(0, coded_height, 8):
        for x in range(0, coded_width, 8):
            order.append((x, y))
            prediction = _prediction(
                canvas, coded_width, coded_height, x, y
            )
            block = [
                padded[(y + row) * coded_width + x + column]
                for row in range(8) for column in range(8)
            ]
            vector = choose_dc_level(block, prediction, 8, qindex)
            levels.append(vector.quantized_dc)
            value = vector.reconstruction
            for row in range(8):
                canvas[(y + row) * coded_width + x:
                       (y + row) * coded_width + x + 8] = bytes([value]) * 8
    cropped = _crop(canvas, coded_width, width, height)
    stream_decoder = bytearray(coded_width * coded_height)
    for (x, y), level in zip(decoder_order, levels):
        prediction = _prediction(
            stream_decoder, coded_width, coded_height, x, y
        )
        value = reconstruct_dc(level, qindex, prediction, 8)[2]
        for row in range(8):
            stream_decoder[(y + row) * coded_width + x:
                           (y + row) * coded_width + x + 8] = (
                               bytes([value]) * 8
                           )
    stream_decoder_crop = _crop(
        stream_decoder, coded_width, width, height
    )
    return {
        "order": order,
        "sha256": hashlib.sha256(cropped).hexdigest(),
        "squared_error": sum(
            (sample - reconstruction) ** 2
            for sample, reconstruction in zip(source, cropped)
        ),
        "stream_decoder_sha256": hashlib.sha256(
            stream_decoder_crop
        ).hexdigest(),
        "stream_decoder_squared_error": sum(
            (sample - reconstruction) ** 2
            for sample, reconstruction in zip(source, stream_decoder_crop)
        ),
    }


def avm_command(
    avmenc: Path, width: int, height: int, source: Path, output: Path
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


def probe_case(
    output: Path, width: int, height: int, avmenc: Path, avmdec: Path,
    project_decoder: Path
) -> dict[str, object]:
    name = f"{width}x{height}"
    source = output / f"{name}.i420"
    obu = output / f"{name}.obu"
    avm_raw = output / f"{name}.avm.i400"
    project_raw = output / f"{name}.project.i400"
    chroma = 2 * ((width + 1) // 2) * ((height + 1) // 2)
    luma = source_samples(width, height)
    source.write_bytes(luma + bytes([128]) * chroma)
    encoded = run(avm_command(avmenc, width, height, source, obu))
    avm = run([
        str(avmdec), "--codec=av2", "--rawvideo", "--limit=1",
        "--output-bit-depth=8", "-o", str(avm_raw), str(obu),
    ]) if encoded.returncode == 0 else None
    project = run([
        str(project_decoder), str(obu), "-o", str(project_raw)
    ]) if encoded.returncode == 0 else None
    avm_bytes = avm_raw.read_bytes() if avm_raw.is_file() else b""
    project_bytes = project_raw.read_bytes() if project_raw.is_file() else b""
    record = {
        "name": name,
        "width": width,
        "height": height,
        "encoder_returncode": encoded.returncode,
        "avmdec_returncode": None if avm is None else avm.returncode,
        "project_decoder_returncode": (
            None if project is None else project.returncode
        ),
        "project_decoder_stderr": (
            None if project is None else
            project.stderr.decode("utf-8", errors="replace").strip()
        ),
        "obu_bytes": obu.stat().st_size if obu.is_file() else 0,
        "obu_sha256": (
            hashlib.sha256(obu.read_bytes()).hexdigest()
            if obu.is_file() else None
        ),
        "validators_exact_equal": avm_bytes == project_bytes,
        "source_sha256": hashlib.sha256(luma).hexdigest(),
        "decoded_sample_count": len(avm_bytes),
        "model_exact_cover": model(width, height)["active_grid_exact_cover"],
        "model_dependency_safe": model(width, height)[
            "above_left_dependency_safe"
        ],
    }
    for path in (source, obu, avm_raw, project_raw):
        path.unlink(missing_ok=True)
    return record


def probes(
    output: Path, all_classes: bool, avmenc: Path, avmdec: Path,
    project_decoder: Path
) -> dict[str, object]:
    output.mkdir(parents=True, exist_ok=True)
    dimensions = (
        [(value, other) for value in range(24, 65, 8)
         for other in range(24, 65, 8)]
        if all_classes else
        list(CASES)
    )
    records = [
        probe_case(
            output, width, height, avmenc, avmdec, project_decoder
        )
        for width, height in dimensions
    ]
    return {
        "schema": "av2-m3-large-partition-probes-v1",
        "coverage": "all 6x6 eight-pixel-rounded frame-mi classes"
        if all_classes else "focused boundary representatives",
        "avm_revision": "966a7d7cd6fcf60360caf5dc413b2aeeb65e144d",
        "avmenc_sha256": hashlib.sha256(avmenc.read_bytes()).hexdigest(),
        "avmdec_sha256": hashlib.sha256(avmdec.read_bytes()).hexdigest(),
        "project_decoder_sha256": hashlib.sha256(
            project_decoder.read_bytes()
        ).hexdigest(),
        "cases": records,
        "all_pass": all(
            record["encoder_returncode"] == 0
            and record["avmdec_returncode"] == 0
            and record["project_decoder_returncode"] == 0
            and record["validators_exact_equal"]
            and record["decoded_sample_count"]
                == record["width"] * record["height"]
            for record in records
        ),
        "avm_all_pass": all(
            record["encoder_returncode"] == 0
            and record["avmdec_returncode"] == 0
            and record["decoded_sample_count"]
                == record["width"] * record["height"]
            for record in records
        ),
        "project_all_pass": all(
            record["project_decoder_returncode"] == 0
            and record["validators_exact_equal"]
            for record in records
        ),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--width", type=int, default=32)
    parser.add_argument("--height", type=int, default=32)
    parser.add_argument("--probe", action="store_true")
    parser.add_argument("--probe-all-classes", action="store_true")
    parser.add_argument("--reconstruction", action="store_true")
    parser.add_argument("--write-vectors", type=Path)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--avmenc", type=Path, default=DEFAULT_AVMENC)
    parser.add_argument("--avmdec", type=Path, default=DEFAULT_AVMDEC)
    parser.add_argument(
        "--project-decoder", type=Path, default=DEFAULT_PROJECT_DECODER
    )
    args = parser.parse_args()
    if args.write_vectors is not None:
        payload = {
            "schema": "av2-m3-multileaf-reconstruction-v1",
            "cases": [reconstruction_model(*case) for case in CASES],
        }
        args.write_vectors.write_text(
            json.dumps(payload, indent=2, sort_keys=True) + "\n"
        )
        print(args.write_vectors)
        return 0
    if args.probe or args.probe_all_classes:
        report = probes(
            args.output, args.probe_all_classes, args.avmenc, args.avmdec,
            args.project_decoder,
        )
        args.output.mkdir(parents=True, exist_ok=True)
        path = args.output / "report.json"
        path.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
        print(path)
        return 0 if report["all_pass"] else 1
    result = (
        reconstruction_model(args.width, args.height)
        if args.reconstruction else model(args.width, args.height)
    )
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
