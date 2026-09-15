#include "orcmap/pack_json.hpp"

#include <cmath>
#include <cstdlib>
#include <map>
#include <vector>

namespace orcmap {
namespace {

// Minimal JSON value model, private to this file. Kept internal on purpose:
// OrcMaps does not expose a JSON API, so nothing outside can depend on it.
struct Value;
using Object = std::map<std::string, Value>;
using Array = std::vector<Value>;

enum class Type { kNull, kBool, kNumber, kString, kArray, kObject };

struct Value {
  Type type = Type::kNull;
  bool boolean = false;
  double number = 0.0;
  std::string text;
  Array array;
  Object object;
};

// Recursive-descent parser with an explicit depth limit. Every failure path
// returns false; nothing is guessed or repaired.
class Parser {
 public:
  Parser(std::string_view text) : text_(text) {}

  bool ParseDocument(Value* out) {
    SkipSpace();
    if (!ParseValue(out, 0)) return false;
    SkipSpace();
    return pos_ == text_.size();
  }

  bool too_deep() const { return too_deep_; }

 private:
  void SkipSpace() {
    while (pos_ < text_.size()) {
      const char c = text_[pos_];
      if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
        ++pos_;
      } else {
        break;
      }
    }
  }

  bool Literal(std::string_view word) {
    if (text_.compare(pos_, word.size(), word) != 0) return false;
    pos_ += word.size();
    return true;
  }

  bool ParseValue(Value* out, int depth) {
    if (depth > kMaxPackManifestDepth) {
      too_deep_ = true;
      return false;
    }
    if (pos_ >= text_.size()) return false;
    switch (text_[pos_]) {
      case '{': return ParseObject(out, depth);
      case '[': return ParseArray(out, depth);
      case '"':
        out->type = Type::kString;
        return ParseString(&out->text);
      case 't':
        if (!Literal("true")) return false;
        out->type = Type::kBool;
        out->boolean = true;
        return true;
      case 'f':
        if (!Literal("false")) return false;
        out->type = Type::kBool;
        out->boolean = false;
        return true;
      case 'n':
        if (!Literal("null")) return false;
        out->type = Type::kNull;
        return true;
      default:
        out->type = Type::kNumber;
        return ParseNumber(&out->number);
    }
  }

  bool ParseObject(Value* out, int depth) {
    out->type = Type::kObject;
    ++pos_;  // '{'
    SkipSpace();
    if (pos_ < text_.size() && text_[pos_] == '}') {
      ++pos_;
      return true;
    }
    for (;;) {
      SkipSpace();
      std::string key;
      if (pos_ >= text_.size() || text_[pos_] != '"') return false;
      if (!ParseString(&key)) return false;
      SkipSpace();
      if (pos_ >= text_.size() || text_[pos_] != ':') return false;
      ++pos_;
      SkipSpace();
      Value child;
      if (!ParseValue(&child, depth + 1)) return false;
      out->object[key] = std::move(child);
      SkipSpace();
      if (pos_ >= text_.size()) return false;
      if (text_[pos_] == ',') {
        ++pos_;
        continue;
      }
      if (text_[pos_] == '}') {
        ++pos_;
        return true;
      }
      return false;
    }
  }

  bool ParseArray(Value* out, int depth) {
    out->type = Type::kArray;
    ++pos_;  // '['
    SkipSpace();
    if (pos_ < text_.size() && text_[pos_] == ']') {
      ++pos_;
      return true;
    }
    for (;;) {
      SkipSpace();
      Value child;
      if (!ParseValue(&child, depth + 1)) return false;
      out->array.push_back(std::move(child));
      SkipSpace();
      if (pos_ >= text_.size()) return false;
      if (text_[pos_] == ',') {
        ++pos_;
        continue;
      }
      if (text_[pos_] == ']') {
        ++pos_;
        return true;
      }
      return false;
    }
  }

