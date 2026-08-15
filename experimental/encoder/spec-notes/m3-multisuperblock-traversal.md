# M3 multi-superblock traversal

Status: production-complete for 8-bit DC-only I400 through 896x896.

The single tile visits 64x64 superblock roots row-major in 16-MI steps.
Frame MI dimensions remain `2 * ceil(pixels / 8)`, so the final root on each
axis is boundary-rounded and nodes outside that grid are omitted. Each root
uses the already verified depth-first rectangle traversal.

Partition and coefficient above contexts span the rounded tile width and
persist across superblock rows. Their left contexts span one 16-MI
superblock row and are cleared when root traversal wraps to the next row.
Adaptive partition, mode, transform, and coefficient CDFs initialize once per
tile and never reset between roots. The reconstruction canvas is frame-wide,
so DC prediction reads reconstructed neighbors across root boundaries.
Source samples outside the active crop repeat the final active row or column;
SSE and returned reconstruction use only the active crop.

`tools/m3_multisuperblock_model.py` independently freezes root and transform
order for the requested 65/127/128/129 cases and prepared-image dimensions.
`tests/m3_multisuperblock_parity.json` records 14/14 deterministic selected
prepared-image encodes and 14/14 exact pinned-AVM/project parity. Level index
0 is used through its 552,960-sample picture limit; larger selected images use
level index 1.
