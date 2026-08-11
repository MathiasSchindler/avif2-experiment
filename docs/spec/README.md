# Local AV2 specification

The AV2 v1.0.0 specification is mirrored locally for decoder work:

```sh
scripts/setup-av2-spec.sh
```

The command downloads the self-contained HTML specification and every linked
header attachment from:

<https://av2.aomedia.org/v1.0.0/index.html>

The resulting files live in `docs/spec/av2-v1.0.0/`. That directory is ignored
by Git so the specification and generated attachment links remain local and
are not published with this repository.

Open `docs/spec/av2-v1.0.0/index.html` in a browser or search the downloaded
headers directly. The decoder foundation checking and table-generation tools use this
directory by default.