  bool ParseString(std::string* out) {
    out->clear();
    if (pos_ >= text_.size() || text_[pos_] != '"') return false;
    ++pos_;
    while (pos_ < text_.size()) {
      const char c = text_[pos_++];
      if (c == '"') return true;
      if (c != '\\') {
        out->push_back(c);
        continue;
      }
      if (pos_ >= text_.size()) return false;
      const char esc = text_[pos_++];
      switch (esc) {
        case '"': out->push_back('"'); break;
        case '\\': out->push_back('\\'); break;
        case '/': out->push_back('/'); break;
        case 'b': out->push_back('\b'); break;
        case 'f': out->push_back('\f'); break;
        case 'n': out->push_back('\n'); break;
        case 'r': out->push_back('\r'); break;
        case 't': out->push_back('\t'); break;
        case 'u': {
          if (pos_ + 4 > text_.size()) return false;
          uint32_t cp = 0;
          for (int i = 0; i < 4; ++i) {
            const char h = text_[pos_++];
            cp <<= 4;
            if (h >= '0' && h <= '9') cp |= static_cast<uint32_t>(h - '0');
            else if (h >= 'a' && h <= 'f') cp |= static_cast<uint32_t>(h - 'a' + 10);
            else if (h >= 'A' && h <= 'F') cp |= static_cast<uint32_t>(h - 'A' + 10);
            else return false;
          }
          // Encode as UTF-8. Surrogate halves are passed through as the
          // replacement character rather than being paired: manifests have
          // no need for astral-plane text, and guessing would be worse.
          if (cp < 0x80) {
            out->push_back(static_cast<char>(cp));
          } else if (cp < 0x800) {
            out->push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out->push_back(static_cast<char>(0x80 | (cp & 0x3F)));
          } else if (cp >= 0xD800 && cp <= 0xDFFF) {
            out->append("\xEF\xBF\xBD");
          } else {
            out->push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out->push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out->push_back(static_cast<char>(0x80 | (cp & 0x3F)));
          }
          break;
        }
        default:
          return false;
      }
    }
    return false;
  }

  bool ParseNumber(double* out) {
    const size_t start = pos_;
    if (pos_ < text_.size() && (text_[pos_] == '-' || text_[pos_] == '+')) ++pos_;
    bool any = false;
    while (pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9') {
      ++pos_;
      any = true;
    }
    if (pos_ < text_.size() && text_[pos_] == '.') {
      ++pos_;
      while (pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9') {
        ++pos_;
        any = true;
      }
    }
    if (!any) return false;
    if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E')) {
      ++pos_;
      if (pos_ < text_.size() && (text_[pos_] == '-' || text_[pos_] == '+')) ++pos_;
      bool exp_digits = false;
      while (pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9') {
        ++pos_;
        exp_digits = true;
      }
      if (!exp_digits) return false;
    }
    const std::string token(text_.substr(start, pos_ - start));
    *out = std::strtod(token.c_str(), nullptr);
    return std::isfinite(*out);
  }

  std::string_view text_;
  size_t pos_ = 0;
  bool too_deep_ = false;
};

const Value* Find(const Object& object, const char* key) {
  const auto it = object.find(key);
  return it == object.end() ? nullptr : &it->second;
}

// Field readers. A missing key is "absent" (the caller decides whether that
// is fatal); a present key of the wrong type is always an error.
enum class Read { kOk, kAbsent, kWrongType, kOutOfRange };

Read ReadString(const Object& object, const char* key, std::string* out) {
  const Value* value = Find(object, key);
  if (value == nullptr) return Read::kAbsent;
  if (value->type != Type::kString) return Read::kWrongType;
  *out = value->text;
  return Read::kOk;
}

Read ReadDouble(const Object& object, const char* key, double* out) {
  const Value* value = Find(object, key);
  if (value == nullptr) return Read::kAbsent;
  if (value->type != Type::kNumber) return Read::kWrongType;
  *out = value->number;
  return Read::kOk;
}

// Integers arrive as JSON numbers; reject anything non-integral or outside
// the destination's range rather than truncating it.
Read ReadIntegral(const Object& object, const char* key, double min, double max,
                  double* out) {
  double raw = 0.0;
  const Read status = ReadDouble(object, key, &raw);
  if (status != Read::kOk) return status;
  if (raw != std::floor(raw) || raw < min || raw > max) return Read::kOutOfRange;
  *out = raw;
  return Read::kOk;
}

Read ReadStringArray(const Object& object, const char* key,
                      std::vector<std::string>* out) {
  const Value* value = Find(object, key);
  if (value == nullptr) return Read::kAbsent;
  if (value->type != Type::kArray) return Read::kWrongType;
  out->clear();
  for (const Value& item : value->array) {
    if (item.type != Type::kString) return Read::kWrongType;
    out->push_back(item.text);
  }
  return Read::kOk;
}

PackJsonError ToError(Read status) {
  switch (status) {
    case Read::kWrongType: return PackJsonError::kWrongType;
    case Read::kOutOfRange: return PackJsonError::kOutOfRange;
    case Read::kAbsent: return PackJsonError::kMissingField;
    case Read::kOk: break;
  }
  return PackJsonError::kNone;
}

}  // namespace

const char* PackJsonErrorName(PackJsonError error) {
  switch (error) {
    case PackJsonError::kNone: return "none";
    case PackJsonError::kTooLarge: return "too-large";
    case PackJsonError::kTooDeep: return "too-deep";
    case PackJsonError::kMalformed: return "malformed";
    case PackJsonError::kNotAnObject: return "not-an-object";
    case PackJsonError::kWrongType: return "wrong-type";
    case PackJsonError::kMissingField: return "missing-field";
    case PackJsonError::kOutOfRange: return "out-of-range";
  }
  return "unknown";
}

