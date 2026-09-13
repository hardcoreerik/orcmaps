# Data provenance registry

The machine-readable half of [`DATA_AND_LICENSING.md`](DATA_AND_LICENSING.md).
Every dataset OrcMaps has reviewed for possible use gets one JSON record
under [`../data/sources/`](../data/sources/), validated by
`tools/check_data_provenance.py` (CI: `.github/workflows/data-provenance.yml`).

## Why JSON, not YAML

The original design sketch for this registry used YAML. We use JSON
instead: `tools/check_documentation_truth.py` (the existing Documentation
Truth checker) is deliberately standard-library-only, with no PyYAML or
similar dependency, and `tools/check_data_provenance.py` follows the same
discipline for the same reason — a CI gate with a third-party Python
dependency is one more thing that can silently break or drift. `json` is
in the Python standard library. The schema and semantics are identical to
what a YAML version would express; only the syntax differs.

## Record schema

```json
{
  "id": "natural-earth",
  "name": "Natural Earth",
  "publisher": "Natural Earth",
  "source": {
    "url": "https://www.naturalearthdata.com/about/terms-of-use/",
    "retrieved": "2026-09-13"
  },
  "rights": {
    "license": "Public Domain",
    "license_class": "PUBLIC_DOMAIN_VERIFIED",
    "commercial_use_allowed": true,
    "redistribution_allowed": true,
    "modification_allowed": true,
    "attribution_required": false,
    "share_alike_required": false,
    "source_disclosure_required": false
  },
  "evidence": {
    "rights_url": "https://www.naturalearthdata.com/about/terms-of-use/",
    "reviewed_date": "2026-09-13",
    "confidence": "CONFIRMED"
  },
  "policy": {
    "official_pack_allowed": true,
    "clean_pack_allowed": true
  },
  "notes": "Free text: caveats, what's NOT covered by this record, etc."
}
```

Required fields (the checker rejects a record missing any of these):
`id`, `name`, `publisher`, `source.url`, `source.retrieved`,
`rights.license`, `rights.license_class`, all seven `rights.*_allowed` /
`rights.*_required` booleans, `evidence.rights_url`,
`evidence.reviewed_date`, `evidence.confidence`,
`policy.official_pack_allowed`, `policy.clean_pack_allowed`.

