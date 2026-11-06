#include "client/ground_field.h"

#include <QColor>

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace qtd::client {

namespace {

constexpr int kHillSeed = 11;
constexpr int kPatchSeed = 23;
constexpr int kGrainSeed = 37;
constexpr int kWobbleSeed = 51;
constexpr int kLatticePerField = 4;    // where picture() works out the slow parts
constexpr int kGrainTile = 128;        // pixels, the fine grain repeats
constexpr float kRimWidth = 5;         // fields, the ground goes over into the plain beyond it
constexpr float kFlatBorder = 1.5F;    // fields around the grid that stay flat
constexpr float kHillStart = 9;        // fields from the grid to the full height of the hills
constexpr float kPondDepth = 0.3F;     // of a field, a water cell below the ground
constexpr float kWobble = 0.32F;       // of a field, the edges of paths are not straight
constexpr float kShoreHeight = 0.015F; // of a field, it stays below the board

float smooth(float from, float to, float value)
{
    const float share = std::clamp((value - from) / (to - from), 0.0F, 1.0F);
    return share * share * (3 - 2 * share);
}

float mix(float from, float to, float share)
{
    return from + (to - from) * share;
}

float noise(float x, float y, int seed)
{
    const int left = static_cast<int>(std::floor(x));
    const int top = static_cast<int>(std::floor(y));
    const float across = smooth(0, 1, x - static_cast<float>(left));
    const float down = smooth(0, 1, y - static_cast<float>(top));
    return mix(
        mix(groundRandom(left, top, seed), groundRandom(left + 1, top, seed), across),
        mix(groundRandom(left, top + 1, seed), groundRandom(left + 1, top + 1, seed), across),
        down);
}

/// Noise in three sizes, the small ones count less.
float layers(float x, float y, int seed)
{
    return (4 * noise(x, y, seed) + 2 * noise(2 * x, 2 * y, seed + 1) +
            noise(4 * x, 4 * y, seed + 2)) /
           7;
}

float grainAt(float x, float y)
{
    return noise(x * 7, y * 7, kGrainSeed) * 0.6F + noise(x * 15, y * 15, kGrainSeed + 1) * 0.4F;
}

std::vector<float> grainTile()
{
    std::vector<float> grains;
    grains.reserve(std::size_t{kGrainTile} * kGrainTile);
    for (int row = 0; row < kGrainTile; ++row) {
        for (int column = 0; column < kGrainTile; ++column) {
            grains.push_back(grainAt(static_cast<float>(column) / kGrainTile * 8,
                                     static_cast<float>(row) / kGrainTile * 8));
        }
    }
    return grains;
}

int channel(float value)
{
    return std::clamp(static_cast<int>(value * 255), 0, 255);
}

} // namespace

float groundRandom(int first, int second, int third)
{
    quint32 value = static_cast<quint32>(first) * 0x9E3779B1U;
    value ^= static_cast<quint32>(second) * 0x85EBCA77U;
    value ^= static_cast<quint32>(third) * 0xC2B2AE3DU;
    value ^= value >> 15U;
    value *= 0x2C1B3C6DU;
    value ^= value >> 12U;
    value *= 0x297A2D39U;
    value ^= value >> 15U;
    return static_cast<float>(value >> 8U) / static_cast<float>(1U << 24U);
}

GroundField::GroundField(const core::Grid &grid, const Landscape &landscape)
    : width_(static_cast<int>(grid.width())), height_(static_cast<int>(grid.height())),
      hasWater_(landscape.water.isValid()), lakes_(landscape.lakes),
      waterLevel_(landscape.waterLevel), hillHeight_(landscape.hillHeight),
      grass_(tintOf(landscape.grass)), grassDark_(tintOf(landscape.grassDark)),
      earth_(tintOf(landscape.earth)), path_(tintOf(landscape.path)),
      pathDark_(tintOf(landscape.pathDark)), mud_(tintOf(landscape.pathDark.darker(170)))
{
    const std::size_t cells = static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_);
    paths_.reserve(cells);
    ponds_.reserve(cells);
    for (qint64 y = 0; y < grid.height(); ++y) {
        for (qint64 x = 0; x < grid.width(); ++x) {
            const core::Terrain terrain = grid.terrainAt({x, y});
            paths_.push_back(terrain == core::Terrain::Path ? 1.0F : 0.0F);
            ponds_.push_back(terrain == core::Terrain::Water ? 1.0F : 0.0F);
        }
    }
    findExits();
}

