#!/usr/bin/env python3
"""Generate and select pinned AVM references for the synthetic 10-bit matrix."""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any

from generate_10bit_avm_references import (
    coding_tool_profile,
    compare_frames,
    decode_command,
    encode_command,
    plane_dimensions,
    relative_command,
    require_tool,
    run,
    sha256_file,
    tool_banner,
)
from obu_carriage import parse_obus


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SOURCE_DIR = ROOT / "tests" / "fixtures" / "10bit"
DEFAULT_OUTPUT_DIR = DEFAULT_SOURCE_DIR / "avm-reference"
DEFAULT_ENCODER = ROOT / "build" / "reference" / "avm" / "avmenc"
DEFAULT_DECODER = ROOT / "build" / "reference" / "avm" / "avmdec"
DEFAULT_CENSUS = (
    ROOT / "build" / "reference" / "census-accounting" / "av2-census"
)
SWEEP_QPS = tuple(range(90, 161))
REPRESENTATIVE_QP = 128
FILTER_TERMS = ("filter", "cdef", "ccso", "restoration", "deblock", "loop")


def load_sources(source_directory: Path) -> tuple[Path, list[dict[str, Any]]]:
    manifest_path = source_directory / "source-manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if manifest.get("schema") != 1 or len(manifest.get("artifacts", [])) != 8:
        raise RuntimeError("synthetic source manifest must contain schema 1 matrix")
    return manifest_path, manifest["artifacts"]


def expected_size(source: dict[str, Any]) -> int:
    return 2 * sum(
        width * height
        for width, height in plane_dimensions(
            source["format"], source["width"], source["height"]
        )
    )


def coverage_atoms(census: dict[str, Any]) -> set[str]:
    atoms = {f"qband:{census['base_qindex'] // 16}"}
    for feature in census["features"]:
        atoms.update(
            {
                f"prediction:{feature['mode']['name']}",
                f"uv-prediction:{feature['uv_mode']['name']}",
                f"partition:{feature['partition']}",
                f"block:{feature['block_size']['name']}",
                f"transform-size:{feature['tx_size']['name']}",
                f"transform:{feature['primary_tx_type']['name']}",
                f"skip-transform:{feature['skip_transform']}",
                f"fsc:{feature['fsc_y']}:{feature['fsc_uv']}",
                f"intrabc:{feature['use_intrabc']}",
                f"palette:{feature['palette_y']}:{feature['palette_uv']}",
            }
        )
        if feature["uv_mode"]["name"] == "CFL":
            atoms.add(
                "cfl:"
                f"{feature['cfl_index']}:"
                f"{feature['cfl_alpha_index']}:"
                f"{feature['cfl_alpha_signs']}"
            )
    for symbol in census.get("accounting_symbols", []):
        identity = " ".join(
            [symbol["function"], symbol["file"], *symbol["tags"]]
        ).lower()
        if any(term in identity for term in FILTER_TERMS):
            atoms.add(
                f"filter:{symbol['function']}:"
                f"{','.join(symbol['tags'])}:"
                f"{symbol['minimum_value']}:{symbol['maximum_value']}"
            )
    return atoms


def select_qps(candidates: list[dict[str, Any]]) -> list[int]:
    if not candidates:
        return []
    ordered = sorted(candidates, key=lambda candidate: candidate["qp"])
    covered: set[str] = set()
    selected = []
    for candidate in ordered:
        atoms = set(candidate["coverage_atoms"])
        if not selected or atoms - covered:
            selected.append(candidate["qp"])
            covered.update(atoms)
    if selected[-1] != ordered[-1]["qp"]:
        selected.append(ordered[-1]["qp"])
    return selected


def lossless_encode_command(
    encoder: Path,
    source: Path,
    output: Path,
    pixel_format: str,
    width: int,
    height: int,
) -> list[str]:
    command = encode_command(
        encoder, source, output, pixel_format, width, height, 0
    )
    command[command.index("--end-usage=q")] = "--lossless=1"
    command.remove("--qp=0")
    return command


