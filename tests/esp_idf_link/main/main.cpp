// Links every OrcMaps path that inflates: PMTiles directory reads
// (DecompressPayload, via Open), streamed tiles (InflatingByteStream, via
// StreamTile) and buffered tiles (DecompressStreaming, via GetTileInflated).
// Never flashed; tools/check_miniz_symbols.py inspects the linked ELF.
#include <vector>

#include "orcmap/esp_idf/partition_byte_source.hpp"
#include "orcmap/mvt_stream.hpp"
#include "orcmap/pmtiles.hpp"

namespace {
bool Discard(const orcmap::Feature&, void*) { return true; }
}  // namespace

extern "C" void app_main(void) {
  orcmap::esp_idf::PartitionByteSource source("orcmaps_world");
  orcmap::PmTilesReader reader(&source, 1);
  if (!reader.Open()) return;

  orcmap::MvtStreamScratch scratch;
  orcmap::MvtStreamOptions options;
  options.feature_sink = &Discard;
  reader.StreamTile(0, 0, 0, options, &scratch);

  std::vector<uint8_t> tile;
  reader.GetTileInflated(0, 0, 0, 1u << 20, &tile);
}
