#!/usr/bin/env python3
"""Gate the experimental production `--search rd` surface against legacy searches."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import re
import shutil
import statistics
import subprocess
import time
from pathlib import Path
from typing import Any, Sequence


ROOT = Path(__file__).resolve().parents[3]
ENCODER = ROOT / "experimental" / "encoder"
DEFAULT_ENCODER = ENCODER / "build" / "av2-encoder-experiment"
DEFAULT_AVMDEC = ROOT / "build" / "reference" / "avm" / "avmdec"
DEFAULT_PROJECT = ROOT / "build" / "host-cmake" / "av2heif-decoder"
DEFAULT_OUTPUT = ENCODER / "metrics" / "blocky-m1-gate.json"
DEFAULT_ARTIFACTS = ENCODER / "metrics" / "blocky-m1-gate"
DEFAULT_WORK = ENCODER / "build" / "blocky-m1-gate"
PREPARED_LOCK = ROOT / "images" / "recipes" / "prepared-lock.json"
PLANE_QDELTA_BLOCKS = ENCODER / "fixtures" / "m3-plane-qdelta-blocks.json"
PREPARED_CROP_FIXTURE = (
    ENCODER / "fixtures" / "blocky-m1-corpus-frontier" / "manifest.json"
)

RD_SEARCH = "rd"
LEGACY_SEARCHES = (
    "off",
    "bounded",
    "directional",
    "directional-exact",
    "directional-full",
    "directional-full-exact",
    "intra",
    "plane-qdelta",
)
LEGACY_ORDER = {name: index for index, name in enumerate(LEGACY_SEARCHES)}
FORMATS = ("i400", "i420", "i444")
REAL_SUBSETS = {"prepared-corpus", "baya-weaver"}
REQUIRED_TARGET_CATEGORIES = {"dc", "skip", "directional", "smooth", "paeth", "ac"}
SUBSET_ORDER = {name: index for index, name in enumerate(
    ("targeted", "ceiling", "baya-weaver", "prepared-corpus"))}
MODE_FIELDS = ("luma_mode", "intra_mode")
CLI_MODE_CATEGORY = {
    0: "dc",
    3: "directional",
    4: "directional",
    5: "smooth",
    6: "smooth",
    7: "smooth",
    8: "paeth",
}
TRUTHY = {"1", "true", "True", "TRUE", "yes", "YES"}
QDELTA_PRESERVE_KEYS = (
    "rd_qdelta_y_preserved",
    "qdelta_y_preserved",
    "qdelta_y_invariant",
    "y_preserved",
    "y_preserve",
    "y_unchanged_across_qdelta_trials",
)
OUTPUT_NAMES = {
    "i400": "gray8",
    "i420": "yuv420p8",
    "i444": "yuv444p8",
}
METRIC_RE = re.compile(
    rb"encoder: bytes=(\d+) sse_y=(\d+)"
    rb"(?: sse_u=(\d+) sse_v=(\d+))?"
    rb"(?: psnr_y=(?:inf|\d+\.\d+)dB)?"
    rb" workspace_bytes=(\d+) qindex=(\d+)"
)
TOKEN_RE = re.compile(r"([A-Za-z_][A-Za-z0-9_]*)=([^\s]+)")


class GateError(RuntimeError):
    pass


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        while chunk := handle.read(1024 * 1024):
            digest.update(chunk)
    return digest.hexdigest()


def run(command: Sequence[str]) -> tuple[subprocess.CompletedProcess[bytes], int]:
    environment = os.environ.copy()
    environment.update({"LC_ALL": "C", "LANG": "C"})
    started = time.monotonic_ns()
    completed = subprocess.run(
        command,
        cwd=ROOT,
        env=environment,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    return completed, time.monotonic_ns() - started


def executable(path: Path, label: str) -> Path:
    if not path.is_file() or not os.access(path, os.X_OK):
        raise GateError(f"{label} is not executable: {path}")
    return path.resolve()


def plane_geometry(pixel_format: str, width: int, height: int) -> list[tuple[int, int]]:
    if pixel_format == "i400":
        return [(width, height)]
    if pixel_format == "i420":
        chroma = ((width + 1) // 2, (height + 1) // 2)
        return [(width, height), chroma, chroma]
    if pixel_format == "i444":
        return [(width, height)] * 3
    raise GateError(f"unsupported pixel format: {pixel_format}")


def split_planes(data: bytes, pixel_format: str, width: int, height: int) -> list[bytes]:
    planes = []
    offset = 0
    for plane_width, plane_height in plane_geometry(pixel_format, width, height):
        size = plane_width * plane_height
        planes.append(data[offset:offset + size])
        offset += size
    if offset != len(data):
        raise GateError(
            f"{pixel_format} planar size mismatch: expected {offset}, got {len(data)}"
        )
    return planes


def psnr(sse: int, samples: int) -> str:
    if sse == 0:
        return "inf"
    return f"{10.0 * math.log10(65025.0 * samples / sse):.6f}"


def total_sse(result: dict[str, Any]) -> int:
    return int(sum(int(value) for value in result.get("reported_sse", [])))


def plane_sse(result: dict[str, Any]) -> list[int]:
    return [int(value) for value in result.get("reported_sse", [])]


def metric_tokens(stderr: str) -> dict[str, str]:
    return {name: value for name, value in TOKEN_RE.findall(stderr)}


def parse_bool_token(tokens: dict[str, str], keys: Sequence[str]) -> bool | None:
    for key in keys:
        if key in tokens:
            return tokens[key] in TRUTHY
    return None


def cli_mode_category(value: int | None) -> str | None:
    if value is None:
        return None
    if value in CLI_MODE_CATEGORY:
        return CLI_MODE_CATEGORY[value]
    if value > 8:
        return "directional"
    return None


def search_category(search: str, tokens: dict[str, str]) -> str | None:
    if search == "off":
        return "skip"
    if search == "bounded":
        return "ac"
    if search == "plane-qdelta":
        return "plane-qdelta"
    for field in MODE_FIELDS:
        if field in tokens:
            try:
                value = int(tokens[field])
            except ValueError:
                continue
            return cli_mode_category(value)
    if search.startswith("directional"):
        return "directional"
    return None


def rd_winner_categories(tokens: dict[str, str]) -> list[str]:
    categories: set[str] = set()
    fields = {
        "dc": ("rd_mode_dc",),
        "directional": ("rd_mode_vertical", "rd_mode_horizontal"),
        "smooth": ("rd_mode_smooth", "rd_mode_smooth_v", "rd_mode_smooth_h"),
        "paeth": ("rd_mode_paeth",),
        "skip": ("rd_residual_skip",),
        "ac": ("rd_residual_ac",),
    }
    for category, names in fields.items():
        for name in names:
            try:
                if int(tokens.get(name, "0")) > 0:
                    categories.add(category)
                    break
            except ValueError:
                continue
    return sorted(categories)


def rd_emitted_candidate(
    pixel_format: str, searches: dict[str, dict[str, Any]],
) -> bool:
    rd = searches.get(RD_SEARCH, {})
    incumbent_name = "bounded" if pixel_format == "i400" else "off"
    incumbent = searches.get(incumbent_name, {})
    if rd.get("status") != "pass" or incumbent.get("status") != "pass":
        return False
    return (
        rd.get("obu_sha256") != incumbent.get("obu_sha256")
        or int(rd.get("obu_bytes", -1)) != int(incumbent.get("obu_bytes", -1))
        or plane_sse(rd) != plane_sse(incumbent)
    )


def supports_rd_in_help(help_text: str) -> bool:
    return re.search(r"--search[^\n]*\brd\b", help_text) is not None


def display_path(path: Path) -> str:
    try:
        return str(path.relative_to(ROOT))
    except ValueError:
        return str(path)


def rd_unavailable_message(help_text: str, probe_stderr: str) -> str:
    if probe_stderr:
        return probe_stderr.strip()
    if help_text:
        return help_text.strip().splitlines()[-1]
    return "encoder did not advertise or accept --search rd"


def probe_rd_support(encoder: Path, work_dir: Path) -> dict[str, Any]:
    help_run, help_time = run([str(encoder), "--help"])
    help_text = help_run.stdout.decode("utf-8", "replace") + \
        help_run.stderr.decode("utf-8", "replace")
    probe_dir = work_dir / "probe"
    probe_dir.mkdir(parents=True, exist_ok=True)
    source = probe_dir / "probe.i400"
    source.write_bytes(bytes([128]) * 64)
    output = probe_dir / "probe.obu"
    probe_command = [
        str(encoder), str(source), "--width", "8", "--height", "8",
        "--qindex", "100", "--search", RD_SEARCH, "--output", str(output),
    ]
    probe_run, probe_time = run(probe_command)
    stderr = probe_run.stderr.decode("utf-8", "replace")
    available = probe_run.returncode == 0 and output.is_file()
    return {
        "help_returncode": help_run.returncode,
        "help_time_ns": help_time,
        "help_mentions_rd": supports_rd_in_help(help_text),
        "help_excerpt": next(
            (line for line in help_text.splitlines() if "--search" in line),
            "",
        ),
        "command": probe_command,
        "probe_returncode": probe_run.returncode,
        "probe_time_ns": probe_time,
        "probe_stderr": stderr.strip(),
        "available": available,
        "reason": None if available else rd_unavailable_message(help_text, stderr),
    }


def legacy_searches_for_case(pixel_format: str) -> tuple[str, ...]:
    if pixel_format == "i400":
        return tuple(search for search in LEGACY_SEARCHES if search != "plane-qdelta")
    return LEGACY_SEARCHES


def dominates(left: dict[str, Any], right: dict[str, Any]) -> bool:
    if left["obu_bytes"] > right["obu_bytes"]:
        return False
    left_sse = plane_sse(left)
    right_sse = plane_sse(right)
    if len(left_sse) != len(right_sse):
        return False
    if any(l > r for l, r in zip(left_sse, right_sse)):
        return False
    return left["obu_bytes"] < right["obu_bytes"] or any(
        l < r for l, r in zip(left_sse, right_sse)
    )


def same_metrics(left: dict[str, Any], right: dict[str, Any]) -> bool:
    return (
        left["obu_bytes"] == right["obu_bytes"]
        and plane_sse(left) == plane_sse(right)
    )


def frontier(results: list[dict[str, Any]]) -> list[dict[str, Any]]:
    ordered = sorted(
        [result for result in results if result.get("status") == "pass"],
        key=lambda result: (
            int(result["obu_bytes"]),
            tuple(plane_sse(result)),
            LEGACY_ORDER[result["search"]],
        ),
    )
    retained: list[dict[str, Any]] = []
    for result in ordered:
        if any(
            dominates(other, result)
            or (
                same_metrics(other, result)
                and LEGACY_ORDER[other["search"]] < LEGACY_ORDER[result["search"]]
            )
            for other in retained
        ):
            continue
        retained = [other for other in retained if not dominates(result, other)]
        retained.append(result)
    return retained


def select_rate_ceiling_baseline(
    legacy_results: list[dict[str, Any]], rd_result: dict[str, Any],
    require_validation: bool = False,
) -> tuple[dict[str, Any] | None, list[dict[str, Any]]]:
    eligible = [
        result
        for result in legacy_results
        if (
            not require_validation
            or (
                result.get("validated")
                and result.get("avm_project_exact")
                and result.get("local_sse_matches_decode")
            )
        )
    ]
    candidates = [
        result for result in frontier(eligible)
        if int(result["obu_bytes"]) >= int(rd_result["obu_bytes"])
    ]
    if not candidates:
        return None, []
    best = min(
        candidates,
        key=lambda result: (
            total_sse(result),
            int(result["obu_bytes"]),
            LEGACY_ORDER[result["search"]],
        ),
    )
    return best, candidates


def plane_records_from_sse(
    pixel_format: str, width: int, height: int, sse_values: Sequence[int],
) -> list[dict[str, Any]]:
    result = []
    for name, value, (plane_width, plane_height) in zip(
        ("y", "u", "v"),
        sse_values,
        plane_geometry(pixel_format, width, height),
    ):
        samples = plane_width * plane_height
        result.append({
            "plane": name,
            "width": plane_width,
            "height": plane_height,
            "samples": samples,
            "reported_sse": int(value),
            "reported_psnr_db": psnr(int(value), samples),
        })
    return result


def decode_output(
    obu: Path, pixel_format: str, avmdec: Path, project: Path, directory: Path,
) -> tuple[bytes, bytes, dict[str, int]]:
    avm_path = directory / f"decoded.avm.{pixel_format}"
    project_path = directory / f"decoded.project.{pixel_format}"
    durations: dict[str, int] = {}
    for name, command in (
        (
            "avm",
            [
                str(avmdec), "--codec=av2", "--rawvideo", "--limit=1",
                "--output-bit-depth=8", "-o", str(avm_path), str(obu),
            ],
        ),
        ("project", [str(project), str(obu), "-o", str(project_path)]),
    ):
        completed, duration = run(command)
        durations[name] = duration
        if completed.returncode != 0:
            raise GateError(
                f"{name} decoder rejected {obu.name}: "
                f"{completed.stderr.decode('utf-8', 'backslashreplace')}"
            )
    avm = avm_path.read_bytes()
    project_data = project_path.read_bytes()
    return avm, project_data, durations


def validate_result(
    case: dict[str, Any],
    result: dict[str, Any],
    source: bytes,
    avmdec: Path,
    project: Path,
) -> dict[str, Any]:
    if result.get("status") != "pass" or result.get("validated"):
        return result
    obu = Path(result["primary_output_path"])
    avm, project_data, durations = decode_output(
        obu, case["pixel_format"], avmdec, project, Path(result["output_dir"]),
    )
    result["decode_time_ns"] = durations
    result["decoded_sha256"] = sha256_bytes(avm)
    result["avm_sha256"] = sha256_bytes(avm)
    result["project_sha256"] = sha256_bytes(project_data)
    result["avm_project_exact"] = avm == project_data
    decoded_planes = split_planes(avm, case["pixel_format"], case["width"], case["height"])
    source_planes = split_planes(source, case["pixel_format"], case["width"], case["height"])
    decoded_sse = []
    for record, source_plane, decoded_plane in zip(
        result["planes"], source_planes, decoded_planes,
    ):
        sse = sum(
            (int(left) - int(right)) ** 2
            for left, right in zip(source_plane, decoded_plane)
        )
        decoded_sse.append(sse)
        record["decoded_sse"] = sse
        record["decoded_psnr_db"] = psnr(sse, int(record["samples"]))
        record["source_sha256"] = sha256_bytes(source_plane)
        record["decoded_sha256"] = sha256_bytes(decoded_plane)
    result["local_sse_matches_decode"] = decoded_sse == plane_sse(result)
    result["validated"] = True
    return result


def case_output_key(case: dict[str, Any]) -> tuple[int, str]:
    return (SUBSET_ORDER[case["subset"]], str(case["id"]))


def neutral_plane(width: int, height: int) -> bytes:
    return bytes([128]) * (width * height)


def make_planar_source(
    pixel_format: str,
    width: int,
    height: int,
    luma: bytes,
    chroma_u: bytes | None = None,
    chroma_v: bytes | None = None,
) -> bytes:
    planes = [luma]
    for plane_index, (plane_width, plane_height) in enumerate(
        plane_geometry(pixel_format, width, height)[1:],
        1,
    ):
        if plane_index == 1 and chroma_u is not None:
            planes.append(chroma_u)
        elif plane_index == 2 and chroma_v is not None:
            planes.append(chroma_v)
        else:
            planes.append(neutral_plane(plane_width, plane_height))
    return b"".join(planes)


def flat_luma(width: int, height: int, value: int = 128) -> bytes:
    return bytes([value]) * (width * height)


def horizontal_edge(width: int, height: int) -> bytes:
    return bytes(
        32 if y < height // 2 else 224
        for y in range(height) for _x in range(width)
    )


def vertical_edge(width: int, height: int) -> bytes:
    return bytes(
        32 if x < width // 2 else 224
        for _y in range(height) for x in range(width)
    )


def checkerboard(width: int, height: int, block: int = 4) -> bytes:
    return bytes(
        48 if ((x // block) ^ (y // block)) & 1 else 208
        for y in range(height) for x in range(width)
    )


def smooth_center(width: int, height: int, divisor: int) -> bytes:
    return bytes(
        min(255, ((x - width // 2) ** 2 + (y - height // 2) ** 2) // divisor)
        for y in range(height) for x in range(width)
    )


def tiled_plane(values: list[int], source_size: int, target_size: int) -> bytes:
    return bytes(
        values[(y % source_size) * source_size + x % source_size]
        for y in range(target_size) for x in range(target_size)
    )


def case_record(
    *,
    case_id: str,
    subset: str,
    pixel_format: str,
    width: int,
    height: int,
    qindex: int,
    tags: Sequence[str],
    expected_categories: Sequence[str],
    source: bytes | None = None,
    source_path: Path | None = None,
    source_sha256: str | None = None,
    origin: dict[str, Any] | None = None,
) -> dict[str, Any]:
    if source is None and source_path is None:
        raise GateError("case requires inline source bytes or a source path")
    if source is not None:
        computed_sha = sha256_bytes(source)
        source_bytes = len(source)
    else:
        assert source_path is not None
        computed_sha = source_sha256 or sha256_file(source_path)
        source_bytes = int(source_path.stat().st_size)
    return {
        "id": case_id,
        "subset": subset,
        "pixel_format": pixel_format,
        "width": width,
        "height": height,
        "qindex": qindex,
        "tags": sorted(set(tags)),
        "expected_categories": sorted(set(expected_categories)),
        "source": source,
        "source_path": str(source_path.resolve()) if source_path else None,
        "source_sha256": computed_sha,
        "source_bytes": source_bytes,
        "origin": origin or {},
    }


def targeted_cases() -> tuple[list[dict[str, Any]], dict[str, Any]]:
    blocks = json.loads(PLANE_QDELTA_BLOCKS.read_text(encoding="utf-8"))["blocks"]
    cases = []
    for pixel_format in FORMATS:
        width = 9
        height = 15
        source = make_planar_source(
            pixel_format,
            width,
            height,
            flat_luma(width, height),
        )
        cases.append(case_record(
            case_id=f"skip-flat-odd-{pixel_format}-q100",
            subset="targeted",
            pixel_format=pixel_format,
            width=width,
            height=height,
            qindex=100,
            tags=("odd-dimension", "flat"),
            expected_categories=("skip", "dc"),
            source=source,
            origin={"generator": "flat", "value": 128},
        ))
    width = height = 64
    cases.extend([
        case_record(
            case_id="directional-horizontal-edge-i400-q60",
            subset="targeted",
            pixel_format="i400",
            width=width,
            height=height,
            qindex=60,
            tags=("edge",),
            expected_categories=("directional",),
            source=horizontal_edge(width, height),
            origin={"generator": "horizontal-edge"},
        ),
        case_record(
            case_id="paeth-vertical-edge-i400-q60",
            subset="targeted",
            pixel_format="i400",
            width=width,
            height=height,
            qindex=60,
            tags=("edge",),
            expected_categories=("paeth",),
            source=vertical_edge(width, height),
            origin={"generator": "vertical-edge"},
        ),
        case_record(
            case_id="ac-checker-i400-q100",
            subset="targeted",
            pixel_format="i400",
            width=width,
            height=height,
            qindex=100,
            tags=("texture",),
            expected_categories=("ac",),
            source=checkerboard(width, height),
            origin={"generator": "checkerboard-4"},
        ),
        case_record(
            case_id="smooth-strong-center-i400-q60",
            subset="targeted",
            pixel_format="i400",
            width=width,
            height=height,
            qindex=60,
            tags=("smooth",),
            expected_categories=("smooth",),
            source=smooth_center(width, height, 4),
            origin={"generator": "strong-center", "divisor": 4},
        ),
    ])
    for pixel_format, chroma_size, block_name in (
        ("i420", 32, "color-gradient-4"),
        ("i444", 64, "color-gradient-8"),
    ):
        block = blocks[block_name]
        luma = flat_luma(64, 64)
        source = make_planar_source(
            pixel_format,
            64,
            64,
            luma,
            tiled_plane(block["u"], block["size"], chroma_size),
            tiled_plane(block["v"], block["size"], chroma_size),
        )
        cases.append(case_record(
            case_id=f"qdelta-tiled-gradient-{pixel_format}-q141",
            subset="targeted",
            pixel_format=pixel_format,
            width=64,
            height=64,
            qindex=141,
            tags=("color", "qdelta", "flat-luma"),
            expected_categories=("plane-qdelta",),
            source=source,
            origin={
                "generator": "plane-qdelta-gradient",
                "blocks_fixture": str(PLANE_QDELTA_BLOCKS.relative_to(ROOT)),
                "block_name": block_name,
            },
        ))
    return sorted(cases, key=case_output_key), {
        "plane_qdelta_blocks_sha256": sha256_file(PLANE_QDELTA_BLOCKS),
        "case_count": len(cases),
    }


def ceiling_cases() -> tuple[list[dict[str, Any]], dict[str, Any]]:
    width = height = 1440
    source = smooth_center(width, height, 4096)
    case = case_record(
        case_id="ceiling-smooth-center-i400-1440x1440-q100",
        subset="ceiling",
        pixel_format="i400",
        width=width,
        height=height,
        qindex=100,
        tags=("ceiling", "multiple-superblocks", "smooth"),
        expected_categories=("smooth",),
        source=source,
        origin={"generator": "center-squared", "divisor": 4096},
    )
    return [case], {"case_count": 1}


def prepared_lock_data() -> dict[str, Any]:
    return json.loads(PREPARED_LOCK.read_text(encoding="utf-8"))


def supporting_prepared_crop_evidence() -> dict[str, Any]:
    manifest = json.loads(PREPARED_CROP_FIXTURE.read_text(encoding="utf-8"))
    fixture_dir = PREPARED_CROP_FIXTURE.parent
    for key in ("bounded", "rd"):
        record = manifest["searches"][key]
        path = fixture_dir / record["file"]
        if sha256_file(path) != record["sha256"]:
            raise GateError(f"locked prepared crop stream hash mismatch: {path}")
    source = fixture_dir / "source.i400"
    if sha256_file(source) != manifest["crop"]["sha256"]:
        raise GateError(f"locked prepared crop source hash mismatch: {source}")
    return {
        "manifest": display_path(PREPARED_CROP_FIXTURE),
        "manifest_sha256": sha256_file(PREPARED_CROP_FIXTURE),
        "id": manifest["id"],
        "baseline_search": manifest["comparison"]["baseline_search"],
        "strict_pareto_win": bool(manifest["comparison"]["strict_pareto_win"]),
        "bounded": manifest["searches"]["bounded"],
        "rd": manifest["searches"]["rd"],
        "satisfies_m1_exit": False,
        "reason": (
            "This locked real-image crop supports candidate reachability and "
            "validator parity, but M1.6 requires the full prepared corpus and "
            "the admission gate requires a strict full prepared-image Pareto win."
        ),
    }


def selected_prepared_records(lock: dict[str, Any], selection: str) -> list[dict[str, Any]]:
    records = list(lock["preparations"])
    if selection in {"le896", "le1440"}:
        return sorted(
            [record for record in records if record["preparation_id"] == selection],
            key=lambda record: record["source_id"],
        )
    best: dict[str, tuple[int, str]] = {}
    chosen: dict[str, dict[str, Any]] = {}
    for record in sorted(records, key=lambda item: (item["source_id"], item["preparation_id"])):
        width, height = map(int, record["dimensions"])
        score = (width * height, record["preparation_id"])
        previous = best.get(record["source_id"])
        if previous is None or score < previous:
            best[record["source_id"]] = score
            chosen[record["source_id"]] = record
    return [chosen[source_id] for source_id in sorted(chosen)]


def prepared_corpus_cases(
    selection: str, limit: int | None,
) -> tuple[list[dict[str, Any]], dict[str, Any]]:
    lock = prepared_lock_data()
    records = selected_prepared_records(lock, selection)
    if limit is not None:
        records = records[:limit]
    cases = []
    hashes = {"prepared_lock_sha256": sha256_file(PREPARED_LOCK), "selection": selection}
    for record in records:
        output = record["outputs"][OUTPUT_NAMES["i400"]]
        path = ROOT / "images" / "prepared" / record["preparation_id"] / \
            record["source_id"] / output["filename"]
        hashes[str(path.relative_to(ROOT))] = output["sha256"]
        cases.append(case_record(
            case_id=(
                f"prepared-{record['source_id']}-{record['preparation_id']}"
                f"-i400-q100"
            ),
            subset="prepared-corpus",
            pixel_format="i400",
            width=int(record["dimensions"][0]),
            height=int(record["dimensions"][1]),
            qindex=100,
            tags=("multiple-superblocks", "prepared-full"),
            expected_categories=(),
            source_path=path,
            source_sha256=output["sha256"],
            origin={
                "source_id": record["source_id"],
                "preparation_id": record["preparation_id"],
                "manifest": record["manifest"],
                "manifest_sha256": record["manifest_sha256"],
            },
        ))
    return sorted(cases, key=case_output_key), {
        "prepared_lock_sha256": sha256_file(PREPARED_LOCK),
        "selected_count": len(cases),
        "selection": selection,
        "sources": hashes,
    }


def baya_weaver_cases() -> tuple[list[dict[str, Any]], dict[str, Any]]:
    lock = prepared_lock_data()
    record = next(
        item for item in lock["preparations"]
        if item["source_id"] == "baya-weaver"
        and item["preparation_id"] == "le1440"
    )
    cases = []
    hashes = {"prepared_lock_sha256": sha256_file(PREPARED_LOCK)}
    for pixel_format in FORMATS:
        output = record["outputs"][OUTPUT_NAMES[pixel_format]]
        path = ROOT / "images" / "prepared" / record["preparation_id"] / \
            record["source_id"] / output["filename"]
        hashes[str(path.relative_to(ROOT))] = output["sha256"]
        cases.append(case_record(
            case_id=f"baya-weaver-le1440-{pixel_format}-q100",
            subset="baya-weaver",
            pixel_format=pixel_format,
            width=int(record["dimensions"][0]),
            height=int(record["dimensions"][1]),
            qindex=100,
            tags=(
                "multiple-superblocks", "prepared-full", "full-image",
                "real-image", "baya-weaver", "ceiling-width",
            ),
            expected_categories=(),
            source_path=path,
            source_sha256=output["sha256"],
            origin={
                "source_id": record["source_id"],
                "preparation_id": record["preparation_id"],
                "manifest": record["manifest"],
                "manifest_sha256": record["manifest_sha256"],
            },
        ))
    return sorted(cases, key=case_output_key), hashes


def build_case_matrix(subsets: Sequence[str], selection: str, limit: int | None) -> tuple[list[dict[str, Any]], dict[str, Any]]:
    cases: list[dict[str, Any]] = []
    corpus: dict[str, Any] = {}
    for subset in subsets:
        if subset == "targeted":
            subset_cases, meta = targeted_cases()
        elif subset == "ceiling":
            subset_cases, meta = ceiling_cases()
        elif subset == "prepared":
            subset_cases, meta = prepared_corpus_cases(selection, limit)
        elif subset == "baya":
            subset_cases, meta = baya_weaver_cases()
        else:
            raise GateError(f"unknown subset: {subset}")
        cases.extend(subset_cases)
        corpus[subset] = meta
    return sorted(cases, key=case_output_key), corpus


def filter_cases(cases: list[dict[str, Any]], pattern: str | None) -> list[dict[str, Any]]:
    if not pattern:
        return cases
    compiled = re.compile(pattern)
    return [case for case in cases if compiled.search(case["id"])]


def materialize_source(case: dict[str, Any], directory: Path) -> tuple[Path, bytes]:
    directory.mkdir(parents=True, exist_ok=True)
    if case.get("source") is not None:
        path = directory / f"source.{case['pixel_format']}"
        source = bytes(case["source"])
        path.write_bytes(source)
        return path.resolve(), source
    source_path = Path(case["source_path"])
    source = source_path.read_bytes()
    if sha256_bytes(source) != case["source_sha256"]:
        raise GateError(f"input hash mismatch: {case['id']}")
    return source_path.resolve(), source


def encode_search(
    case: dict[str, Any],
    source_path: Path,
    source: bytes,
    search: str,
    repetitions: int,
    encoder: Path,
    case_root: Path,
) -> dict[str, Any]:
    output_dir = case_root / search
    output_dir.mkdir(parents=True, exist_ok=True)
    attempts = []
    output_hashes = []
    outputs: list[Path] = []
    metric_match = None
    for attempt in range(repetitions):
        output = output_dir / f"encoded-{attempt}.obu"
        output.unlink(missing_ok=True)
        command = [
            str(encoder), str(source_path),
            "--width", str(case["width"]),
            "--height", str(case["height"]),
            "--qindex", str(case["qindex"]),
            "--search", search,
            "--output", str(output),
        ]
        completed, duration = run(command)
        stderr = completed.stderr.decode("utf-8", "replace").strip()
        attempts.append({
            "argv": command,
            "returncode": completed.returncode,
            "duration_ns": duration,
            "stderr": stderr,
            "output_exists": output.is_file(),
        })
        if completed.returncode != 0:
            return {
                "search": search,
                "status": (
                    "unsupported"
                    if completed.returncode in (3, 5)
                    or "search must be" in stderr
                    or "unsupported" in stderr.lower()
                    else "error"
                ),
                "attempts": attempts,
                "output_dir": str(output_dir.resolve()),
                "reported_sse": [],
            }
        metric_match = METRIC_RE.search(completed.stderr)
        if metric_match is None:
            return {
                "search": search,
                "status": "error",
                "reason": "encoder metrics missing",
                "attempts": attempts,
                "output_dir": str(output_dir.resolve()),
                "reported_sse": [],
            }
        outputs.append(output)
        output_hashes.append(sha256_file(output))
    if len(set(output_hashes)) != 1:
        return {
            "search": search,
            "status": "error",
            "reason": "nondeterministic OBU hash",
            "attempts": attempts,
            "output_dir": str(output_dir.resolve()),
            "reported_sse": [],
        }
    assert metric_match is not None
    reported_sse = [int(metric_match.group(2))]
    if metric_match.group(3) is not None:
        reported_sse.extend((int(metric_match.group(3)), int(metric_match.group(4))))
    stderr = attempts[-1]["stderr"]
    tokens = metric_tokens(stderr)
    category = search_category(search, tokens)
    plane_records = plane_records_from_sse(
        case["pixel_format"], case["width"], case["height"], reported_sse,
    )
    result = {
        "search": search,
        "status": "pass",
        "attempts": attempts,
        "deterministic": True,
        "output_dir": str(output_dir.resolve()),
        "primary_output_path": str(outputs[0].resolve()),
        "obu_bytes": outputs[0].stat().st_size,
        "obu_sha256": output_hashes[0],
        "reported_sse": reported_sse,
        "workspace_bytes": int(metric_match.group(5)),
        "reported_qindex": int(metric_match.group(6)),
        "planes": plane_records,
        "tokens": tokens,
        "category": category,
        "selected_qdelta": [
            int(tokens[name])
            for name in ("qdelta_u", "qdelta_v")
            if name in tokens
        ],
        "qdelta_y_preserved_reported": parse_bool_token(tokens, QDELTA_PRESERVE_KEYS),
        "encode_time_ns": {
            "samples": [int(attempt["duration_ns"]) for attempt in attempts],
            "median": int(statistics.median(attempt["duration_ns"] for attempt in attempts)),
            "worst": int(max(attempt["duration_ns"] for attempt in attempts)),
        },
        "validated": False,
        "local_sse_matches_decode": None,
        "avm_project_exact": None,
        "input_sha256": case["source_sha256"],
        "input_bytes": len(source),
    }
    return result


def retain_stream(
    artifacts_root: Path,
    case: dict[str, Any],
    result: dict[str, Any],
) -> str:
    target_dir = artifacts_root / "streams" / case["subset"] / case["id"]
    target_dir.mkdir(parents=True, exist_ok=True)
    target = target_dir / f"{result['search']}.obu"
    shutil.copyfile(Path(result["primary_output_path"]), target)
    return display_path(target)


def enrich_case_record(
    case: dict[str, Any],
    searches: dict[str, dict[str, Any]],
    artifacts_root: Path,
) -> dict[str, Any]:
    record = {
        key: value for key, value in case.items()
        if key not in {"source", "source_path"}
    }
    legacy_results = [searches[name] for name in legacy_searches_for_case(case["pixel_format"])]
    frontier_results = frontier(legacy_results)
    frontier_categories = sorted({
        result["category"]
        for result in frontier_results
        if result.get("category") is not None
    })
    telemetry_categories = sorted({
        result["category"]
        for result in legacy_results
        if result.get("status") == "pass" and result.get("category") is not None
    })
    record["searches"] = searches
    record["legacy_frontier"] = [result["search"] for result in frontier_results]
    record["frontier_categories"] = frontier_categories
    record["telemetry_categories"] = telemetry_categories
    record["rd_emitted_candidate"] = rd_emitted_candidate(
        case["pixel_format"], searches,
    )
    record["rd_winner_categories"] = (
        rd_winner_categories(searches[RD_SEARCH].get("tokens", {}))
        if record["rd_emitted_candidate"]
        else []
    )
    rd_result = searches[RD_SEARCH]
    if rd_result.get("status") != "pass":
        record["baseline_rate_ceiling_matched"] = False
        record["baseline_search"] = None
        record["baseline_candidates"] = []
        record["rd_vs_baseline"] = None
        return record
    baseline, candidates = select_rate_ceiling_baseline(
        legacy_results,
        rd_result,
        require_validation=case["subset"] in REAL_SUBSETS,
    )
    record["baseline_rate_ceiling_matched"] = baseline is not None
    record["baseline_search"] = None if baseline is None else baseline["search"]
    record["baseline_candidates"] = [candidate["search"] for candidate in candidates]
    if baseline is not None:
        baseline_result = searches[baseline["search"]]
        record["rd_vs_baseline"] = {
            "baseline_search": baseline["search"],
            "byte_delta": int(rd_result["obu_bytes"]) - int(baseline_result["obu_bytes"]),
            "plane_sse_gain": [
                int(left) - int(right)
                for left, right in zip(plane_sse(baseline_result), plane_sse(rd_result))
            ],
            "strict_pareto_win": dominates(rd_result, baseline_result),
            "nonregression": (
                int(rd_result["obu_bytes"]) <= int(baseline_result["obu_bytes"])
                and all(
                    int(right) <= int(left)
                    for left, right in zip(
                        plane_sse(baseline_result),
                        plane_sse(rd_result),
                    )
                )
            ),
        }
        rd_result.setdefault("artifacts", {})["obu"] = retain_stream(
            artifacts_root, case, rd_result,
        )
        baseline_result.setdefault("artifacts", {})["obu"] = retain_stream(
            artifacts_root, case, baseline_result,
        )
    else:
        record["rd_vs_baseline"] = None
    if (
        case["subset"] == "targeted"
        and "qdelta" in case["tags"]
        and all(searches[name].get("status") == "pass" and searches[name].get("validated")
                for name in ("off", "plane-qdelta"))
    ):
        off = searches["off"]
        qdelta = searches["plane-qdelta"]
        record["plane_qdelta_y_invariant"] = (
            off["planes"][0].get("decoded_sha256") == qdelta["planes"][0].get("decoded_sha256")
            and off["planes"][0].get("decoded_sse") == qdelta["planes"][0].get("decoded_sse")
        )
    return record


def aggregate_plane_sse(cases: Sequence[dict[str, Any]], key: str) -> list[int]:
    totals: list[int] = []
    for case in cases:
        result = case[key]
        gains = plane_sse(result)
        if not totals:
            totals = [0] * len(gains)
        for index, value in enumerate(gains):
            totals[index] += int(value)
    return totals


def summarize(cases: list[dict[str, Any]], report: dict[str, Any]) -> dict[str, Any]:
    rd_pass = [case for case in cases if case["searches"][RD_SEARCH].get("status") == "pass"]
    real = [
        case for case in rd_pass
        if case["subset"] in REAL_SUBSETS and case.get("rd_vs_baseline") is not None
    ]
    real_with_baseline = [case for case in real if case["baseline_rate_ceiling_matched"]]
    prepared_corpus_with_baseline = [
        case for case in real_with_baseline
        if case["subset"] == "prepared-corpus"
    ]
    baya_with_baseline = [
        case for case in real_with_baseline
        if case["subset"] == "baya-weaver"
    ]
    targeted = [case for case in cases if case["subset"] == "targeted"]
    observed_categories = sorted({
        category
        for case in targeted
        for category in case.get("rd_winner_categories", [])
    })
    emitted_categories = sorted({
        category
        for case in cases
        if case.get("rd_emitted_candidate", False)
        for category in case.get("rd_winner_categories", [])
    })
    real_rd = [case["searches"][RD_SEARCH] for case in real_with_baseline]
    real_baselines = [case["searches"][case["baseline_search"]] for case in real_with_baseline]
    emitted_rd = [
        case["searches"][RD_SEARCH]
        for case in cases if case.get("rd_emitted_candidate", False)
    ]
    emitted_mode_counts = {
        name: sum(
            int(result.get("tokens", {}).get(f"rd_mode_{name}", "0"))
            for result in emitted_rd
        )
        for name in (
            "dc", "vertical", "horizontal", "smooth", "smooth_v",
            "smooth_h", "paeth",
        )
    }
    emitted_residual_counts = {
        name: sum(
            int(result.get("tokens", {}).get(f"rd_residual_{name}", "0"))
            for result in emitted_rd
        )
        for name in ("skip", "dc", "ac")
    }
    max_workspace = max(
        (case["searches"][RD_SEARCH].get("workspace_bytes", 0) for case in rd_pass),
        default=0,
    )
    return {
        "case_count": len(cases),
        "rd_pass_cases": len(rd_pass),
        "real_case_count": len(real),
        "real_case_count_with_rate_ceiling_baseline": len(real_with_baseline),
        "full_prepared_case_count": sum(
            case["subset"] == "prepared-corpus" for case in cases
        ),
        "full_prepared_expected_case_count": int(
            report.get("corpus", {}).get("prepared", {}).get(
                "selected_count", 0
            )
        ),
        "strict_real_pareto_wins": sum(
            bool(case.get("rd_vs_baseline", {}).get("strict_pareto_win"))
            for case in real_with_baseline
        ),
        "strict_full_prepared_pareto_wins": sum(
            bool(case.get("rd_vs_baseline", {}).get("strict_pareto_win"))
            for case in real_with_baseline
            if "prepared-full" in case["tags"]
        ),
        "strict_prepared_corpus_pareto_wins": sum(
            bool(case.get("rd_vs_baseline", {}).get("strict_pareto_win"))
            for case in prepared_corpus_with_baseline
        ),
        "strict_baya_pareto_wins": sum(
            bool(case.get("rd_vs_baseline", {}).get("strict_pareto_win"))
            for case in baya_with_baseline
        ),
        "targeted_observed_categories": observed_categories,
        "missing_targeted_categories": sorted(
            REQUIRED_TARGET_CATEGORIES - set(observed_categories)
        ),
        "emitted_winner_categories": emitted_categories,
        "missing_emitted_winner_categories": sorted(
            REQUIRED_TARGET_CATEGORIES - set(emitted_categories)
        ),
        "real_rd_plane_sse": aggregate_plane_sse(
            [{RD_SEARCH: result} for result in real_rd],
            RD_SEARCH,
        ) if real_rd else [],
        "real_legacy_plane_sse": aggregate_plane_sse(
            [{"baseline": result} for result in real_baselines],
            "baseline",
        ) if real_baselines else [],
        "real_total_byte_delta": sum(
            int(case["rd_vs_baseline"]["byte_delta"])
            for case in real_with_baseline
        ),
        "real_rd_bytes": sum(int(result["obu_bytes"]) for result in real_rd),
        "real_legacy_bytes": sum(
            int(result["obu_bytes"]) for result in real_baselines
        ),
        "emitted_rd_mode_counts": emitted_mode_counts,
        "emitted_rd_residual_counts": emitted_residual_counts,
        "rd_encode_median_time_total_ns": sum(
            int(case["searches"][RD_SEARCH]["encode_time_ns"]["median"])
            for case in rd_pass
        ),
        "rd_encode_worst_single_time_ns": max(
            (
                int(case["searches"][RD_SEARCH]["encode_time_ns"]["worst"])
                for case in rd_pass
            ),
            default=0,
        ),
        "rd_qdelta_trial_total": sum(
            int(case["searches"][RD_SEARCH].get("tokens", {}).get(
                "rd_qdelta_trials", "0"
            ))
            for case in rd_pass
        ),
        "maximum_workspace_bytes": max_workspace,
        "selected_subsets": report["configuration"]["selected_subsets"],
    }


def build_gates(cases: list[dict[str, Any]], summary: dict[str, Any], probe: dict[str, Any]) -> dict[str, Any]:
    rd_pass = [case for case in cases if case["searches"][RD_SEARCH].get("status") == "pass"]
    real = [
        case for case in rd_pass
        if case["subset"] in REAL_SUBSETS and case.get("rd_vs_baseline") is not None
    ]
    real_with_baseline = [case for case in real if case["baseline_rate_ceiling_matched"]]
    validated_baselines = [
        case["searches"][case["baseline_search"]]
        for case in real_with_baseline
        if case["baseline_search"] is not None
    ]
    rd_real_plane_totals = summary["real_rd_plane_sse"]
    baseline_real_plane_totals = summary["real_legacy_plane_sse"]
    color_rd = [
        case["searches"][RD_SEARCH]
        for case in rd_pass if case["pixel_format"] != "i400"
    ]
    prepared_cases = [
        case for case in cases if case["subset"] == "prepared-corpus"
    ]
    return {
        "rd_available": bool(probe["available"]),
        "all_cases_validated": len(rd_pass) == len(cases),
        "rd_deterministic": all(
            case["searches"][RD_SEARCH].get("deterministic", False)
            for case in rd_pass
        ),
        "rd_local_avm_project_exact": all(
            case["searches"][RD_SEARCH].get("avm_project_exact", False)
            and case["searches"][RD_SEARCH].get("local_sse_matches_decode", False)
            for case in rd_pass
        ),
        "all_formats_covered": {case["pixel_format"] for case in cases} == set(FORMATS),
        "odd_dimensions_covered": any(
            case["width"] % 2 or case["height"] % 2 for case in cases
        ),
        "multiple_superblocks_covered": any(
            "multiple-superblocks" in case["tags"]
            or case["width"] > 64 or case["height"] > 64
            for case in cases
        ),
        "ceiling_covered": any(
            max(case["width"], case["height"]) == 1440 for case in cases
        ),
        "baya_weaver_covered": {
            case["pixel_format"] for case in cases if case["subset"] == "baya-weaver"
        } == set(FORMATS),
        "prepared_corpus_covered": (
            bool(prepared_cases)
            and len(prepared_cases) ==
                summary["full_prepared_expected_case_count"]
        ),
        "emitted_winner_categories_covered":
            not summary["missing_emitted_winner_categories"],
        "real_cases_have_rate_ceiling_baseline": len(real) == len(real_with_baseline),
        "selected_baselines_local_avm_project_exact": all(
            result.get("avm_project_exact", False)
            and result.get("local_sse_matches_decode", False)
            for result in validated_baselines
        ),
        "real_aggregate_plane_nonregression": bool(real_with_baseline) and all(
            rd <= baseline
            for rd, baseline in zip(rd_real_plane_totals, baseline_real_plane_totals)
        ),
        "real_total_byte_nonregression": bool(real_with_baseline) and all(
            int(case["rd_vs_baseline"]["byte_delta"]) <= 0
            for case in real_with_baseline
        ),
        "real_per_case_nonregression": bool(real_with_baseline) and all(
            bool(case["rd_vs_baseline"]["nonregression"])
            for case in real_with_baseline
        ),
        "real_strict_pareto_win": summary["strict_real_pareto_wins"] > 0,
        "full_prepared_strict_pareto_win":
            summary["strict_full_prepared_pareto_wins"] > 0,
        "plane_qdelta_y_invariant": all(
            case.get("plane_qdelta_y_invariant", True)
            for case in cases if case["subset"] == "targeted" and "qdelta" in case["tags"]
        ),
        "rd_color_qdelta_trials_complete": bool(color_rd) and all(
            int(result.get("tokens", {}).get("rd_qdelta_trials", "0")) == 9
            for result in color_rd
        ),
        "rd_color_qdelta_y_invariant": bool(color_rd) and all(
            result.get("qdelta_y_preserved_reported") is True
            for result in color_rd
        ),
    }


def report_for_unavailable_rd(
    *,
    args: argparse.Namespace,
    probe: dict[str, Any],
    output: Path,
) -> int:
    report = {
        "schema": "av2-encoder-blocky-m1-gate-v1",
        "status": "blocked",
        "reason": f"rd unavailable: {probe['reason']}",
        "configuration": {
            "selected_subsets": parse_subsets(args.subsets),
            "prepared_selection": args.prepared_selection,
            "prepared_limit": args.prepared_limit,
            "rd_repetitions": args.rd_repetitions,
            "legacy_repetitions": args.legacy_repetitions,
            "baseline_selector": (
                "validator-exact legacy Pareto-frontier rate ceilings for which "
                "RD emits equal-or-fewer bytes; choose minimum total SSE, then "
                "bytes, then legacy help order"
            ),
        },
        "support_probe": probe,
        "supporting_prepared_crop_evidence": supporting_prepared_crop_evidence(),
        "gates": {"rd_available": False},
        "summary": {
            "case_count": 0,
            "rd_pass_cases": 0,
            "missing_targeted_categories": sorted(REQUIRED_TARGET_CATEGORIES),
            "missing_emitted_winner_categories":
                sorted(REQUIRED_TARGET_CATEGORIES),
        },
        "cases": [],
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps({"status": report["status"], "reason": report["reason"]}, sort_keys=True))
    return 1


def parse_subsets(value: str) -> list[str]:
    mapping = {
        "targeted": "targeted",
        "ceiling": "ceiling",
        "prepared": "prepared",
        "baya": "baya",
    }
    result = []
    for token in value.split(","):
        name = token.strip().lower()
        if not name:
            continue
        if name not in mapping:
            raise GateError(f"unknown subset selection: {token}")
        result.append(mapping[name])
    if not result:
        raise GateError("at least one subset is required")
    return result


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--encoder", type=Path, default=DEFAULT_ENCODER)
    parser.add_argument("--avmdec", type=Path, default=DEFAULT_AVMDEC)
    parser.add_argument("--project-decoder", type=Path, default=DEFAULT_PROJECT)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--artifacts-dir", type=Path, default=DEFAULT_ARTIFACTS)
    parser.add_argument("--work-dir", type=Path, default=DEFAULT_WORK)
    parser.add_argument(
        "--subsets",
        default="targeted,ceiling,baya,prepared",
        help="comma-separated subset list: targeted, ceiling, baya, prepared",
    )
    parser.add_argument(
        "--prepared-selection",
        choices=("selected", "le896", "le1440"),
        default="selected",
    )
    parser.add_argument("--prepared-limit", type=int)
    parser.add_argument("--case-filter")
    parser.add_argument("--rd-repetitions", type=int, default=2)
    parser.add_argument("--legacy-repetitions", type=int, default=1)
    parser.add_argument("--keep-work", action="store_true")
    args = parser.parse_args(argv)

    encoder = executable(args.encoder, "experimental encoder")

    selected_subsets = parse_subsets(args.subsets)
    shutil.rmtree(args.work_dir, ignore_errors=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    probe = probe_rd_support(encoder, args.work_dir)
    if not probe["available"]:
        if not args.keep_work:
            shutil.rmtree(args.work_dir, ignore_errors=True)
        return report_for_unavailable_rd(args=args, probe=probe, output=args.output)
    avmdec = executable(args.avmdec, "pinned AVM decoder")
    project = executable(args.project_decoder, "project decoder")

    cases, corpus = build_case_matrix(
        selected_subsets,
        args.prepared_selection,
        args.prepared_limit,
    )
    cases = filter_cases(cases, args.case_filter)
    if not cases:
        raise GateError("no cases matched the requested subset/filter")

    shutil.rmtree(args.artifacts_dir, ignore_errors=True)
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)

    records = []
    try:
        for case in cases:
            case_root = args.work_dir / case["id"]
            source_path, source = materialize_source(case, case_root)
            searches: dict[str, dict[str, Any]] = {}
            for search in legacy_searches_for_case(case["pixel_format"]):
                searches[search] = encode_search(
                    case,
                    source_path,
                    source,
                    search,
                    args.legacy_repetitions,
                    encoder,
                    case_root,
                )
            searches[RD_SEARCH] = encode_search(
                case,
                source_path,
                source,
                RD_SEARCH,
                args.rd_repetitions,
                encoder,
                case_root,
            )
            needs_validation: set[str] = set()
            if searches[RD_SEARCH].get("status") == "pass":
                needs_validation.add(RD_SEARCH)
                if case["subset"] in REAL_SUBSETS:
                    needs_validation.update(
                        name
                        for name in legacy_searches_for_case(case["pixel_format"])
                        if searches[name].get("status") == "pass"
                    )
                baseline, _candidates = select_rate_ceiling_baseline(
                    [searches[name] for name in legacy_searches_for_case(case["pixel_format"])],
                    searches[RD_SEARCH],
                )
                if baseline is not None:
                    needs_validation.add(baseline["search"])
            if case["subset"] == "targeted" and "qdelta" in case["tags"]:
                needs_validation.update({"off", "plane-qdelta"})
            for search in sorted(needs_validation):
                if search in searches:
                    searches[search] = validate_result(
                        case,
                        searches[search],
                        source,
                        avmdec,
                        project,
                    )
            records.append(enrich_case_record(case, searches, args.artifacts_dir))

        report = {
            "schema": "av2-encoder-blocky-m1-gate-v1",
            "status": "blocked",
            "configuration": {
                "selected_subsets": selected_subsets,
                "prepared_selection": args.prepared_selection,
                "prepared_limit": args.prepared_limit,
                "case_filter": args.case_filter,
                "legacy_repetitions": args.legacy_repetitions,
                "rd_repetitions": args.rd_repetitions,
                "legacy_searches": list(LEGACY_SEARCHES),
                "baseline_selector": (
                    "validator-exact legacy Pareto-frontier rate ceilings for which "
                    "RD emits equal-or-fewer bytes; choose minimum total SSE, then "
                    "bytes, then legacy help order"
                ),
                "required_target_categories": sorted(REQUIRED_TARGET_CATEGORIES),
            },
            "tools": {
                "encoder": {
                    "path": display_path(encoder),
                    "sha256": sha256_file(encoder),
                },
                "avmdec": {
                    "path": display_path(avmdec),
                    "sha256": sha256_file(avmdec),
                },
                "project_decoder": {
                    "path": display_path(project),
                    "sha256": sha256_file(project),
                },
            },
            "support_probe": probe,
            "supporting_prepared_crop_evidence": supporting_prepared_crop_evidence(),
            "corpus": corpus,
            "cases": records,
        }
        summary = summarize(records, report)
        gates = build_gates(records, summary, probe)
        report["summary"] = summary
        report["gates"] = gates
        report["status"] = "pass" if all(gates.values()) else "blocked"
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        print(json.dumps({
            "status": report["status"],
            "case_count": summary["case_count"],
            "strict_real_pareto_wins": summary["strict_real_pareto_wins"],
            "missing_targeted_categories": summary["missing_targeted_categories"],
            "missing_emitted_winner_categories":
                summary["missing_emitted_winner_categories"],
        }, sort_keys=True))
        return 0 if report["status"] == "pass" else 1
    finally:
        if not args.keep_work:
            shutil.rmtree(args.work_dir, ignore_errors=True)


if __name__ == "__main__":
    raise SystemExit(main())