def run_census(census: Path, obus: list[Path], output: Path) -> list[dict[str, Any]]:
    command = [str(census), "-o", str(output), *(str(path) for path in obus)]
    completed = subprocess.run(
        command, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
    )
    if completed.returncode != 0:
        raise RuntimeError(
            f"census failed ({completed.returncode}): {completed.stderr.strip()}"
        )
    document = json.loads(output.read_text(encoding="utf-8"))
    if document.get("schema") != "av2-feature-census/v1":
        raise RuntimeError("unexpected census schema")
    return document["files"]


def encode_decode(
    encoder: Path,
    decoder: Path,
    source_path: Path,
    source: dict[str, Any],
    qp: int,
    directory: Path,
    lossless: bool = False,
) -> tuple[Path, Path]:
    stem = (
        f"{source_path.stem}-lossless"
        if lossless
        else f"{source_path.stem}-qp{qp:03d}"
    )
    obu = directory / f"{stem}.obu"
    decoded = directory / f"{stem}-decoded.yuv"
    run(
        lossless_encode_command(
            encoder,
            source_path,
            obu,
            source["format"],
            source["width"],
            source["height"],
        )
        if lossless
        else encode_command(
            encoder,
            source_path,
            obu,
            source["format"],
            source["width"],
            source["height"],
            qp,
        )
    )
    run(decode_command(decoder, obu, decoded))
    if decoded.stat().st_size != expected_size(source):
        raise RuntimeError(f"decoded size mismatch: {decoded.name}")
    return obu, decoded


def summarize_census(census: dict[str, Any]) -> dict[str, Any]:
    features = census["features"]
    filter_atoms = sorted(
        atom for atom in coverage_atoms(census) if atom.startswith("filter:")
    )
    return {
        "base_qindex": census["base_qindex"],
        "coding_blocks": census["coding_block_count"],
        "unique_features": census["unique_feature_count"],
        "prediction_modes": sorted({item["mode"]["name"] for item in features}),
        "uv_prediction_modes": sorted(
            {item["uv_mode"]["name"] for item in features}
        ),
        "transforms": sorted(
            {item["primary_tx_type"]["name"] for item in features}
        ),
        "transform_sizes": sorted({item["tx_size"]["name"] for item in features}),
        "qindices": sorted({item["qindex"] for item in features}),
        "cfl": sorted(
            {
                (
                    item["cfl_index"],
                    item["cfl_alpha_index"],
                    item["cfl_alpha_signs"],
                )
                for item in features
                if item["uv_mode"]["name"] == "CFL"
            }
        ),
        "filter_accounting": filter_atoms,
    }


