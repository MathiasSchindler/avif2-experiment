# Deterministic AV2-HEIF v0 container

The deterministic container implements a strict typed parser and writer for the
project-defined AV2-HEIF experimental v0 still-image profile. It does not claim
standardized AVIF or AV2 image-item conformance and does not decode AV2 picture
syntax.

## Canonical file graph

The writer emits one primary image item in this fixed top-level order:

```text
ftyp  major=mif1, minor=0, compatible=mif1
meta
  hdlr  handler=pict
  pitm  item_ID=1
  iloc  one construction-method-0 extent
  iinf
    infe  item_ID=1, item_type=av02, name="AV2"
  iprp
    ipco
      av2C  draft-shaped nine-byte header plus one Sequence Header OBU and
        optional static Content Interpretation OBU
      ispe  dimensions derived from the Sequence Header
      pixi  three 8-bit channels
      colr  explicit nclx values
    ipma  item 1 associations, with only av2C essential
mdat  one Closed Loop Key OBU
```

The extent points directly to all bytes following the `mdat` header. The writer
uses 32-bit box sizes and offsets, emits no AVIF brands, and produces stable box,
property, and association ordering. For the 13-byte test Sequence Header and
three-byte coded payload, the complete independent golden is 282 bytes.

The parser requires `ftyp` first but accepts `meta` and `mdat` in either order.
It requires exactly one of each required structure and rejects unknown top-level
boxes, duplicate required children or properties, inconsistent item IDs,
dangling or duplicate associations, nonincreasing property indices, malformed
extents, and unknown essential properties. Unknown nonessential item properties
may be ignored within the bounded property table.

## API and ownership

`av2h_heif_write` accepts an `Av2hHeifSource` containing caller-owned
configuration and coded-data spans plus explicit CICP color values. Calling it
with a null output and zero output size returns the required size. A real write
validates v0 carriage, derives dimensions, serializes the canonical graph, then
reparses and compares the emitted typed model before returning success.

`av2h_heif_parse` returns a zero-copy `Av2hHeifImage`. Its source spans point
into the caller's input buffer, so that buffer must remain valid while the
result is used. The result also contains parsed Sequence Header metadata,
dimensions, channel count, and bit depth.

Both operations are allocation-free, perform no file I/O, use no global mutable
state, and report checked status, byte offset, and parser context through
`Av2hError`.

## Color contract

Experimental v0 follows Decision D9 with the static-metadata extension:
`configOBUs` contains exactly one Sequence Header OBU followed by at most one
Content Interpretation OBU, and item data contains exactly one Closed Loop Key
OBU. Other additional configuration or payload OBUs are rejected.

`Av2hHeifSource.color` is the effective metadata channel for the canonical
`nclx` property and optional AV2 chroma positions. When Content Interpretation
is present, its effective CICP values must equal `nclx`; both chroma-position
values are preserved through parse and query. Pinned AVM's legal position range
is `0..6`, with `6` meaning unspecified.

## Validation

`make test` runs without AVM or any other vendored software. deterministic container adds a
separate C sanitizer binary covering:

- byte-exact output against a complete 282-byte golden;
- sizing, write, parse, typed-model checks, and byte-identical remuxing;
- wrapping and reparsing the real 100-byte tiny AVM coded payload;
- parser acceptance of `mdat` before `meta` with a checked relocated extent;
- output exhaustion and size-overflow preflight before caller spans are read;
- truncation, brands, handler, item IDs, versions, field widths, item type,
  duplicate structures, and invalid extents;
- missing, duplicate, reordered, dangling, nonessential, and unknown essential
  property associations;
- `av2C`, `ispe`, `pixi`, and `nclx` conflicts and reserved bits; and
- Content Interpretation write/parse/query roundtrip, CICP mismatch rejection,
  and rejection when it appears in item data or an invalid configuration order.

Named container mutations assert the HEIF diagnostic context. Nested AV2
configuration errors retain their more specific carriage, OBU, Sequence Header,
or `av2C` context where applicable.

The complete gate also builds every core source as C11 with `-ffreestanding`,
`-nostdinc`, no builtins, no stack protector, and no unwind tables. The linked
relocatable core has no undefined symbols.

## Limitations

The implemented profile is one 8-bit, three-channel still image with one item,
one extent, and one coded OBU. It intentionally excludes thumbnails, auxiliary
images, grids, metadata items, references, multiple extents, fragmentation,
large-size boxes, nonzero data references, and alternative construction
methods. It validates carriage and container consistency but does not validate
or decode the complete Closed Loop Key payload. Fuzzing, cross-compiler gates,
hosted interoperability tooling, and pixel reconstruction remain later work.