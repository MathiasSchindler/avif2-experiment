#!/usr/bin/env python3
"""Reproduce the constrained WP0 10-bit OBU and decoded oracle."""

from __future__ import annotations

import argparse
import json
import subprocess
import tempfile
from pathlib import Path
from typing import Any

from check_10bit_wp0_oracle import (
    DEFAULT_ORACLE,
    FIXTURE_ROOT,
    first_mismatch,
    read_oracle,
    validate_oracle,
)


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_ENCODER = ROOT / "build" / "reference" / "avm" / "avmenc"
DEFAULT_DECODER = ROOT / "build" / "reference" / "avm" / "avmdec"
DEFAULT_CENSUS = (
    ROOT / "build" / "reference" / "census-accounting" / "av2-census"
)

DISABLED_ARGUMENTS = (
    "--film-grain-test=0",
    "--enable-lcr=0",
    "--enable-operating-point-sets=0",
    "--enable-atlas=0",
    "--enable-restoration=0",
    "--enable-deblocking=0",
    "--enable-cdef=0",
    "--enable-ccso=0",
    "--enable-gdf=0",
    "--enable-palette=0",
    "--enable-ibp=0",
    "--enable-intrabc=0",
    "--enable-cfl-intra=0",
    "--enable-mhccp=0",
    "--enable-cctx=0",
    "--enable-mrls=0",
    "--enable-fsc=0",
    "--enable-ist=0",
    "--enable-idtx-intra=0",
    "--enable-flip-idtx=0",
    "--enable-paeth-intra=0",
    "--enable-smooth-intra=0",
    "--use-intra-dct-only=1",
)


def encode_command(encoder: Path, source: Path, output: Path) -> list[str]:
    return [
        str(encoder),
        "--debug", "--codec=av2", "--obu", "--passes=1", "--limit=1",
        "--good", "--cpu-used=9", "--threads=1", "--profile=0",
        "--width=16", "--height=16", "--i420", "--bit-depth=10",
        "--input-bit-depth=10", "--input-chroma-subsampling-x=1",
        "--input-chroma-subsampling-y=1",
        "--chroma-sample-position=unspecified", "--force-video-mode=0",
        *DISABLED_ARGUMENTS,
        "--end-usage=q", "--qp=208", "--test-decode=fatal", "--quiet",
        "-o", str(output), str(source),
    ]


def decode_command(decoder: Path, source: Path, output: Path) -> list[str]:
    return [
        str(decoder), "--codec=av2", "--rawvideo", "--output-bit-depth=10",
        "--limit=1", "--threads=1", "-o", str(output), str(source),
    ]


def require_file(path: Path, label: str) -> Path:
    if not path.is_file():
        raise RuntimeError(f"missing {label}: {path}")
    return path.resolve()


def run(command: list[str]) -> None:
    subprocess.run(command, cwd=ROOT, check=True)


def verify_trace(oracle: dict[str, Any], census_path: Path) -> None:
    document = json.loads(census_path.read_text(encoding="ascii"))
    if document.get("avm_revision") != oracle["generation"]["avm_revision"]:
        raise RuntimeError("WP0 census AVM revision differs from the oracle")
    files = document.get("files")
    if not isinstance(files, list) or len(files) != 1:
        raise RuntimeError("WP0 census must contain exactly one file")
    record = files[0]
    trace = oracle["trace_evidence"]
    census = oracle["census"]
    if (
        record.get("base_qindex") != census["base_qindex"]
        or record.get("coding_block_count") != census["coding_blocks"]
        or record.get("frame_header_bits") != trace["frame_header_bits"]
        or record.get("tile_entropy_tell") != trace["tile_entropy_tell"]
        or record.get("filters") != trace["filters"]
        or len(record.get("accounting_trace", [])) != census["accounting_events"]
    ):
        raise RuntimeError("WP0 regenerated census differs from the oracle")
    features = record.get("features", [])
    observed = {
        "prediction_modes": sorted({item["mode"]["name"] for item in features}),
        "uv_prediction_modes": sorted(
            {item["uv_mode"]["name"] for item in features}
        ),
        "transforms": sorted(
            {item["primary_tx_type"]["name"] for item in features}
        ),
        "transform_sizes": sorted({item["tx_size"]["name"] for item in features}),
        "qindices": sorted({item["qindex"] for item in features}),
    }
    for key, value in observed.items():
        if value != census[key]:
            raise RuntimeError(f"WP0 regenerated {key} differs from the oracle")
    forbidden_fields = (
        "enable_ibp", "use_intrabc", "fsc_y", "fsc_uv", "mrl_index",
        "multi_line_mrl", "secondary_tx_type", "palette_y", "palette_uv",
        "cfl_index", "cfl_alpha_index", "cfl_alpha_signs",
    )
    if any(item[field] != 0 for item in features for field in forbidden_fields):
        raise RuntimeError("WP0 regenerated fixture reaches a disabled sample tool")
    block_order = [
        {
            "mi_row": block["mi_row"],
            "mi_column": block["mi_column"],
            "chroma_mi_row": block["chroma_mi_row"],
            "chroma_mi_column": block["chroma_mi_column"],
            "luma_block_size_id": block["luma_block_size"],
            "chroma_block_size_id": block["chroma_block_size"],
            "tree_type": block["tree_type"],
        }
        for block in record.get("block_trace", [])
    ]
    if block_order != trace["block_order"]:
        raise RuntimeError("WP0 regenerated block order differs from the oracle")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--oracle", type=Path, default=DEFAULT_ORACLE)
    parser.add_argument("--encoder", type=Path, default=DEFAULT_ENCODER)
    parser.add_argument("--decoder", type=Path, default=DEFAULT_DECODER)
    parser.add_argument("--census", type=Path, default=DEFAULT_CENSUS)
    arguments = parser.parse_args()
    oracle = read_oracle(arguments.oracle)
    validate_oracle(oracle, FIXTURE_ROOT)
    encoder = require_file(arguments.encoder, "pinned AVM encoder")
    decoder = require_file(arguments.decoder, "pinned AVM decoder")
    census = require_file(arguments.census, "accounting census")
    source = require_file(
        FIXTURE_ROOT / oracle["generation"]["source_file"], "10-bit source"
    )
    expected_obu = (FIXTURE_ROOT / oracle["obu"]["file"]).read_bytes()
    expected_decoded = (FIXTURE_ROOT / oracle["decoded"]["file"]).read_bytes()
    with tempfile.TemporaryDirectory(prefix="av2h-10bit-wp0-") as temporary:
        directory = Path(temporary)
        obu = directory / "fixture.obu"
        decoded = directory / "decoded.yuv"
        census_json = directory / "census.json"
        run(encode_command(encoder, source, obu))
        run(decode_command(decoder, obu, decoded))
        run([
            str(census), "--trace-accounting", "--trace-blocks",
            "-o", str(census_json), str(obu),
        ])
        if obu.read_bytes() != expected_obu:
            raise RuntimeError("WP0 regenerated OBU differs from the oracle")
        mismatch = first_mismatch(oracle, expected_decoded, decoded.read_bytes())
        if mismatch is not None:
            raise RuntimeError(f"WP0 regenerated decoded output differs: {mismatch}")
        verify_trace(oracle, census_json)
    print(f"WP0 oracle reproducible: {oracle['fixture']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())