def reference_record(
    source_path: Path,
    source: dict[str, Any],
    qp: int,
    obu: Path,
    decoded: Path,
    census: dict[str, Any],
    encoder: Path,
    decoder: Path,
    output_directory: Path,
    lossless: bool = False,
) -> dict[str, Any]:
    canonical_obu = output_directory / obu.name
    canonical_decoded = output_directory / decoded.name
    encode = lossless_encode_command(
        encoder,
        source_path,
        canonical_obu,
        source["format"],
        source["width"],
        source["height"],
    ) if lossless else encode_command(
        encoder, source_path, canonical_obu, source["format"],
        source["width"], source["height"], qp
    )
    decode = decode_command(decoder, canonical_obu, canonical_decoded)
    entries = parse_obus(obu.read_bytes())
    return {
        "name": obu.stem,
        "source": source_path.name,
        "source_sha256": source["sha256"],
        "pixel_format": source["format"],
        "width": source["width"],
        "height": source["height"],
        "bit_depth": 10,
        "qp": qp,
        "lossless": lossless,
        "obu": {
            "file": obu.name,
            "size_bytes": obu.stat().st_size,
            "sha256": sha256_file(obu),
            "units": [entry.ledger_entry() for entry in entries],
        },
        "decoded": {
            "file": decoded.name,
            "size_bytes": decoded.stat().st_size,
            "sha256": sha256_file(decoded),
            "planes": compare_frames(
                source_path,
                decoded,
                source["format"],
                source["width"],
                source["height"],
            ),
        },
        "census": summarize_census(census),
        "encode_command": relative_command(encode),
        "decode_command": relative_command(decode),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-dir", type=Path, default=DEFAULT_SOURCE_DIR)
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT_DIR)
    parser.add_argument("--encoder", type=Path, default=DEFAULT_ENCODER)
    parser.add_argument("--decoder", type=Path, default=DEFAULT_DECODER)
    parser.add_argument("--census", type=Path, default=DEFAULT_CENSUS)
    arguments = parser.parse_args()

    source_directory = arguments.source_dir.resolve()
    output_directory = arguments.output_dir.resolve()
    encoder = require_tool(arguments.encoder)
    decoder = require_tool(arguments.decoder)
    census_tool = require_tool(arguments.census)
    source_manifest, sources = load_sources(source_directory)
    for source in sources:
        source_path = source_directory / source["file"]
        if sha256_file(source_path) != source["sha256"]:
            raise RuntimeError(f"source hash mismatch: {source_path.name}")
        if source_path.stat().st_size != expected_size(source):
            raise RuntimeError(f"source size mismatch: {source_path.name}")

    output_directory.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(
        prefix="wp10-reference-", dir=output_directory.parent
    ) as temporary_text:
        temporary = Path(temporary_text)
        tiny_sources = [source for source in sources if source["name"] == "tiny-precision"]
        sweep_paths = []
        sweep_lookup: dict[str, tuple[dict[str, Any], int, Path, Path]] = {}
        for source in tiny_sources:
            source_path = source_directory / source["file"]
            for qp in SWEEP_QPS:
                obu, decoded = encode_decode(
                    encoder, decoder, source_path, source, qp, temporary
                )
                sweep_paths.append(obu)
                sweep_lookup[str(obu)] = (source, qp, obu, decoded)
        sweep_census = run_census(
            census_tool, sweep_paths, temporary / "sweep-census.json"
        )
        sweep_by_path = {item["path"]: item for item in sweep_census}

        sweep_report = []
        selected_by_format: dict[str, list[int]] = {}
        for source in tiny_sources:
            candidates = []
            for qp in SWEEP_QPS:
                obu = temporary / f"{Path(source['file']).stem}-qp{qp:03d}.obu"
                evidence = sweep_by_path[str(obu)]
                candidates.append(
                    {
                        "qp": qp,
                        "coverage_atoms": sorted(coverage_atoms(evidence)),
                        **summarize_census(evidence),
                    }
                )
            selected = select_qps(candidates)
            selected_by_format[source["format"]] = selected
            sweep_report.append(
                {
                    "source": source["file"],
                    "format": source["format"],
                    "tested_qps": list(SWEEP_QPS),
                    "selected_qps": selected,
                    "candidates": candidates,
                }
            )

        permanent: list[tuple[dict[str, Any], int, Path, Path, bool]] = []
        for source in sources:
            source_path = source_directory / source["file"]
            qps = (
                selected_by_format[source["format"]]
                if source["name"] == "tiny-precision"
                else [REPRESENTATIVE_QP]
            )
            for qp in qps:
                key = str(temporary / f"{source_path.stem}-qp{qp:03d}.obu")
                if key in sweep_lookup:
                    _, _, obu, decoded = sweep_lookup[key]
                else:
                    obu, decoded = encode_decode(
                        encoder, decoder, source_path, source, qp, temporary
                    )
                permanent.append((source, qp, obu, decoded, False))

        lossless_probes = []
        for source in tiny_sources:
            source_path = source_directory / source["file"]
            obu, decoded = encode_decode(
                encoder, decoder, source_path, source, 0, temporary,
                lossless=True,
            )
            exact = decoded.read_bytes() == source_path.read_bytes()
            lossless_probes.append(
                {
                    "format": source["format"],
                    "mode": "--lossless=1",
                    "decoded_equals_source": exact,
                    "source_sha256": source["sha256"],
                    "decoded_sha256": sha256_file(decoded),
                    "retained": exact,
                }
            )
            if exact:
                permanent.append((source, 0, obu, decoded, True))

        permanent_census = run_census(
            census_tool,
            [item[2] for item in permanent],
            temporary / "permanent-census.json",
        )
        census_by_path = {item["path"]: item for item in permanent_census}
        records = []
        for source, qp, obu, decoded, lossless in permanent:
            source_path = source_directory / source["file"]
            records.append(
                reference_record(
                    source_path,
                    source,
                    qp,
                    obu,
                    decoded,
                    census_by_path[str(obu)],
                    encoder,
                    decoder,
                    output_directory,
                    lossless,
                )
            )

        minimum_quantizer_probes = []
        for source in tiny_sources:
            source_path = source_directory / source["file"]
            obu, decoded = encode_decode(
                encoder, decoder, source_path, source, 0, temporary
            )
            minimum_quantizer_probes.append(
                {
                    "format": source["format"],
                    "qp": 0,
                    "decoded_equals_source": (
                        decoded.read_bytes() == source_path.read_bytes()
                    ),
                    "source_sha256": source["sha256"],
                    "decoded_sha256": sha256_file(decoded),
                    "retained": False,
                    "reason": "QP 0 reconstruction is not byte-exact",
                }
            )

        coverage_report = {
            "schema": 1,
            "description": "WP10 AVM QP sweep and census coverage selection",
            "selection_rule": (
                "In ascending QP order, retain the first candidate and each "
                "candidate adding a quantizer-band or syntax coverage atom; "
                "always retain QP 160."
            ),
            "quantizer_band": "floor(base_qindex / 16)",
            "filter_accounting_terms": list(FILTER_TERMS),
            "sweeps": sweep_report,
            "lossless_probes": lossless_probes,
            "minimum_quantizer_probes": minimum_quantizer_probes,
            "permanent": [
                {"name": record["name"], **record["census"]} for record in records
            ],
        }
        (temporary / "coverage-report.json").write_text(
            json.dumps(coverage_report, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )

        revision = run(
            ["git", "-C", str(ROOT / "vendor" / "avm"), "rev-parse", "HEAD"]
        ).strip()
        manifest = {
            "schema": 1,
            "description": "Pinned AVM synthetic 10-bit references",
            "generator": {
                "file": "tools/generate_10bit_synthetic_references.py",
                "sha256": sha256_file(Path(__file__).resolve()),
                "command": [
                    "python3",
                    "tools/generate_10bit_synthetic_references.py",
                ],
            },
            "source_manifest": {
                "file": str(source_manifest.relative_to(ROOT)),
                "sha256": sha256_file(source_manifest),
            },
            "coverage_report": {
                "file": "coverage-report.json",
                "sha256": sha256_file(temporary / "coverage-report.json"),
            },
            "coding_tool_profile": coding_tool_profile(),
            "output_contract": {
                "storage": "headerless planar unsigned 16-bit little-endian words",
                "significant_bits": 10,
                "sample_range": [0, 1023],
                "plane_order": ["Y", "U", "V"],
            },
            "tools": {
                "avm_revision": revision,
                "avmenc": tool_banner(encoder, "AOMedia Project AV2 Encoder"),
                "avmenc_sha256": sha256_file(encoder),
                "avmdec": tool_banner(decoder, "AOMedia Project AV2 Decoder"),
                "avmdec_sha256": sha256_file(decoder),
                "av2_census_sha256": sha256_file(census_tool),
            },
            "artifacts": sorted(records, key=lambda record: record["name"]),
        }
        (temporary / "manifest.json").write_text(
            json.dumps(manifest, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )

        if output_directory.exists():
            shutil.rmtree(output_directory)
        output_directory.mkdir()
        for source, qp, obu, decoded, lossless in permanent:
            del source, qp, lossless
            shutil.copyfile(obu, output_directory / obu.name)
            shutil.copyfile(decoded, output_directory / decoded.name)
        for name in ("coverage-report.json", "manifest.json"):
            shutil.copyfile(temporary / name, output_directory / name)

    print(
        f"generated {len(records)} selected references in {output_directory}",
        flush=True,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())