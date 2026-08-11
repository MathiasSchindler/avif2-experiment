# 10-bit implementation WP3 sample and quantizer domain

## Result

10B-WP3 is complete. The decoder now has a production 10-bit sample domain,
depth-aware quantizer and dequantizer derivation, depth-dependent coefficient
saturation, complete Y/U/V delta composition, a normative lossless predicate,
and explicit signed intermediate-width contracts. Public 10-bit decode remains
deliberately unsupported and nonmutating at reconstruction.

This package establishes arithmetic inputs and bounds. It does not implement
10-bit prediction, inverse transforms, reconstruction, or sample filtering;
those remain owned by later packages.

## Sample domain

The production 10-bit domain is:

- bit depth: 10;
- storage width: two bytes per sample;
- midpoint: 512; and
- inclusive maximum: 1023.

Typed helpers provide midpoint, adjacent midpoint values, maximum, and clipping
without changing the existing 8-bit helpers. Boundary vectors cover 255/256,
511/512, 1022/1023, values outside the sample range, and signed 32-bit extrema.
The U16 sample operations use this production domain rather than a test-local
descriptor.

## Q-index domain

The frame header retains the complete 9-bit base Q-index. Effective quantizer
indices are clamped only when quantizer or delta-Q semantics require it:

| Input depth | Effective Q-index range |
| --- | ---: |
| 8-bit | 0 through 255 |
| 10-bit | 0 through 303 |

The 10-bit maximum is `255 + 2 * 24`. The existing 24-step exponential lookup
is shared by both depths; 10-bit extends its allowed range rather than applying
a separate bit-depth multiplier. Locked values include:

| Q-index | Dequant step |
| ---: | ---: |
| 252 | 56,320 |
| 255 | 61,440 |
| 256 | 63,488 |
| 303 | 245,760 |

Inputs above the effective maximum, including 304, 400, and 511, derive the
Q=303 step. The existing 8-bit entry point continues to clamp Q=256 to Q=255
and retains its previous results byte-for-byte.

Delta-Q updates now clamp to the sequence depth's effective range. This removes
the previous 255 ceiling for admitted 10-bit streams while retaining the
normative lower bound of one for non-lossless delta updates.

## Plane quantizers and lossless state

A single plane-step owner composes frame and sequence deltas before selecting
the depth-specific quantizer:

- Y DC uses frame Y DC plus sequence base Y DC; Y AC uses no delta;
- U/V DC use their frame DC delta plus sequence base UV DC; and
- U/V AC use their frame AC delta plus sequence base UV AC.

The coefficient readers use these derived DC/AC steps for luma, FSC, and
chroma paths. Signaled chroma deltas are no longer rejected merely because they
are nonzero.

A frame segment is lossless only when its Q-index is zero, delta-Q is absent,
and all composed Y/U/V DC and U/V AC deltas are nonpositive. Deblock metadata
uses that predicate instead of testing base Q-index alone.

Deblock threshold derivation also consumes the depth-aware Q-index and composed
UV AC deltas. Its sample kernels remain 8-bit; this package only prevents
10-bit quantizer inputs from narrowing before later U16 filter work.

## Coefficient and intermediate bounds

Dequantized coefficients use signed 32-bit storage with depth-dependent
saturation:

- 8-bit: -32,768 through 32,767; and
- 10-bit: -131,072 through 131,071.

The dequant product is explicitly signed 64-bit, retains the normative low
24-bit working value, rounds by three bits, applies the transform-size shift,
and then saturates to the depth-specific coefficient range. Checked extreme
vectors run under ASan and UBSan without signed overflow or invalid shifts.

The reconstruction interfaces now name their intended numeric contracts:
signed 32-bit transform intermediates and signed 64-bit transform sums,
prediction sums, and reconstruction sums. These aliases do not change private
workspace layouts or implement WP4 kernels.

## Deliberate decode boundary

For the locked 16x16 profile-0 10-bit 4:2:0 stream, the parser retains base
Q-index 256 and tile state reaches effective Q-index 252. Its quantizer is now
available and pinned to step 56,320. Public decode still returns
`AV2H_UNSUPPORTED` with `AV2H_CONTEXT_DECODE` before reconstruction mutates any
of the 768 output bytes. The WP0 raw decoder oracle continues to require an
unchanged pre-existing output file.

## Validation

Run the focused package gate with:

```sh
make 10bit-wp3
```

It runs coefficient, sample-domain, U16 sample-operation, delta-Q, frame/tile,
lossless/deblock, private-layout, and public nonmutation tests under the
sanitized hosted build, followed by the WP0 real-decoder nonmutation oracle.
The ordinary `make test` suite remains the complete regression gate.
