# Data and licensing

This document is the map-**data** counterpart to [`../LICENSING.md`](../LICENSING.md)
(which covers engine code, and which you should also read — they are
deliberately independent documents about deliberately independent legal
layers). It's written for a developer, not a lawyer. Nothing in this
document is legal advice; it explains the terms we've reviewed and the
policy we follow, so any future contributor can see the reasoning instead
of having to re-derive it.

We deliberately avoid phrases like "zero legal risk" or "copyright risk
free" anywhere in this project. Nothing is ever risk-free. What we can
actually say, and do say, is **"verified low-risk source"** or **"approved
for official OrcMaps distribution under the documented source terms."**
That's a claim we can back up with an evidence URL and a review date. "Risk
free" isn't.

## The core rule: known provenance, not assumed safety

> Every external code dependency, dataset, asset, font, icon set, schema,
> map style source, build-time tool, or other third-party material must
> have known provenance and known usage rights before it is incorporated
> into official OrcMaps source or official OrcMaps map packs. Unknown
> licensing is rejected rather than assumed safe.

This applies identically whether the material was found by a human
browsing GitHub or by an AI agent generating code — see "AI-assisted
development" below. It also applies regardless of *who* published
something: a `.gov` domain, a well-known foundation, or a popular open-data
project are all starting points for review, not automatic approvals. See
"Government data isn't automatically safe" below.

We are **not** banning any particular license, including ODbL (OpenStreetMap's
license). We are banning *ambiguity*. A dataset under a clearly-understood
license with real obligations (attribution, share-alike) is approvable. A
dataset whose terms we haven't actually read is not, no matter how
reputable the source looks.

## Code and data are different intellectual-property layers

This is a hard architectural and conceptual boundary, not just a licensing
footnote:

```
OrcMaps C++ engine (AGPL-3.0-only / potential commercial licensing)
        |
        | reads
        v
MapSource (PMTiles archive, MVT-encoded tiles)
        |
        | the DATA inside has its own, separate license
```

- A map pack being ODbL does **not** make the OrcMaps engine ODbL.
- A public-domain map source does **not** make the OrcMaps engine public
  domain.
- A future commercial OrcMaps engine license would **not** make OSM data
  (or any other source's data) proprietary — ODbL data stays ODbL no
  matter who's selling access to the engine that reads it.

Engine licensing decisions and data licensing decisions are made, tracked,
and reviewed independently. Never let one drive the other.

## Rule: no bulk tile scraping

OrcMaps map packs must never be built by bulk-downloading a raster tile
service such as `tile.openstreetmap.org`. OSM's public tile service
explicitly prohibits using bulk tile downloads to construct offline
archives for redistribution — this is a *usage-policy* violation, separate
from and in addition to any copyright question about the underlying data.
The same caution applies to any other commercial map tile service (Google
Maps, Apple Maps, Bing Maps, etc.) — official OrcMaps builders must not
scrape rendered tiles from any of these unless we hold explicit rights
covering that exact offline-redistribution use, which we do not today for
any of them.

**Raw source data and a provider's rendered-tile service are legally and
operationally different things.** OpenStreetMap's underlying *database* is
ODbL-licensed and can be legitimately extracted and redistributed by
following ODbL's terms. OpenStreetMap's public *tile-rendering
infrastructure* (`tile.openstreetmap.org`) is a separate, donated service
with its own usage policy that bulk offline extraction violates regardless
of the data's own license. We use the former, never the latter.

Approved acquisition paths:

- Raw OpenStreetMap `.osm.pbf` extracts (e.g. Geofabrik regional extracts,
  or the full OSM planet file) processed with a proper extraction/tiling
  pipeline (osmium, Planetiler, tippecanoe, or equivalent).
- Any other data source whose terms explicitly permit offline
  redistribution, evaluated and documented per-source in the provenance
  registry (see below) before use.

## Government data isn't automatically safe

U.S. federal government works are automatically public domain under
17 U.S.C. § 105 — this is real and it's a genuinely strong, simple basis
for a dataset (see "Approved sources" below for confirmed examples). But
this fact gets over-applied in casual conversation, and we correct that
here explicitly:

- It only applies to the **U.S. federal** government. State/local
  government data, and other countries' government data (UK Ordnance
  Survey, for instance), generally follow *different* rules and are often
  copyrighted (e.g. Crown copyright).
