#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "orcmap/compression.hpp"
#include "orcmap/mvt.hpp"

namespace orcmap {

// END-TO-END STREAMING MVT PARSE: reads a compressed tile straight out of an
// archive and emits its features one at a time, without ever materialising
// the inflated tile.
//
// Why this exists, in measured numbers. On a CYD 3.5" (ESP32-3248S035, no
// PSRAM) the binding limit is the largest CONTIGUOUS free block, not total
// free memory: 190,264 bytes free but a 69,632-byte largest block, against a
// 111,366-byte inflated Oregon z7 tile. Buffering the tile therefore cannot
// work however it is obtained -- pre-reserving the buffer at startup only
// moved the problem and made it worse.
//
// This parser's footprint is a similar total but in SEPARATE, SMALL pieces:
//
//   inflate scratch (32 KiB window + ~8 KiB state + 2 KiB input)  ~43 KiB
//   all layers' key/value tables (worst measured: Oregon z7)       ~24 KiB
//   one feature's bytes (worst measured: Springfield z13)          ~36 KiB
//
// so no single allocation exceeds ~36 KiB and the whole thing fits in a
// fragmented heap that cannot supply one 111 KiB block.
//
// TWO passes, because MVT field order forbids one. Measured across the
// project's own packs, 167 of 173 layers emit features (field 2) BEFORE keys
// (3) and values (4) -- ascending protobuf field order, as Planetiler writes
// -- so a feature's attribute indices cannot be resolved when it is first
// seen. Pass 1 skips features and collects the tables; pass 2 decodes and
// emits features. The payload is inflated twice, which trades CPU for the
// contiguous memory the board does not have.

// Reusable scratch. Allocate ONE of these and keep it: every buffer inside
// is reused across tiles, so a steady-state frame performs no large
// allocation at all.
struct MvtStreamScratch {
  // Per-layer tables gathered in pass 1. Only included layers keep their
  // tables; excluded layers keep just enough to be skipped in pass 2.
  struct LayerTables {
    std::string name;
    uint32_t extent = 4096;
    uint32_t version = 1;
    bool included = false;
    std::vector<std::string> keys;
    std::vector<MvtValue> values;
  };

  InflatingByteStream stream;
  std::vector<LayerTables> layers;
  std::vector<uint8_t> feature_bytes;  // one feature at a time
  MvtFeature feature;                  // reused
  MvtLayer layer;                      // name/extent/version for the sink
};

// Refuses a single feature larger than this rather than growing without
// bound on hostile input. The largest feature measured in this project's
// packs is 36,259 bytes.
inline constexpr size_t kMaxStreamedFeatureBytes = 256u * 1024u;

// Parses the compressed tile at [input_offset, input_offset + input_size)
// pulled through `reader`, emitting each feature to
// `options.feature_sink` (which must be set). `options.include_layer` is
// honoured. Returns false on malformed input, on a feature exceeding
// kMaxStreamedFeatureBytes, or if the sink refuses.
bool StreamMvtTile(Compression compression, CompressedChunkReader reader,
                   void* ctx, uint64_t input_offset, size_t input_size,
                   const MvtDecodeOptions& options, MvtStreamScratch* scratch);

}  // namespace orcmap
