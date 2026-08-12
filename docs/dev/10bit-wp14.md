# 10-bit image release gate (WP14)

## Result

10B-WP14 is complete. The public decoder accepts profile-0 10-bit 4:2:0 and
profile-4 10-bit 4:4:4 canonical still images and emits sample-exact planar
output against AVM revision `966a7d7cd6fcf60360caf5dc413b2aeeb65e144d`.
All 43 retained `yuv420p10le` records and all 39 retained `yuv444p10le`
records are byte-identical through hosted native/public decode, Linux no-libc,
and WebAssembly.

The complete release gate is:

```sh
make 10bit-wp14
```

`make 10bit-wp14-fuzz` runs the bounded public-boundary mutation gate alone.

## Accepted formats

| Sequence profile | Chroma | Depth | Public output | Status |
| ---: | --- | ---: | --- | --- |
| 0 | 4:2:0 | 8 | planar I420/U8 | retained |
| 0 | monochrome | 8 | planar I400/U8 | retained |
| 4 | 4:4:4 | 8 | planar I444/U8 | retained |
| 0 | 4:2:0 | 10 | `yuv420p10le` | released |
| 4 | 4:4:4 | 10 | `yuv444p10le` | released |

The accepted profile/chroma pair is exact: profile 0 owns 4:2:0 and
monochrome, while profile 4 owns 4:4:4. Mismatched profile, chroma, container
`pixi`, or sequence metadata fails closed.

## Output contract

Output is one contiguous, tightly packed allocation in Y, U, V plane order.
Eight-bit samples occupy unsigned bytes. Ten-bit samples occupy unsigned
16-bit little-endian words with values in `[0, 1023]`; there is no implicit
downconversion, RGB conversion, padding, or row alignment.

For an image of width $W$ and height $H$:

| Format | Y dimensions | U/V dimensions | Bytes per sample |
| --- | --- | --- | ---: |
| I400 | $W \times H$ | absent | 1 |
| I420 | $W \times H$ | $\lceil W/2 \rceil \times \lceil H/2 \rceil$ | 1 |
| I444 | $W \times H$ | $W \times H$ | 1 |
| `yuv420p10le` | $W \times H$ | $\lceil W/2 \rceil \times \lceil H/2 \rceil$ | 2 |
| `yuv444p10le` | $W \times H$ | $W \times H$ | 2 |

The caller supplies the exact `output_size`, plane offsets, row byte counts,
and workspace requirement returned by `av2h_query`. Workspace must satisfy the
reported power-of-two alignment, currently four bytes. Ten-bit output must be
aligned for `uint16_t`, currently two bytes; byte output has no stronger
alignment requirement. Input is byte-addressed. Workspace, output, input,
`Av2hInfo`, and `Av2hError` ranges must obey the public non-overlap contract.

For the retained 16 x 16 probes, query reports:

| Format | Output bytes | Workspace bytes | Plane offsets |
| --- | ---: | ---: | --- |
| `yuv420p10le` | 768 | 26,497 | 0, 512, 640 |
| `yuv444p10le` | 1,536 | 28,801 | 0, 512, 1,024 |

The LP64 ABI remains `sizeof(Av2hInfo) == 248` and
`_Alignof(Av2hInfo) == 8`. Query applies checked dimension, pixel, output,
workspace, property, OBU, and recursion limits before decode allocation.

## Public-boundary fuzzing

`tests/public_decode_fuzz.c` exports the conventional
`LLVMFuzzerTestOneInput` entry point and a dependency-free deterministic
driver. Each input is queried with these stricter fuzz limits:

- input, output, and workspace: at most 1 MiB each;
- width and height: at most 512 samples each;
- pixels: at most 262,144.

The driver converts retained OBU seeds to canonical HEIF and exercises query,
conditional decode, truncations, distributed byte substitutions, and bit
flips. It also checks output nonmutation for undersized workspace/output and
stale-info failures. The release run covered 1,206 deterministic ASan/UBSan
cases from 8-bit I444, 10-bit I420, and 10-bit I444 seeds. The same entry point
can be linked to libFuzzer in a toolchain that provides its runtime.

## Unsupported boundary

The following remain outside the accepted image profile and fail closed:

- 10-bit monochrome, all 4:2:2, and all 12-bit output;
- restoration, film grain, unsupported segmentation, and quantization matrices;
- unsupported tile forms, non-initial tile groups, and multiframe/video streams;
- alpha, grids, transformations, auxiliary items, and expanded metadata;
- production RGB conversion, color management, and implicit 8-bit previews.

Retained tiny I420 and I444 streams produced with AVM `--lossless=1` are exact.
That evidence does not admit arbitrary base-Q-zero streams or lossless streams
that use otherwise unsupported syntax or tools.

## Diagnostics

The public API returns a stable `Av2hStatus` and, when supplied, an
`Av2hError` containing byte offset, context, OBU index, and OBU type. Resource
caps report `AV2H_LIMIT_EXCEEDED`; malformed or inconsistent syntax reports
`AV2H_INVALID_DATA` or `AV2H_TRUNCATED`; excluded profile features report
`AV2H_UNSUPPORTED`; stale query/decode bindings report `AV2H_INVALID_DATA` in
`AV2H_CONTEXT_BINDING`; and undersized caller buffers report
`AV2H_OUT_OF_MEMORY` in the workspace or output context. Rejected preflight
calls leave output unchanged where the contract can determine failure before
decoding.

## Validation evidence

The completion run established:

- 25/25 hosted CTest entries passed under ASan/UBSan, including fuzz;
- 36/36 Python development tests passed;
- 31/31 retained 8-bit I420 artifacts, 12/12 I400 native records, 12/12
  I444 native records, and 12/12 I444 public records remained exact;
- 43/43 10-bit I420 and 39/39 10-bit I444 records were exact through hosted
  native/public decode;
- 2/2 Linux no-libc tests passed and all 82 10-bit records were exact there;
- all 82 10-bit records were exact through WebAssembly;
- Windows x86-64 executables cross-built with exact import contracts; and
- 94 generator-owned synthetic reference files regenerated byte-for-byte.

Windows runtime parity was not executed because this Linux host has no PE
runtime configured. Set `WP14_WINDOWS_EMULATOR` when invoking the release gate
to configure and execute the Windows CTest matrix through an available
emulator.

## Locked hashes

- synthetic manifest:
  `1887f3ee201b6fb9ec61485dfe0cf5aba37e9acca56ccbe1ceb728d6ee13486f`
- synthetic coverage report:
  `e9ef3c3afe33c85904651d6e1bbd8223a993b49db51c47bbc0e8e28268f071fb`
- synthetic retained-tree digest:
  `9ca69bd3b5427355acaf6ba2570d92a2467a1cd4182cb2dd6314337f6bc9a91b`
- photographic manifest:
  `3136c54a6d953235890d36b56acdfe66753ee1fd7944e4aa774aac7eeee3e337`
- photographic retained-tree digest:
  `2e5f29eb358b35c94931743d0b311cbe5620fc437272cd953bd6838a7545e293`

The tree digests hash the sorted `sha256sum` records, including relative file
names, for every file below the corresponding `avm-reference` directory.
Production core code remains allocator-free and freestanding; the reference
AVM and census tools are development-only and are not linked into release
artifacts.