- Even within U.S. federal data, **we approve specific datasets, not
  agencies.** "It's on a `.gov` domain" or "USGS published it" is not a
  license determination — a given USGS product might bundle in
  third-party-licensed data, or a given page might describe a program
  rather than a dataset's actual terms. Every dataset we use gets its own
  provenance record with its own evidence URL. See "Data provenance
  registry" below.

## Data-license classification system

Every dataset we might use gets classified along explicit properties, not
reduced to a single license string. The full field list and JSON schema
live in [`DATA_PROVENANCE_REGISTRY.md`](DATA_PROVENANCE_REGISTRY.md); the
short version:

**License class** (one value per source, chosen from a fixed enum):
`CC0`, `PDDL`, `PUBLIC_DOMAIN_VERIFIED`, `US_FEDERAL_PUBLIC_DOMAIN`,
`PERMISSIVE_ATTRIBUTION`, `CDLA_PERMISSIVE`, `ODBL`, `PROPRIETARY_APPROVED`,
`NONCOMMERCIAL`, `NO_DERIVATIVES`, `UNKNOWN`.

**Properties recorded per source**, all independently:
`commercial_use_allowed`, `redistribution_allowed`, `modification_allowed`,
`attribution_required`, `share_alike_required`, `source_disclosure_required`,
`official_pack_allowed`, `clean_pack_allowed`.

A source's license class **implies** most of these properties (and the
Data Provenance Truth checker enforces that implication — see below), but
they're stored explicitly rather than derived at read time, so a reviewer
can see exactly what was determined without re-deriving it from the class
name.

## Map-pack policy classes

Not every OrcMaps map pack carries the same legal weight, so we don't treat
them as legally equivalent. Three classes:

### OrcMaps Clean

Sources with extremely low licensing burden: `CC0`, `PDDL`,
`PUBLIC_DOMAIN_VERIFIED`, `US_FEDERAL_PUBLIC_DOMAIN`. No attribution
requirement, no share-alike, generally usable without restriction,
including exclusively/commercially if we ever wanted to. A Clean pack must
contain **only** Clean-class sources — an ODbL layer never enters a Clean
pack, even mixed with mostly-clean data. See "Keep license classes
separable" below.

### OrcMaps Permissive

Commercially usable sources with manageable obligations (typically
attribution) but no database share-alike requirement: `PERMISSIVE_ATTRIBUTION`,
`CDLA_PERMISSIVE`. Attribution must be shown; the pack itself doesn't
carry a "you must let others redistribute this" obligation.

### OrcMaps Open

Open-database sources with stronger obligations — today, this means
`ODBL` (OpenStreetMap). Allowed, and expected to be a primary source for
detail Clean/Permissive sources can't provide. Must remain clearly
identified as Open-class; never silently blended into a Clean pack's
metadata.

`PROPRIETARY_APPROVED` sources (data we've obtained under specific
negotiated rights, not any public open license) and `NONCOMMERCIAL`/
`NO_DERIVATIVES` sources are handled case-by-case and are **not** eligible
for standard official packs by default — see the checker rules below.
`UNKNOWN` is never eligible for anything official.

## Keep license classes separable

Where practical, we do not build one giant archive blending sources under
radically different obligations. The intended shape (see
[`../ARCHITECTURE.md`](../ARCHITECTURE.md) for the composition mechanism
once it exists):

```
world-base.pmtiles      <- Natural Earth (Clean)
terrain.pmtiles         <- NOAA ETOPO (Clean, pending review)
us-roads.pmtiles        <- Census TIGER/USGS (Clean)
places.pmtiles          <- a permissive-license places dataset
osm-detail.pmtiles      <- OpenStreetMap (Open)
```

OrcMaps is meant to compose multiple `MapSource`s at render time rather
than forcing everything into one file. This isn't a hard rule if
performance testing later shows physical separation is unreasonable for
some target — but the *logical* source/license boundary must survive in
metadata even if physical packaging changes, so provenance and attribution
never become ambiguous just because a build step merged files together.

## Data provenance registry