PackJsonError ParsePackManifestJson(std::string_view text, PackManifest* out) {
  if (out == nullptr) return PackJsonError::kMalformed;
  if (text.size() > kMaxPackManifestBytes) return PackJsonError::kTooLarge;

  Value document;
  Parser parser(text);
  if (!parser.ParseDocument(&document)) {
    return parser.too_deep() ? PackJsonError::kTooDeep
                             : PackJsonError::kMalformed;
  }
  if (document.type != Type::kObject) return PackJsonError::kNotAnObject;
  const Object& root = document.object;

  PackManifest manifest;

  // Required strings. Every one of these is also required by
  // ValidatePackManifest, so a manifest missing any is unusable anyway --
  // failing here just names the problem more precisely.
  struct StringField {
    const char* key;
    std::string* target;
  };
  const StringField strings[] = {
      {"pack_id", &manifest.pack_id},
      {"pack_version", &manifest.pack_version},
      {"display_name", &manifest.display_name},
      {"region_id", &manifest.region_id},
      {"region_name", &manifest.region_name},
      {"content_profile", &manifest.content_profile},
      {"schema_version", &manifest.schema_version},
      {"source_snapshot", &manifest.source_snapshot},
      {"builder", &manifest.builder},
      {"builder_version", &manifest.builder_version},
      {"builder_commit", &manifest.builder_commit},
      {"build_date", &manifest.build_date},
      {"pack_class", &manifest.pack_class},
      {"output_sha256", &manifest.output_sha256},
  };
  for (const StringField& field : strings) {
    const Read status = ReadString(root, field.key, field.target);
    if (status != Read::kOk) return ToError(status);
  }

  // Bounds.
  const Value* bounds = Find(root, "bounds");
  if (bounds == nullptr) return PackJsonError::kMissingField;
  if (bounds->type != Type::kObject) return PackJsonError::kWrongType;
  const struct {
    const char* key;
    double* target;
  } bounds_fields[] = {
      {"min_lon", &manifest.bounds.min_lon_deg},
      {"min_lat", &manifest.bounds.min_lat_deg},
      {"max_lon", &manifest.bounds.max_lon_deg},
      {"max_lat", &manifest.bounds.max_lat_deg},
  };
  for (const auto& field : bounds_fields) {
    const Read status = ReadDouble(bounds->object, field.key, field.target);
    if (status != Read::kOk) return ToError(status);
  }

  // Integers.
  double value = 0.0;
  Read status = ReadIntegral(root, "min_zoom", 0, 31, &value);
  if (status != Read::kOk) return ToError(status);
  manifest.min_zoom = static_cast<uint8_t>(value);
  status = ReadIntegral(root, "max_zoom", 0, 31, &value);
  if (status != Read::kOk) return ToError(status);
  manifest.max_zoom = static_cast<uint8_t>(value);
  status = ReadIntegral(root, "pmtiles_version", 0, 255, &value);
  if (status != Read::kOk) return ToError(status);
  manifest.pmtiles_version = static_cast<uint8_t>(value);
  status = ReadIntegral(root, "size_bytes", 0, 9007199254740992.0, &value);
  if (status != Read::kOk) return ToError(status);
  manifest.size_bytes = static_cast<uint64_t>(value);
  // priority is optional and defaults to 0.
  status = ReadIntegral(root, "priority", -2147483648.0, 2147483647.0, &value);
  if (status == Read::kOk) {
    manifest.priority = static_cast<int>(value);
  } else if (status != Read::kAbsent) {
    return ToError(status);
  }

  // Provenance ids come from sources[].provenance_id -- the manifest's own
  // shape -- rather than from a flat list, so the on-disk format and the
  // registry stay in step.
  const Value* sources = Find(root, "sources");
  if (sources == nullptr) return PackJsonError::kMissingField;
  if (sources->type != Type::kArray) return PackJsonError::kWrongType;
  for (const Value& source : sources->array) {
    if (source.type != Type::kObject) return PackJsonError::kWrongType;
    std::string id;
    const Read got = ReadString(source.object, "provenance_id", &id);
    if (got != Read::kOk) return ToError(got);
    manifest.provenance_ids.push_back(id);
  }

  // Attribution stays local to the manifest: the engine reports what the
  // pack itself declares and never synthesises a credit string.
  std::vector<std::string> credits;
  std::vector<std::string> links;
  status = ReadStringArray(root, "required_attribution", &credits);
  if (status == Read::kWrongType) return PackJsonError::kWrongType;
  status = ReadStringArray(root, "attribution_links", &links);
  if (status == Read::kWrongType) return PackJsonError::kWrongType;
  for (size_t i = 0; i < credits.size(); ++i) {
    AttributionInfo info;
    info.required = true;
    info.text = credits[i];
    if (i < links.size()) info.url = links[i];
    manifest.attribution.push_back(std::move(info));
  }

  *out = std::move(manifest);
  return PackJsonError::kNone;
}

}  // namespace orcmap