void GroundField::findExits()
{
    const auto isPath = [this](int x, int y) {
        const bool inside = x >= 0 && y >= 0 && x < width_ && y < height_;
        return inside && paths_[static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) +
                                static_cast<std::size_t>(x)] > 0;
    };
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            const int across = int{isPath(x - 1, y)} + int{isPath(x + 1, y)};
            const int down = int{isPath(x, y - 1)} + int{isPath(x, y + 1)};
            if (!isPath(x, y) || across + down != 1) {
                continue;
            }
            // The end of a path goes on the way it came, if that leads off the grid.
            const int stepX = int{isPath(x - 1, y)} - int{isPath(x + 1, y)};
            const int stepY = int{isPath(x, y - 1)} - int{isPath(x, y + 1)};
            const int nextX = x + stepX;
            const int nextY = y + stepY;
            if (nextX < 0 || nextY < 0 || nextX >= width_ || nextY >= height_) {
                exits_.push_back({x, y, stepX, stepY});
            }
        }
    }
}

float GroundField::exitAt(float x, float y) const
{
    float found = 0;
    for (const Exit &exit : exits_) {
        const float offsetX = x - static_cast<float>(exit.x);
        const float offsetY = y - static_cast<float>(exit.y);
        const float ahead =
            offsetX * static_cast<float>(exit.stepX) + offsetY * static_cast<float>(exit.stepY);
        const float aside = std::abs(offsetX * static_cast<float>(exit.stepY)) +
                            std::abs(offsetY * static_cast<float>(exit.stepX));
        if (ahead > 0) {
            found = std::max(found, 1 - aside);
        }
    }
    return found;
}

GroundField::Tint GroundField::tintOf(const QColor &color)
{
    return {color.redF(), color.greenF(), color.blueF()};
}

GroundField::Tint GroundField::mixed(Tint from, Tint to, float share)
{
    return {mix(from.red, to.red, share),
            mix(from.green, to.green, share),
            mix(from.blue, to.blue, share)};
}

int GroundField::width() const
{
    return width_;
}

int GroundField::height() const
{
    return height_;
}

float GroundField::maskAt(const std::vector<float> &mask, float x, float y) const
{
    const float clampedX = std::clamp(x, 0.0F, static_cast<float>(width_ - 1));
    const float clampedY = std::clamp(y, 0.0F, static_cast<float>(height_ - 1));
    const int left = static_cast<int>(clampedX);
    const int top = static_cast<int>(clampedY);
    const int right = std::min(left + 1, width_ - 1);
    const int bottom = std::min(top + 1, height_ - 1);
    const auto at = [&](int column, int row) {
        return mask[static_cast<std::size_t>(row) * static_cast<std::size_t>(width_) +
                    static_cast<std::size_t>(column)];
    };
    const float across = clampedX - static_cast<float>(left);
    const float down = clampedY - static_cast<float>(top);
    return mix(mix(at(left, top), at(right, top), across),
               mix(at(left, bottom), at(right, bottom), across),
               down);
}

float GroundField::pathAt(float x, float y) const
{
    const float wobbledX = x + 2 * kWobble * (noise(x * 1.3F, y * 1.3F, kWobbleSeed) - 0.5F);
    const float wobbledY = y + 2 * kWobble * (noise(x * 1.3F, y * 1.3F, kWobbleSeed + 1) - 0.5F);
    // A path along the edge keeps its width, but only an exit goes on.
    const float beyond = smooth(0.2F, 0.6F, distanceToGrid(wobbledX, wobbledY));
    const float path = maskAt(paths_, wobbledX, wobbledY) * (1 - beyond);
    return smooth(0.3F, 0.62F, std::max(path, exitAt(wobbledX, wobbledY)));
}

float GroundField::pondAt(float x, float y) const
{
    return distanceToGrid(x, y) > 0 ? 0 : smooth(0.35F, 0.7F, maskAt(ponds_, x, y));
}

float GroundField::distanceToGrid(float x, float y) const
{
    const float outX = std::max({-x, x - static_cast<float>(width_ - 1), 0.0F});
    const float outY = std::max({-y, y - static_cast<float>(height_ - 1), 0.0F});
    return std::hypot(outX, outY);
}

float GroundField::rimAt(float x, float y) const
{
    const float outX = std::max({-x, x - static_cast<float>(width_ - 1), 0.0F});
    const float outY = std::max({-y, y - static_cast<float>(height_ - 1), 0.0F});
    const float margin = static_cast<float>(kGroundMargin);
    return smooth(margin - kRimWidth, margin - 0.5F, std::max(outX, outY));
}

float GroundField::heightWith(float path, float x, float y) const
{
    const float rise = smooth(kFlatBorder, kHillStart, distanceToGrid(x, y)) * (1 - rimAt(x, y));
    const float noiseHeight =
        rise == 0 ? 0 : hillHeight_ * (layers(x * 0.11F, y * 0.11F, kHillSeed) - 0.42F);
    const float hills = lakes_ ? noiseHeight : std::max(noiseHeight, 0.0F);
    return rise * hills * (1 - path) - kPondDepth * pondAt(x, y);
}

float GroundField::heightAt(float x, float y) const
{
    return heightWith(pathAt(x, y), x, y);
}

bool GroundField::isUnderWater(float x, float y) const
{
    return hasWater_ && heightAt(x, y) < waterLevel_;
}

