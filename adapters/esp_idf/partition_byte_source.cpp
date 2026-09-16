#include "orcmap/esp_idf/partition_byte_source.hpp"

#include "orcmap/byte_range.hpp"

#include "esp_partition.h"

namespace orcmap {
namespace esp_idf {

PartitionByteSource::PartitionByteSource(const char* label) {
  if (label == nullptr || label[0] == '\0') return;
  // ESP_PARTITION_SUBTYPE_ANY: the application owns its partition table and
  // may give a map partition whatever data subtype it likes. Matching on the
  // label alone keeps that choice out of OrcMaps.
  const esp_partition_t* partition = esp_partition_find_first(
      ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, label);
  if (partition == nullptr) return;
  partition_ = partition;
  size_ = partition->size;
}

size_t PartitionByteSource::Read(uint64_t offset, void* destination,
                                 size_t length) {
  if (partition_ == nullptr || destination == nullptr) return 0;
  size_t readable = 0;
  if (!ClampReadRange(offset, length, size_, &readable)) return 0;

  // esp_partition_read takes a size_t offset relative to the partition, so a
  // 64-bit offset that survived clamping still has to fit. It always does
  // here -- clamping proved offset < size_, and a partition size is a 32-bit
  // quantity -- but the check is cheap and keeps the cast honest.
  if (offset > static_cast<uint64_t>(SIZE_MAX)) return 0;

  const esp_partition_t* partition =
      static_cast<const esp_partition_t*>(partition_);
  if (esp_partition_read(partition, static_cast<size_t>(offset), destination,
                         readable) != ESP_OK) {
    return 0;
  }
  bytes_read_ += readable;
  return readable;
}

}  // namespace esp_idf
}  // namespace orcmap