`evidence.confidence` is `CONFIRMED` (an explicit, authoritative statement
was found and is cited) or `REVIEW_REQUIRED` (terms are ambiguous,
conflicting, or no authoritative primary source could be found — this is
not a failure state to hide, it's the correct honest answer when that's
what's true).

## License class enum

`rights.license_class` is one of:

| Class | Meaning |
|---|---|
| `CC0` | Creative Commons Zero — public domain dedication |
| `PDDL` | ODC Public Domain Dedication and License |
| `PUBLIC_DOMAIN_VERIFIED` | Publisher explicitly states public domain, not via CC0/PDDL specifically |
| `US_FEDERAL_PUBLIC_DOMAIN` | Public domain via 17 U.S.C. §105 (U.S. federal government work) |
| `PERMISSIVE_ATTRIBUTION` | e.g. CC-BY — commercial/redistribution/modification fine, attribution required |
| `CDLA_PERMISSIVE` | Community Data License Agreement – Permissive |
| `ODBL` | Open Database License (OpenStreetMap's license) |
| `PROPRIETARY_APPROVED` | Not a public open license — specific negotiated/approved rights, reviewed case-by-case |
| `NONCOMMERCIAL` | Commercial use prohibited or restricted by the source terms |
| `NO_DERIVATIVES` | Modification/derivative works prohibited by the source terms |
| `UNKNOWN` | Not yet reviewed, or review inconclusive — never usable officially |

## What each class implies (enforced by the checker)

The checker's `LICENSE_CLASS_CONSTRAINTS` table is the single source of
truth; this table documents the same rules for a human reader — if they
ever disagree, the checker is authoritative and this table is stale and
needs fixing.

| Class | `commercial_use` | `redistribution` | `modification` | `attribution` | `share_alike` | `clean_pack_allowed` |
|---|---|---|---|---|---|---|
| `CC0` / `PDDL` / `PUBLIC_DOMAIN_VERIFIED` / `US_FEDERAL_PUBLIC_DOMAIN` | must be `true` | must be `true` | must be `true` | per-record | must be `false` | may be `true` |
| `PERMISSIVE_ATTRIBUTION` / `CDLA_PERMISSIVE` | per-record | per-record | per-record | per-record | must be `false` | must be `false` |
| `ODBL` | must be `true` | must be `true` | must be `true` | must be `true` | must be `true` | must be `false` |
| `PROPRIETARY_APPROVED` | per-record | per-record | per-record | per-record | must be `false` | must be `false` |
| `NONCOMMERCIAL` | must be `false` | per-record | per-record | per-record | per-record | must be `false` |
| `NO_DERIVATIVES` | per-record | per-record | must be `false` | per-record | per-record | must be `false` |
| `UNKNOWN` | per-record | per-record | per-record | per-record | per-record | must be `false`, and `official_pack_allowed` must be `false` |

"Per-record" means the checker doesn't force a value (the source's actual
terms decide it, and the reviewer records what they found) but the field
must still be present and boolean. Note that even a fully public-domain
class doesn't force `attribution_required: false` — a publisher can have
no copyright basis to demand credit (there's nothing to license) and still
*request or contractually require* a citation as a term of use for
repackaged distribution (U.S. Census TIGER/Line is exactly this case: the
data is public domain, but the Bureau's terms of use ask for
acknowledgment when the data is repackaged). Public domain removes the
*copyright* basis for restriction, not necessarily every other basis a
publisher's terms of use might state.

Additional rules the checker enforces beyond this table:

- `NONCOMMERCIAL` sources: `policy.official_pack_allowed` must be `false`
  (standard official packs are assumed to be usable commercially; a
  noncommercial-only source doesn't fit that model without a dedicated,
  explicitly-labeled pack type we haven't built yet).
- `UNKNOWN` sources: `policy.official_pack_allowed` must be `false`.
- **Any source with `evidence.confidence: "REVIEW_REQUIRED"` must have both
  `policy.official_pack_allowed: false` and `policy.clean_pack_allowed:
  false`, regardless of license class.** A
  plausible-but-unconfirmed classification is not a basis for official use
  — see `DATA_AND_LICENSING.md`'s core rule. Several of the registry's
  current records are `REVIEW_REQUIRED` for exactly this reason: an
  authoritative rights page could not be directly loaded/confirmed during
  research, even though the likely answer is favorable.
- Source `id` values are unique across every file in `data/sources/`.
- `evidence.rights_url` and `source.url` must look like URLs (`http://` or
  `https://`).
- `evidence.reviewed_date` and `source.retrieved` must be `YYYY-MM-DD`.

## Pack manifests reference registry IDs, not free text

Once pack manifests exist (`PACK_MANIFEST_SCHEMA.md`, not yet built — see
`ROADMAP.md`), each manifest's source list will reference these records by
`id`, and the checker will (once manifests exist) verify every referenced
ID actually exists in the registry and that the manifest's own license
classification is consistent with its sources' classes (e.g. a manifest
claiming `pack_class: clean` can't reference a source whose
`clean_pack_allowed` is `false`). This part of the checker is not yet
active — there is nothing to check yet, per `STATUS.md` — but the rule is
documented here so it isn't invented differently later.

## Current records

See [`../data/sources/`](../data/sources/) for the live, checker-validated
list. As of this writing: see `STATUS.md` for exactly which sources are
`CONFIRMED` vs. `REVIEW_REQUIRED` — this document doesn't duplicate that
list because it would drift; the registry files themselves and
`STATUS.md`'s summary are the sources of truth.
