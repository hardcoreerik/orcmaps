import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.onthegomap.planetiler.FeatureCollector;
import com.onthegomap.planetiler.Planetiler;
import com.onthegomap.planetiler.Profile;
import com.onthegomap.planetiler.config.Arguments;
import com.onthegomap.planetiler.reader.SourceFeature;
import java.nio.file.Path;
import java.util.HashMap;
import java.util.Map;

/** Minimal Natural Earth profile for OrcMaps' graphics-independent overview pack. */
public final class WorldOverviewProfile implements Profile {
  private record SourceSpec(String layer, String geometry, int minZoom, int maxZoom, int adminLevel) {}
  private final Map<String, SourceSpec> sources;

  private WorldOverviewProfile(Map<String, SourceSpec> sources) {
    this.sources = sources;
  }

  @Override public void processFeature(SourceFeature source, FeatureCollector features) {
    SourceSpec spec = sources.get(source.getSource());
    if (spec == null) return;
    FeatureCollector.Feature feature = switch (spec.geometry) {
      case "polygon" -> features.polygon(spec.layer);
      case "line" -> features.line(spec.layer);
      case "point" -> features.point(spec.layer);
      default -> throw new IllegalArgumentException("unsupported geometry: " + spec.geometry);
    };
    feature.setMinZoom(spec.minZoom).setMaxZoom(spec.maxZoom);
    Object name = first(source, "name", "NAME");
    Object scalerank = first(source, "scalerank", "SCALERANK");
    if (name != null && (spec.layer.equals("place") || spec.layer.equals("waterway"))) feature.setAttr("name", name);
    if (scalerank != null) feature.setAttr("scalerank", scalerank);
    if (spec.adminLevel >= 0) feature.setAttr("admin_level", spec.adminLevel);
  }

  private static Object first(SourceFeature source, String first, String second) {
    Object value = source.getTag(first);
    return value == null ? source.getTag(second) : value;
  }

  @Override public String name() { return "OrcMaps Natural Earth World Overview"; }
  @Override public String description() { return "Offline world overview for OrcMaps"; }
  @Override public String attribution() { return "Made with Natural Earth"; }
  @Override public String version() { return "1"; }
  @Override public Map<String, String> extraArchiveMetadata() {
    return Map.of("orcmaps_schema", "orcmaps-overview-1", "natural_earth_version", "5.1.2");
  }

  public static void main(String[] argv) throws Exception {
    Arguments args = Arguments.fromArgsOrConfigFile(argv);
    Path profilePath = args.file("profile_config", "OrcMaps profile JSON");
    Path sourcesPath = args.file("sources_config", "Pinned Natural Earth source JSON");
    Path sourceRoot = args.file("source_root", "Local Natural Earth root");
    Path output = args.file("output", "Output PMTiles");
    ObjectMapper mapper = new ObjectMapper();
    JsonNode profile = mapper.readTree(profilePath.toFile());
    JsonNode sourceList = mapper.readTree(sourcesPath.toFile());
    Map<String, SourceSpec> specs = new HashMap<>();
    Planetiler planetiler = Planetiler.create(args);
    for (JsonNode dataset : sourceList.path("datasets")) {
      String scale = dataset.path("scale").asText();
      String layer = dataset.path("use").asText();
      String archive = dataset.path("archive").asText();
      String id = archive.substring(0, archive.length() - 4);
      JsonNode zoom = profile.path("zoom_ranges").path(scale);
      String geometry = profile.path("layers").path(layer).path("geometry").asText();
      int admin = layer.equals("boundary") ? (id.contains("admin_0") ? 0 : 1) : -1;
      specs.put(id, new SourceSpec(layer, geometry, zoom.path("min").asInt(), zoom.path("max").asInt(), admin));
      Path shape = sourceRoot.resolve(scale).resolve("datasets").resolve(id).resolve(id + ".shp");
      planetiler.addShapefileSource(id, shape);
    }
    planetiler.setProfile(new WorldOverviewProfile(specs)).setOutput(output).run();
  }
}