Every dataset we've reviewed has a machine-readable record under
[`data/sources/`](../data/sources/) (JSON, not YAML — see
[`DATA_PROVENANCE_REGISTRY.md`](DATA_PROVENANCE_REGISTRY.md) for why, and
for the full schema and worked examples). `tools/check_data_provenance.py`
validates every record and enforces the rules that make this more than
documentation:

- `UNKNOWN` sources cannot be marked `official_pack_allowed`.
- `NONCOMMERCIAL` sources cannot enter standard official packs.
- `ODBL` sources cannot be marked `clean_pack_allowed`.
- Every record needs a real evidence URL and a review date — no
  undocumented approvals.
- Source IDs are unique.
- A record's boolean properties must actually match what its license class
  implies (e.g. a record claiming `CC0` can't also claim
  `share_alike_required: true`).

See [`DATA_PROVENANCE_REGISTRY.md`](DATA_PROVENANCE_REGISTRY.md) for the
current list of reviewed sources, what's approved, and what's flagged
`REVIEW_REQUIRED`.

## Required pack provenance metadata

Every generated map pack must carry, in its manifest — see
[`PACK_MANIFEST_SCHEMA.md`](PACK_MANIFEST_SCHEMA.md) for the full schema —
enough to trace every byte back to a reviewed source: pack ID/version,
region/bounds/zoom range, schema versions, builder tool + Git commit +
build date, the source dataset(s) used (by provenance registry ID, with
their own versions/acquisition dates), the pack's overall license
classification, required attribution text, and input/output content
hashes. None of this is baked into tile geometry — it lives in the
manifest, alongside the pack, not inside it.

The first real triplet is the committed Springfield hardware-regression pack
under `examples/m5stack-tab5/test-pack/`. Data Provenance Truth checks its
source references and policy eligibility. Runtime JSON parsing and streamed
archive hashing remain separate unfinished runtime work; the presence of a
sidecar alone is not yet an on-device validation claim.

## Runtime attribution

Attribution is not documentation-only. The engine is meant to expose which
active map source(s) require attribution and what text to show, so a
consuming application (OrcSDR or anyone else) never has to hard-code
`"© OpenStreetMap contributors"` — it asks the engine instead. See
[`../ARCHITECTURE.md`](../ARCHITECTURE.md) "Attribution handling" for the
current (early, header-only) state of this API
(`orcmap::AttributionInfo`, `include/orcmap/attribution.hpp`) and what's
still unbuilt. The core engine does not render attribution text itself
(that would constrain UI flexibility) — it exposes the requirement, and
the application/renderer adapter decides how to display it.

## OpenStreetMap / ODbL, in plain terms

See the project's own plain-language framing (this is the same thing,
written down): OSM data is licensed under the Open Database License
(ODbL). Two obligations follow anyone who redistributes it:

1. **Attribution.** Show "© OpenStreetMap contributors" (or an equivalent
   credit) wherever the map is displayed.
2. **Share-alike, on the data itself — not on code that merely reads it.**
   If we hand someone a map pack built from OSM data (not just a picture
   of a rendered map, but the actual queryable geometry/attributes), they
   are entitled to redistribute that pack further. We can still sell
   convenience (curation, bandwidth, a nice download experience) — ODbL
   permits commercial sale — but we cannot make an OSM-derived pack
   *exclusive*. This constrains data distribution, not engine licensing:
   see "Code and data are different intellectual-property layers" above.

This is why the Clean/Permissive/Open pack classes exist: so a user (or an
OEM partner) can choose "give me only the packs with the least-restrictive
terms" without us having to explain ODbL to every consumer of the project.

## Schema obligations are separate from data obligations

A pack has **two** independent intellectual-property layers, and crediting
one does not discharge the other:

| Layer | Example | License | Obligation |
|---|---|---|---|
| Map data | OpenStreetMap extract | ODbL 1.0 | credit **+** share-alike on the data |
| Tile schema / production | OpenMapTiles 3.16 via Planetiler | CC BY 4.0 grant | credit only, **no** share-alike |

So any pack whose `schema_version` begins `openmaptiles-` must display
**both**:

```
© OpenMapTiles            https://openmaptiles.org/
© OpenStreetMap contributors   https://www.openstreetmap.org/copyright
```

Planetiler prints this requirement in its own build output: generated tiles
are "reusable under CC-BY license granted by OpenMapTiles team" and "maps
made with these vector tiles must display a visible credit". Registry
record: `data/sources/openmaptiles.json` (`CONFIRMED`, reviewed
2026-09-14, class `PERMISSIVE_ATTRIBUTION`).

