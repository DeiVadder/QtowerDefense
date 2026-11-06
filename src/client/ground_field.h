#pragma once

#include "client/landscape.h"
#include "core/content.h"

#include <QByteArray>
#include <QRgb>

#include <vector>

namespace qtd::client {

/// How many fields of ground lie around the grid on each side.
inline constexpr int kGroundMargin = 24;

/// A random number in [0, 1) that only depends on its arguments. The picture of a map stays the
/// same from start to start, and nothing of it reaches the simulation.
[[nodiscard]] float groundRandom(int first, int second, int third);

/// The shape and the colors of the ground, as functions of a place in fields: the middle of the
/// cell (x, y) is at (x, y). Over the grid the ground is flat, so that towers and enemies need no
/// height.
class GroundField
{
public:
    GroundField(const core::Grid &grid, const Landscape &landscape);

    [[nodiscard]] int width() const;
    [[nodiscard]] int height() const;
    /// In fields, 0 is the height of the board.
    [[nodiscard]] float heightAt(float x, float y) const;
    [[nodiscard]] QRgb colorAt(float x, float y) const;
    /// The colors of the whole ground, grid and margin, as rows of RGBA from the north down. Close
    /// to colorAt, but much faster: the slow parts come from a coarse lattice.
    [[nodiscard]] QByteArray picture(int pixelsPerField) const;
    /// How much of a path is at a place, from 0 to 1. Where a path ends on the edge of the grid,
    /// it goes on beyond it.
    [[nodiscard]] float pathAt(float x, float y) const;
    /// Whether the water of the landscape covers the place.
    [[nodiscard]] bool isUnderWater(float x, float y) const;
    /// How far a place is outside of the grid, 0 on it.
    [[nodiscard]] float distanceToGrid(float x, float y) const;

private:
    struct Tint
    {
        float red = 0;
        float green = 0;
        float blue = 0;
    };

    /// Where a path leaves the grid: it goes straight on from the cell, in the direction.
    struct Exit
    {
        int x = 0;
        int y = 0;
        int stepX = 0;
        int stepY = 0;
    };

    /// What decides the color of a place, without the fine grain.
    struct Surface
    {
        float patches = 0;
        float path = 0;
        float wet = 0;
        float rim = 0; ///< how far the ground has gone over into the plain beyond it
    };

    [[nodiscard]] static Tint tintOf(const QColor &color);
    [[nodiscard]] static Tint mixed(Tint from, Tint to, float share);
    [[nodiscard]] float maskAt(const std::vector<float> &mask, float x, float y) const;
    [[nodiscard]] float pondAt(float x, float y) const;
    [[nodiscard]] float rimAt(float x, float y) const;
    void findExits();
    [[nodiscard]] float exitAt(float x, float y) const;
    [[nodiscard]] float heightWith(float path, float x, float y) const;
    [[nodiscard]] Surface surfaceAt(float x, float y) const;
    [[nodiscard]] QRgb shade(const Surface &surface, float grain) const;
    [[nodiscard]] std::vector<Surface> lattice(int columns, int rows) const;
    [[nodiscard]] static Surface
    blended(const std::vector<Surface> &surfaces, int columns, float x, float y);

    int width_ = 0;
    int height_ = 0;
    bool hasWater_ = false;
    bool lakes_ = true;
    float waterLevel_ = 0;
    float hillHeight_ = 0;
    Tint grass_;
    Tint grassDark_;
    Tint earth_;
    Tint path_;
    Tint pathDark_;
    Tint mud_;
    std::vector<float> paths_;
    std::vector<Exit> exits_;
    std::vector<float> ponds_;
};

} // namespace qtd::client