GroundField::Surface GroundField::surfaceAt(float x, float y) const
{
    const float path = pathAt(x, y);
    const float wet =
        hasWater_ ? 1 - smooth(waterLevel_, waterLevel_ + kShoreHeight, heightWith(path, x, y))
                  : pondAt(x, y);
    return {layers(x * 0.45F, y * 0.45F, kPatchSeed), path, wet, rimAt(x, y)};
}

QRgb GroundField::shade(const Surface &surface, float grain) const
{
    Tint color = mixed(grassDark_, grass_, smooth(0.3F, 0.7F, surface.patches));
    color = mixed(color, earth_, 0.7F * smooth(0.62F, 0.75F, 1 - surface.patches));
    color = mixed(color, earth_, 0.8F * smooth(0, 0.5F, surface.path));
    color = mixed(color, mixed(pathDark_, path_, grain), smooth(0.5F, 0.9F, surface.path));
    color = mixed(color, mud_, 0.85F * surface.wet);
    color = mixed(color, grassDark_, surface.rim);

    const float light = 1 + (1 - surface.rim) * (0.2F * grain - 0.1F);
    return qRgb(
        channel(color.red * light), channel(color.green * light), channel(color.blue * light));
}

QRgb GroundField::colorAt(float x, float y) const
{
    return shade(surfaceAt(x, y), grainAt(x, y));
}

std::vector<GroundField::Surface> GroundField::lattice(int columns, int rows) const
{
    const float start = -static_cast<float>(kGroundMargin) - 0.5F;
    std::vector<Surface> surfaces;
    surfaces.reserve(static_cast<std::size_t>(columns) * static_cast<std::size_t>(rows));
    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < columns; ++column) {
            surfaces.push_back(surfaceAt(start + static_cast<float>(column) / kLatticePerField,
                                         start + static_cast<float>(row) / kLatticePerField));
        }
    }
    return surfaces;
}

QByteArray GroundField::picture(int pixelsPerField) const
{
    const int fieldsAcross = width_ + 2 * kGroundMargin;
    const int fieldsDown = height_ + 2 * kGroundMargin;
    const int columns = fieldsAcross * kLatticePerField + 1;
    const std::vector<Surface> surfaces = lattice(columns, fieldsDown * kLatticePerField + 1);
    const std::vector<float> grains = grainTile();

    const int pixelsAcross = fieldsAcross * pixelsPerField;
    const int pixelsDown = fieldsDown * pixelsPerField;
    const float latticePerPixel =
        static_cast<float>(kLatticePerField) / static_cast<float>(pixelsPerField);
    QByteArray pixels(static_cast<qsizetype>(pixelsAcross) * pixelsDown * 4, Qt::Uninitialized);
    char *const out = pixels.data();
    qsizetype offset = 0;
    for (int row = 0; row < pixelsDown; ++row) {
        // The first row of a texture is the bottom of the mesh, see the v of TerrainMesh.
        const float latticeY = (static_cast<float>(pixelsDown - 1 - row) + 0.5F) * latticePerPixel;
        for (int column = 0; column < pixelsAcross; ++column) {
            const float latticeX = (static_cast<float>(column) + 0.5F) * latticePerPixel;
            const float grain = grains[static_cast<std::size_t>(row % kGrainTile) * kGrainTile +
                                       static_cast<std::size_t>(column % kGrainTile)];
            const QRgb color = shade(blended(surfaces, columns, latticeX, latticeY), grain);
            out[offset++] = static_cast<char>(qRed(color));
            out[offset++] = static_cast<char>(qGreen(color));
            out[offset++] = static_cast<char>(qBlue(color));
            out[offset++] = static_cast<char>(0xff);
        }
    }
    return pixels;
}

GroundField::Surface
GroundField::blended(const std::vector<Surface> &surfaces, int columns, float x, float y)
{
    const int left = static_cast<int>(x);
    const int top = static_cast<int>(y);
    const float across = x - static_cast<float>(left);
    const float down = y - static_cast<float>(top);
    const std::size_t first = static_cast<std::size_t>(top) * static_cast<std::size_t>(columns) +
                              static_cast<std::size_t>(left);
    const std::size_t below = first + static_cast<std::size_t>(columns);
    const Surface &northWest = surfaces[first];
    const Surface &northEast = surfaces[first + 1];
    const Surface &southWest = surfaces[below];
    const Surface &southEast = surfaces[below + 1];
    const auto blend = [&](float one, float two, float three, float four) {
        return mix(mix(one, two, across), mix(three, four, across), down);
    };
    return {blend(northWest.patches, northEast.patches, southWest.patches, southEast.patches),
            blend(northWest.path, northEast.path, southWest.path, southEast.path),
            blend(northWest.wet, northEast.wet, southWest.wet, southEast.wet),
            blend(northWest.rim, northEast.rim, southWest.rim, southEast.rim)};
}

} // namespace qtd::client