**This does not apply to `orcmaps-overview-1`.** That profile is Natural
Earth through an OrcMaps-native schema and owes OpenMapTiles nothing;
forcing the credit onto it would be a false claim of provenance. The world
overview pack therefore stays Clean-class with no required attribution.

Neither obligation touches engine licensing — see "Code and data are
different intellectual-property layers".

**Enforced, not just documented.** `tools/check_data_provenance.py`
(`SCHEMA_ATTRIBUTION_REQUIREMENTS`) fails any committed `openmaptiles-*`
manifest missing the provenance id, the visible credit, or the link, and
`tools/pack-builder/build_regional_pack.py` (`SCHEMA_ATTRIBUTION`) adds them
to every manifest it emits, because the builder — not the caller — knows
which schema it produced. Keep those two tables in sync.

> **Defect history.** Until 2026-09-14 the Springfield demo pack credited
> only OpenStreetMap despite being built with the OpenMapTiles profile, and
> its credit text was double-encoded (`Â©`). The device therefore displayed
> an incomplete credit. Fixed, and both classes of error are now CI failures.

## Contribution / AI-assisted development safeguard

Because AI agents (Claude, Codex, ChatGPT, or any other) are doing
substantial development work on this project, this rule is explicit and
durable:

> AI-generated code or data does not bypass provenance requirements. If an
> AI agent finds a GitHub implementation, a Stack Overflow snippet, a map
> style, a dataset, a font, an icon, or a binary fixture and wants to
> incorporate it, the normal provenance/license review still applies. "An
> AI generated it for us" is never proof that the underlying source
> material is ours to use — the AI is doing the research and drafting the
> record, the same review standard applies regardless of who typed it.

If provenance or license terms are unclear, the answer is
`status: REVIEW_REQUIRED` (or `UNKNOWN` if not yet reviewed at all), never
a guess written down as if it were confirmed.

## Fonts, icons, and other non-map assets

The provenance discipline above isn't limited to geographic data. Fonts,
icon/symbol sets, sprites, logos, SVGs, sample/test map fixtures — anything
not written by an OrcMaps contributor from scratch — gets the same
treatment: known source, known license, recorded before use. Symbols we
design ourselves (e.g. any future RF/map iconography) are recorded as
OrcMaps-owned assets, which is itself worth writing down so a later
reviewer doesn't have to guess whether something was original or borrowed.

## Styles remain independently designed

The four built-in styles (`docs/STYLING.md`) are OrcMaps' own color/rule
choices, not reproductions of Google Maps, Apple Maps, Mapbox Streets,
CARTO, or any other commercial style. Visual *inspiration* (dark mode
generally looks like X) is fine; copying an actual style specification
(a Mapbox GL style JSON, for instance) is not, and no such copying has
happened — the built-in styles were designed from a written brief, not
derived from an existing style file. Any future externally-derived style
resource goes through the same provenance discipline as data and code.

## Engine license vs. data license — do not conflate

- Engine code: AGPL-3.0 / commercial (see [`../LICENSING.md`](../LICENSING.md)).
- Map data: whatever its source dictates, tracked per-source in the
  provenance registry. A commercial engine license does **not** grant
  different rights over ODbL (or any other) data, and AGPL engine code
  does not force map data into any particular license — they are reviewed
  and tracked completely independently.

## Per-pack ledger

Following the same pattern as OrcSDR's *docs/DATA_SOURCE_LEDGER.md* (a file
in the `hardcoreerik/OrcSDR` repository, not this one), every published map
pack should have a ledger entry recording: publisher, retrieval method,
retrieval timestamp, license/terms review, exact transformation command +
tool version used to build it, SHA-256, and a removal/takedown contact.
This ledger is created alongside the first real published pack (tracked as
part of the Lane County vertical slice, `ROADMAP.md` Phase 3) — not yet
populated here since no pack has been built through the new pipeline yet.
The per-source provenance registry (`data/sources/`) and this per-*pack*
ledger are complementary, not duplicates: the registry says "this dataset
is safe to use and under what terms," the ledger says "this specific pack,
built on this date, from these exact source versions, produced this exact
file."
