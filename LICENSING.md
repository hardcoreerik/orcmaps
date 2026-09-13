# Licensing

OrcMaps follows the same dual-licensing model as
[`hardcoreerik/esp-rtl-sdr`](https://github.com/hardcoreerik/esp-rtl-sdr) and
[`hardcoreerik/OrcSDR`](https://github.com/hardcoreerik/OrcSDR):

- **Open source:** AGPL-3.0-only (see [`LICENSE`](LICENSE)).
- **Commercial:** available by direct agreement for organizations that need
  terms outside the AGPL (e.g. embedding in a closed-source product). Contact
  hardcoreerik@gmail.com.

## Engine license vs. map-data license

This repository's **code** — the engine, adapters, tools, examples, tests —
is AGPL-3.0/commercial as above.

**Map data** (prebuilt map packs, whether shipped as release artifacts or
built by users with `tools/pack-builder`) is a separate concern with its own
license, determined entirely by its source data. Most packs will be built
from OpenStreetMap extracts and are subject to the
[Open Database License (ODbL)](https://www.openstreetmap.org/copyright),
which requires attribution and share-alike redistribution of the data (not
the engine code). See [`docs/DATA_AND_LICENSING.md`](docs/DATA_AND_LICENSING.md)
for the full policy, provenance requirements, and attribution rules.

**Do not let map-data licensing constrain engine licensing, and do not let
engine licensing choices contaminate map-data redistribution rights.** They
are tracked and reviewed independently.

## Third-party dependency policy

Every external dependency (library, ported code, data format specification)
must be recorded before use, with:

- name, version/commit, and upstream URL
- license
- what it's used for
- whether any code was copied (should generally be "no" — link/depend on
  upstream rather than vendoring, unless a small header-only utility with a
  compatible permissive license is deliberately vendored, noted as such)

This ledger lives in [`docs/DEPENDENCY_LEDGER.md`](docs/DEPENDENCY_LEDGER.md)
and is kept current as dependencies are added (currently: miniz, MIT,
vendored). No dependency should be added to this project "because GitHub
makes it easy" — each one is a deliberate, reviewed decision, particularly
given the intent to keep this project commercially licensable.

The same discipline applies to map **data** sources, via a separate,
machine-readable registry ([`data/sources/`](data/sources/), enforced by
`tools/check_data_provenance.py`) rather than a prose ledger — see
[`docs/DATA_AND_LICENSING.md`](docs/DATA_AND_LICENSING.md) and
[`docs/DATA_PROVENANCE_REGISTRY.md`](docs/DATA_PROVENANCE_REGISTRY.md).

## Contributions

See [`CONTRIBUTING.md`](CONTRIBUTING.md) for how the dual-license model
above applies to externally submitted contributions.
