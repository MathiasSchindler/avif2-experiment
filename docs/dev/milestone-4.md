# Milestone 4: freestanding query surface

Milestone 4 adds a bounded, allocation-free query and validation API above the
strict AV2-HEIF experimental v0 parser. It identifies the container dialect,
returns effective metadata and immutable source spans, computes exact planar
I420 output layout, reports current pre-decode workspace needs, and binds the
result to the queried bytes.

No AV2 pixel decoder is implemented in this milestone.

## Public API

The query surface is declared in `include/av2heif.h`:

```c
void av2h_limits_default(Av2hLimits *limits);

Av2hStatus av2h_dialect_detect(
    Av2hSpan input,
    Av2hDialect *dialect,
    Av2hError *error);

Av2hStatus av2h_query(
    Av2hSpan input,
    const Av2hLimits *limits,
    Av2hInfo *info,
    Av2hError *error);

Av2hStatus av2h_validate(
    Av2hSpan input,
    const Av2hLimits *limits,
    Av2hError *error);
```

Passing a null limits pointer selects conservative defaults. `av2h_validate`
is the validation-only form of `av2h_query` and performs the same work without
returning the typed result.

`Av2hInfo` contains:

- the detected dialect and complete strict `Av2hHeifImage` metadata;
- separate zero-copy `config_obus` and `item_payload` spans into input;
- exact Y, U, and V dimensions, row bytes, offsets, and byte counts;
- total output bytes for tightly packed 8-bit I420;
- observed OBU count, property count, and container nesting depth;
- current query/validation workspace size and alignment; and
- input size plus a deterministic 64-bit structural fingerprint.

The fingerprint is a stale-query guard for a future decode API, not a
cryptographic digest or authentication mechanism. Query results retain pointers
into caller-owned input, so the input bytes must remain unchanged and alive.

## Caller limits

`Av2hLimits` bounds input bytes, configuration bytes, item payload bytes,
dimensions, pixels, output bytes, OBU count, property count, and container
depth. Every observed value is compared to the caller's maximum. Limit failures
return `AV2H_LIMIT_EXCEEDED` with `AV2H_CONTEXT_LIMITS`.

The default limits are:

| Resource | Default maximum |
| --- | ---: |
| Input | `UINT32_MAX` bytes |
| Configuration OBUs | 1 MiB |
| Item payload | 256 MiB |
| Width and height | 16,384 each |
| Pixels | 268,435,456 |
| Output | 402,653,184 bytes |
| OBUs | 2 |
| Properties | 16 |
| Container depth | 3 |

The strict v0 profile has one Sequence Header OBU, one Closed Loop Key OBU, and
three levels in its deepest supported box path. Later profiles must revise
their defaults deliberately rather than silently widening these values.

Query and validation currently need no caller workspace, so they report zero
bytes with alignment one. This is explicitly pre-decode sizing. Milestone 5
must add conservative decoder workspace sizing before exposing reconstruction.

## Dialect detection

Detection is structural and fail closed:

- `AV2H_DIALECT_CANONICAL_V0` requires the complete strict canonical parser to
  succeed.
- `AV2H_DIALECT_LIBAVIF_AVM_2026_07` requires an `avif` brand, primary `av02`
  item, associated four-byte AV1-shaped `av2C`, and a Sequence Header OBU in
  item data.
- `AV2H_DIALECT_CONFLICTING` identifies mixed indicators such as an `avif`
  brand with draft-shaped configuration or a Sequence Header in both
  configuration and payload.
- unknown structures remain `AV2H_DIALECT_UNKNOWN` and return an error.

Compatibility detection does not enable compatibility decoding. `av2h_query`
accepts canonical v0 only and returns `AV2H_UNSUPPORTED` in
`AV2H_CONTEXT_DIALECT` for the detected libavif dialect. Strict canonical
errors, including unknown `av2C` versions and malformed OBUs, retain their
owning `AV2C`, OBU, Sequence Header, or HEIF contexts.

The internal probe is bounded to 16 properties and 16 OBUs. It reuses checked
box, item, extent, and association parsing but does not weaken
`av2h_heif_parse`.

## Output sizing

The accepted profile is tightly packed 8-bit I420. Query computes:

```text
Y: width x height
U: ceil(width / 2) x ceil(height / 2)
V: ceil(width / 2) x ceil(height / 2)
```

Every multiplication and accumulated offset uses checked arithmetic. U starts
immediately after Y and V immediately after U. The four-fixture laboratory
checks every plane's dimensions, stride, offset, and size against source
geometry and requires total output bytes to equal the pinned AVM I420 artifact.
This includes the odd 17x15 fixture, whose planes are 17x15, 9x8, and 9x8 for
399 bytes total.

## Validation

`make milestone-4` runs the vendor-free sanitizer and freestanding gates, then
the complete offline four-fixture laboratory. Coverage includes:

- canonical query, explicit validation, immutable span metadata, input
  fingerprint, exact output sizing, and zero pre-decode workspace;
- every public limit category and `AV2H_CONTEXT_LIMITS` diagnostics;
- structural canonical, libavif AVM, and conflicting dialect fixtures;
- strict rejection of compatibility query and preservation of unknown-version
  `AV2H_CONTEXT_AV2C` diagnostics;
- exact query/manifest parity for tiny, odd, ordinary, and high-detail images;
- AddressSanitizer and UndefinedBehaviorSanitizer; and
- C11 `-ffreestanding -nostdinc` compilation plus a zero-undefined-symbol
  relocatable core.

## Limitations

The compatibility dialect is detected but cannot be queried or decoded. The
public query supports one canonical v0 primary image only. Output sizing is for
the current 8-bit I420 profile and does not account for caller-selected strides,
RGB conversion, presentation transforms, or decoded workspace. The input
fingerprint is intentionally noncryptographic. Decoder state, output buffers,
workspace consumption, and query-to-decode binding checks belong to Milestone
5.