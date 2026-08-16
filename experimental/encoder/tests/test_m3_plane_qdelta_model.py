#!/usr/bin/env python3

import hashlib
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path
from unittest import TestCase

ROOT = Path(__file__).resolve().parents[3]
ENCODER = ROOT / "experimental" / "encoder"
sys.path.insert(0, str(ENCODER / "tools"))

from m3_plane_qdelta_model import (
    Candidate,
    FrameQuantizerDeltas,
    QuantizerDeltaConfig,
    build_stream,
    candidate_pairs,
    choose_dc_level,
    coeff_q_context,
    delta_code_bits,
    dequant_step,
    encode_frame_header,
    encode_sequence_header,
    frame_delta_payload_bits,
    select_candidate,
)
from obu_inspect import parse_obus
from .validator_support import require_validator


DRIVER = ENCODER / "build" / "m3-plane-qdelta-driver"
CHROMA_DRIVER = ENCODER / "build" / "m3-chroma-driver"
BLOCKS = ENCODER / "fixtures" / "m3-plane-qdelta-blocks.json"
VECTORS = ENCODER / "tests" / "m3_plane_qdelta_vectors.json"
WORK = ENCODER / "build" / "m3-plane-qdelta"
PINNED_AVMDEC_SHA256 = (
    "c247cbb4e71e48bdec89f3c8b2b8efdce17f673aeaa39e3c116a276d16286aa0"
)
PINNED_PROJECT_SHA256 = (
    "7d17f598f2cdffe7dfb4ab46982cf5c4822215a5f429cb39e7748a60ce5ef535"
)


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


