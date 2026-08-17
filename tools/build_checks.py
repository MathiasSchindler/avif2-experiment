#!/usr/bin/env python3
import argparse
import hashlib
import re
import shutil
import subprocess
import sys
from pathlib import Path


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def run(command: list[str], expected: int = 0) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(command, text=True, capture_output=True)
    if result.returncode != expected:
        detail = result.stderr.strip() or result.stdout.strip()
        raise RuntimeError(
            f"{' '.join(command)} exited {result.returncode}, expected {expected}"
            + (f": {detail}" if detail else ""))
    return result


def decode(decoder: Path, source: Path, output: Path) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    output.unlink(missing_ok=True)
    run([str(decoder), str(source), "-o", str(output)])


def check_parity(args: argparse.Namespace) -> None:
    decode(args.decoder, args.input, args.output)
    actual = sha256(args.output)
    args.output.unlink()
    if actual != args.expected:
        raise RuntimeError(f"expected {args.expected}, got {actual}")
    print(f"decoder output matches {args.expected}")


def check_artifacts(args: argparse.Namespace) -> None:
    entries = args.manifest.read_text(encoding="utf-8").splitlines()
    if len(entries) != args.expected_count:
        raise RuntimeError(
            f"expected {args.expected_count} parity entries, found {len(entries)}")
    shutil.rmtree(args.output_dir, ignore_errors=True)
    args.output_dir.mkdir(parents=True)
    try:
        for entry in entries:
            match = re.fullmatch(r"([^|]+)\|([0-9a-f]+)", entry)
            if match is None:
                raise RuntimeError(f"invalid manifest entry: {entry}")
            name, expected = match.groups()
            output = args.output_dir / f"{name}.i420"
            decode(args.decoder, args.artifact_dir / f"{name}.obu", output)
            actual = sha256(output)
            if actual != expected:
                raise RuntimeError(f"{name}: expected {expected}, got {actual}")
    finally:
        shutil.rmtree(args.output_dir, ignore_errors=True)
    print(f"artifact parity: {len(entries)}/{args.expected_count} exact")


def check_static_elf(args: argparse.Namespace) -> None:
    elf = run([args.readelf, "-h", "-l", "-d", str(args.input)]).stdout
    if "EXEC" not in elf:
        raise RuntimeError(f"{args.input} is not a fixed-address ELF executable")
    for forbidden in ("INTERP", "DYNAMIC", "NEEDED"):
        if forbidden in elf:
            raise RuntimeError(f"{args.input} contains forbidden ELF metadata: {forbidden}")
    undefined = run([args.nm, "-u", str(args.input)]).stdout.strip()
    if undefined:
        raise RuntimeError(f"{args.input} has undefined symbols:\n{undefined}")
    print("static ELF has no interpreter, dynamic dependencies, or undefined symbols")


def check_macho(args: argparse.Namespace) -> None:
    imports = [line for line in run(["/usr/bin/otool", "-L", str(args.input)]).stdout.splitlines()
               if line.strip()]
    if len(imports) > 1:
        raise RuntimeError("Mach-O executable has dylib imports:\n" + "\n".join(imports))
    undefined = run([args.nm, "-u", str(args.input)]).stdout.strip()
    if undefined:
        raise RuntimeError(f"Mach-O executable has undefined symbols:\n{undefined}")
    print("Mach-O executable has no dylib imports or undefined symbols")


def check_output_policy(args: argparse.Namespace) -> None:
    cases = (
        ("bmp", "monochrome-odd.obu", "i400", "2cf48f28100237d4ac9656e17f3cdf2c38c8bb5dd2b3c8af999a159cba513a8b"),
        ("bmp", "ordinary-160x120.obu", "i420", "66ebb65fe21ec8ec8fbfe46fc009b0cf395dba2a22c8f6f91df45eaaa867c20f"),
        ("bmp", "i444-odd.obu", "i444", "782695dcf08bce63864ea9187d3e0fd472a9681547671805c299eb474b2f6469"),
        ("png", "monochrome-odd.obu", "i400", "2faa3ad4be68c069360745d3b402c359d0224e8c6ccd20cf15341099ddd92f7b"),
        ("png", "ordinary-160x120.obu", "i420", "53350b01df7bd559645fb3edd0d36727b47bbc3f55095108591478343b94bacd"),
        ("png", "i444-odd.obu", "i444", "2064d1563ff1e65ea31ee631a3608a9e72606573e6ca4cca2cf42e1c3c32276b"),
    )
    shutil.rmtree(args.output_dir, ignore_errors=True)
    args.output_dir.mkdir(parents=True)
    for extension, fixture, name, expected in cases:
        output = args.output_dir / f"{name}.{extension}"
        decode(args.decoder, args.fixture_dir / fixture, output)
        actual = sha256(output)
        if extension == "png":
            repeat = args.output_dir / f"{name}-repeat.png"
            decode(args.decoder, args.fixture_dir / fixture, repeat)
            if output.read_bytes() != repeat.read_bytes():
                raise RuntimeError(f"{name} PNG output is not deterministic")
            repeat.unlink()
        output.unlink()
        if actual != expected:
            raise RuntimeError(f"{name} {extension.upper()} expected {expected}, got {actual}")
    for name, extension in (("generic", "yuv"), ("mismatched", "i444")):
        output = args.output_dir / f"{name}.{extension}"
        output.unlink(missing_ok=True)
        run([str(args.decoder), str(args.fixture_dir / "ordinary-160x120.obu"),
             "-o", str(output)], expected=2)
        if output.exists():
            raise RuntimeError(f"rejected output was created: {output}")
    shutil.rmtree(args.output_dir)
    print("BMP/PNG hashes, PNG determinism, and raw-output policy match")


def path_argument(value: str) -> Path:
    return Path(value).resolve()


def main() -> int:
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)

    parity = subparsers.add_parser("parity")
    parity.add_argument("--decoder", type=path_argument, required=True)
    parity.add_argument("--input", type=path_argument, required=True)
    parity.add_argument("--output", type=path_argument, required=True)
    parity.add_argument("--expected", required=True)
    parity.set_defaults(action=check_parity)

    artifacts = subparsers.add_parser("artifact-parity")
    artifacts.add_argument("--decoder", type=path_argument, required=True)
    artifacts.add_argument("--artifact-dir", type=path_argument, required=True)
    artifacts.add_argument("--manifest", type=path_argument, required=True)
    artifacts.add_argument("--output-dir", type=path_argument, required=True)
    artifacts.add_argument("--expected-count", type=int, required=True)
    artifacts.set_defaults(action=check_artifacts)

    static_elf = subparsers.add_parser("static-elf")
    static_elf.add_argument("--input", type=path_argument, required=True)
    static_elf.add_argument("--readelf", default="readelf")
    static_elf.add_argument("--nm", default="nm")
    static_elf.set_defaults(action=check_static_elf)

    macho = subparsers.add_parser("no-import-macho")
    macho.add_argument("--input", type=path_argument, required=True)
    macho.add_argument("--nm", default="nm")
    macho.set_defaults(action=check_macho)

    policy = subparsers.add_parser("output-policy")
    policy.add_argument("--decoder", type=path_argument, required=True)
    policy.add_argument("--fixture-dir", type=path_argument, required=True)
    policy.add_argument("--output-dir", type=path_argument, required=True)
    policy.set_defaults(action=check_output_policy)

    args = parser.parse_args()
    try:
        args.action(args)
    except (OSError, RuntimeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())