# Build data/local/springfield-97477.pmtiles from the Geofabrik Oregon OSM
# extract via Planetiler. Host pack-production only -- not a runtime dep.
$ErrorActionPreference = "Stop"

$Root = Resolve-Path (Join-Path $PSScriptRoot "../..")
$Local = Join-Path $Root "data/local"
New-Item -ItemType Directory -Force -Path $Local | Out-Null

$OsmUrl = "https://download.geofabrik.de/north-america/us/oregon-latest.osm.pbf"
$OsmPath = Join-Path $Local "oregon-latest.osm.pbf"
$OutPath = Join-Path $Local "springfield-97477.pmtiles"
$SourceLog = Join-Path $Local "SOURCE.txt"

# Springfield / 97477 urban test bbox (W,S,E,N). Not an OrcMaps core constant.
$Bounds = "-123.055,44.030,-122.960,44.090"
$PlanetilerJarUrl = "https://github.com/onthegomap/planetiler/releases/download/v0.10.2/planetiler.jar"
$PlanetilerJar = Join-Path $Local "planetiler.jar"
$PlanetilerVersion = "0.10.2 (git 0e5588c4a6e8c29a270a33afe8df62027d889604)"

if (-not (Test-Path $OsmPath)) {
    Write-Host "Downloading $OsmUrl"
    curl.exe -L --fail --retry 3 -o $OsmPath $OsmUrl
}

$Hash = (Get-FileHash -Algorithm SHA256 $OsmPath).Hash.ToLowerInvariant()
$Bytes = (Get-Item $OsmPath).Length
$When = (Get-Date).ToUniversalTime().ToString("yyyy-MM-dd")

@"
source_url=$OsmUrl
download_date_utc=$When
source_format=osm.pbf
sha256=$Hash
bytes=$Bytes
license=ODbL 1.0
provenance_id=openstreetmap
geographic_bounds_wsen=$Bounds
tiler=planetiler
tiler_version=$PlanetilerVersion
tile_type=mvt
tile_compression=gzip
maxzoom=15
"@ | Set-Content -Path $SourceLog -Encoding utf8

Write-Host "SOURCE.txt written ($Bytes bytes, sha256=$Hash)"

if (-not (Test-Path $PlanetilerJar)) {
    Write-Host "Downloading $PlanetilerJarUrl"
    curl.exe -L --fail --retry 3 -o $PlanetilerJar $PlanetilerJarUrl
}

$Java = Get-ChildItem "C:\Program Files\Eclipse Adoptium" -Directory |
    Where-Object { $_.Name -like "jdk-21*" } |
    Select-Object -First 1
if (-not $Java) { throw "Java 21 (Temurin) is required to run Planetiler 0.10.2" }
$JavaExe = Join-Path $Java.FullName "bin\java.exe"

Push-Location $Local
try {
    & $JavaExe -Xmx4g -jar $PlanetilerJar `
        --osm-path=$OsmPath `
        --bounds=$Bounds `
        --output=$OutPath `
        --maxzoom=15 `
        --render_maxzoom=15 `
        --tile_compression=gzip `
        --download `
        --force
    if ($LASTEXITCODE -ne 0) { throw "Planetiler exited $LASTEXITCODE" }
} finally {
    Pop-Location
}

if (-not (Test-Path $OutPath)) {
    throw "Planetiler did not write $OutPath"
}
Write-Host "wrote $OutPath ($((Get-Item $OutPath).Length) bytes)"