class M3PlaneQDeltaModelTests(TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        if WORK.exists():
            shutil.rmtree(WORK)
        WORK.mkdir(parents=True)

    @classmethod
    def tearDownClass(cls) -> None:
        if WORK.exists():
            shutil.rmtree(WORK)

    def tile(self, format_id: int, qindex: int,
             u_level: int, v_level: int, name: str) -> bytes:
        path = WORK / f"{name}.tile"
        subprocess.run(
            [
                str(DRIVER), str(format_id), str(qindex), "0",
                str(u_level), str(v_level), str(path),
            ],
            cwd=ROOT, check=True)
        return path.read_bytes()

    def validators(self) -> tuple[Path, Path]:
        avmdec = require_validator(
            self,
            Path(os.environ.get(
                "AVMDEC", ROOT / "build/reference/avm/avmdec")),
            "pinned avmdec")
        project = require_validator(
            self,
            Path(os.environ.get(
                "PROJECT_DECODER",
                ROOT / "build/host-cmake/av2heif-decoder")),
            "project decoder")
        self.assertEqual(sha256_bytes(avmdec.read_bytes()), PINNED_AVMDEC_SHA256)
        self.assertEqual(
            sha256_bytes(project.read_bytes()), PINNED_PROJECT_SHA256)
        return avmdec, project

    def test_signed_range_encoding_and_flags(self) -> None:
        self.assertEqual(delta_code_bits(0), "0")
        self.assertEqual(delta_code_bits(-64), "11000000")
        self.assertEqual(delta_code_bits(-1), "11111111")
        self.assertEqual(delta_code_bits(1), "10000001")
        self.assertEqual(delta_code_bits(63), "10111111")
        with self.assertRaises(ValueError):
            delta_code_bits(-65)
        with self.assertRaises(ValueError):
            delta_code_bits(64)
        shared = QuantizerDeltaConfig()
        self.assertEqual(
            frame_delta_payload_bits(
                shared,
                FrameQuantizerDeltas(
                    u_dc=-8, u_ac=-8, v_dc=-8, v_ac=-8)),
            "1" + f"{-8 & 0x7f:07b}")
        separate = QuantizerDeltaConfig(separate_uv_delta_q=True)
        self.assertEqual(
            len(frame_delta_payload_bits(
                separate,
                FrameQuantizerDeltas(
                    u_dc=-8, u_ac=-8, v_dc=8, v_ac=8))),
            17)
        with self.assertRaises(ValueError):
            frame_delta_payload_bits(
                shared,
                FrameQuantizerDeltas(
                    u_dc=-8, u_ac=-8, v_dc=8, v_ac=8))
        with self.assertRaises(ValueError):
            frame_delta_payload_bits(
                shared,
                FrameQuantizerDeltas(
                    u_dc=-8, u_ac=8, v_dc=-8, v_ac=8))

    def test_dequant_lookup_and_q_context_boundaries(self) -> None:
        self.assertEqual(dequant_step(1, -64), 40)
        self.assertEqual(dequant_step(24, 0), 78)
        self.assertEqual(dequant_step(25, 0), 80)
        self.assertEqual(dequant_step(255, 63), dequant_step(255, 0))
        self.assertEqual(
            [coeff_q_context(q) for q in (1, 90, 91, 140, 141, 190, 191, 255)],
            [0, 0, 1, 1, 2, 2, 3, 3])
        for qindex in (90, 91, 140, 141, 190, 191):
            self.assertEqual(
                coeff_q_context(qindex),
                coeff_q_context(qindex))

    def test_default_headers_match_existing_profile(self) -> None:
        obu = WORK / "baseline.obu"
        local = WORK / "baseline.raw"
        subprocess.run(
            [
                str(CHROMA_DRIVER), "0", "100", "8", "8",
                "128", "128", "128", str(obu), str(local),
            ],
            cwd=ROOT, check=True)
        parsed = parse_obus(obu.read_bytes())
        disabled = QuantizerDeltaConfig(uv_ac_delta_q_enabled=False)
        self.assertEqual(
            parsed[1].payload,
            encode_sequence_header(8, 8, 0, disabled))
        frame, _ = encode_frame_header(
            8, 8, 100, disabled, FrameQuantizerDeltas())
        self.assertTrue(parsed[2].payload.startswith(frame))

    def test_frozen_parity_and_rd_vectors(self) -> None:
        avmdec, project = self.validators()
        blocks = json.loads(BLOCKS.read_text())["blocks"]
        manifest = json.loads(VECTORS.read_text())
        self.assertEqual(manifest["avmdec_sha256"], PINNED_AVMDEC_SHA256)
        self.assertEqual(
            manifest["project_decoder_sha256"], PINNED_PROJECT_SHA256)
        for case in manifest["cases"]:
            block = blocks[case["block"]]
            size = block["size"]
            candidates = []
            for entry in case["candidates"]:
                u_level, u_value, u_sse = choose_dc_level(
                    block["u"], case["qindex"], entry["u_delta"], size)
                v_level, v_value, v_sse = choose_dc_level(
                    block["v"], case["qindex"], entry["v_delta"], size)
                self.assertEqual([u_level, v_level], entry["levels"])
                self.assertEqual([u_value, v_value], entry["reconstruction"])
                config = QuantizerDeltaConfig(
                    separate_uv_delta_q=entry["u_delta"] != entry["v_delta"])
                deltas = FrameQuantizerDeltas(
                    u_dc=entry["u_delta"], u_ac=entry["u_delta"],
                    v_dc=entry["v_delta"], v_ac=entry["v_delta"])
                tile = self.tile(
                    case["format"], case["qindex"],
                    u_level, v_level,
                    f"{case['name']}-{entry['u_delta']}-{entry['v_delta']}")
                stream, header_bits = build_stream(
                    8, 8, case["format"], case["qindex"],
                    config, deltas, tile)
                self.assertEqual(len(stream), entry["stream_bytes"])
                self.assertEqual(header_bits, entry["header_bits"])
                self.assertEqual(sha256_bytes(stream), entry["obu_sha256"])
                self.assertEqual(u_sse + v_sse, entry["weighted_chroma_sse"])
                candidates.append(Candidate(
                    entry["u_delta"], entry["v_delta"],
                    (u_level, v_level), (u_value, v_value),
                    u_sse + v_sse, len(stream), header_bits))
                if entry["selected"]:
                    obu = WORK / f"{case['name']}.obu"
                    avm = WORK / f"{case['name']}.avm"
                    project_raw = WORK / (
                        f"{case['name']}.i420" if case["format"] == 0
                        else f"{case['name']}.i444")
                    obu.write_bytes(stream)
                    subprocess.run(
                        [
                            str(avmdec), "--codec=av2", "--rawvideo",
                            "--output-bit-depth=8", "--limit=1",
                            "-o", str(avm), str(obu),
                        ],
                        cwd=ROOT, check=True, stdout=subprocess.DEVNULL,
                        stderr=subprocess.DEVNULL)
                    subprocess.run(
                        [str(project), str(obu), "-o", str(project_raw)],
                        cwd=ROOT, check=True, stdout=subprocess.DEVNULL,
                        stderr=subprocess.DEVNULL)
                    self.assertEqual(avm.read_bytes(), project_raw.read_bytes())
                    raw = avm.read_bytes()
                    plane_size = size * size
                    self.assertEqual(set(raw[:64]), {128})
                    self.assertEqual(set(raw[64:64 + plane_size]), {u_value})
                    self.assertEqual(
                        set(raw[64 + plane_size:64 + 2 * plane_size]),
                        {v_value})
                    self.assertEqual(
                        sha256_bytes(raw), case["reconstruction_sha256"])
            baseline = next(
                entry for entry in case["candidates"]
                if entry["u_delta"] == 0 and entry["v_delta"] == 0)
            selected = select_candidate(
                candidates, baseline["stream_bytes"])
            expected = next(
                entry for entry in case["candidates"] if entry["selected"])
            self.assertEqual(
                (selected.u_delta, selected.v_delta),
                (expected["u_delta"], expected["v_delta"]))
            self.assertEqual(
                tuple(candidate_pairs(
                    block["u"], block["v"], case["qindex"], size)),
                tuple((entry["u_delta"], entry["v_delta"])
                      for entry in case["candidates"]))
        for probe in manifest["syntax_probes"]:
            block = blocks[probe["block"]]
            config = QuantizerDeltaConfig(**probe["config"])
            deltas = FrameQuantizerDeltas(**probe["deltas"])
            u_level, u_value, u_sse = choose_dc_level(
                block["u"], probe["qindex"],
                deltas.u_dc + config.base_uv_dc_delta_q, block["size"])
            v_level, v_value, v_sse = choose_dc_level(
                block["v"], probe["qindex"],
                deltas.v_dc + config.base_uv_dc_delta_q, block["size"])
            self.assertEqual([u_level, v_level], probe["levels"])
            tile = self.tile(
                probe["format"], probe["qindex"], u_level, v_level,
                probe["name"])
            stream, header_bits = build_stream(
                8, 8, probe["format"], probe["qindex"],
                config, deltas, tile)
            self.assertEqual(header_bits, probe["header_bits"])
            self.assertEqual(len(stream), probe["stream_bytes"])
            self.assertEqual(sha256_bytes(stream), probe["obu_sha256"])
            self.assertEqual(u_sse + v_sse, probe["weighted_chroma_sse"])
            obu = WORK / f"{probe['name']}.obu"
            avm = WORK / f"{probe['name']}.avm"
            project_raw = WORK / f"{probe['name']}.i420"
            obu.write_bytes(stream)
            subprocess.run(
                [
                    str(avmdec), "--codec=av2", "--rawvideo",
                    "--output-bit-depth=8", "--limit=1",
                    "-o", str(avm), str(obu),
                ],
                cwd=ROOT, check=True, stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL)
            subprocess.run(
                [str(project), str(obu), "-o", str(project_raw)],
                cwd=ROOT, check=True, stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL)
            self.assertEqual(avm.read_bytes(), project_raw.read_bytes())
            self.assertEqual(
                sha256_bytes(avm.read_bytes()),
                probe["reconstruction_sha256"])

    def test_i400_production_vectors_are_untouched(self) -> None:
        with self.assertRaises(ValueError):
            encode_sequence_header(
                8, 8, 1,
                QuantizerDeltaConfig(uv_ac_delta_q_enabled=False))
