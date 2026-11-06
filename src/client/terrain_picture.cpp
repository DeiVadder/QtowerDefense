#include "client/terrain_picture.h"

#include <QByteArray>
#include <QSize>

namespace qtd::client {

namespace {

constexpr int kPixelsPerField = 16;
constexpr int kCellPixels = 64;
constexpr int kCellLine = 2;

} // namespace

LandscapeView *TerrainPicture::view() const
{
    return view_;
}

void TerrainPicture::setView(LandscapeView *view)
{
    if (view_ == view) {
        return;
    }
    view_ = view;
    paint();
    emit viewChanged();
}

void TerrainPicture::paint()
{
    const GroundField *field = view_ ? view_->field() : nullptr;
    if (field == nullptr) {
        return;
    }
    const int width = (field->width() + 2 * kGroundMargin) * kPixelsPerField;
    const int height = (field->height() + 2 * kGroundMargin) * kPixelsPerField;
    setSize(QSize(width, height));
    setFormat(RGBA8);
    setHasTransparency(false);
    setTextureData(field->picture(kPixelsPerField));
}

GridCellPicture::GridCellPicture(QQuick3DObject *parent) : QQuick3DTextureData(parent)
{
    QByteArray pixels(static_cast<qsizetype>(kCellPixels) * kCellPixels * 4, '\0');
    char *const out = pixels.data();
    qsizetype offset = 0;
    for (int row = 0; row < kCellPixels; ++row) {
        for (int column = 0; column < kCellPixels; ++column) {
            const bool line = row < kCellLine || column < kCellLine ||
                              row >= kCellPixels - kCellLine || column >= kCellPixels - kCellLine;
            const char value = line ? static_cast<char>(0xff) : '\0';
            for (int part = 0; part < 4; ++part) {
                out[offset++] = value;
            }
        }
    }
    setSize(QSize(kCellPixels, kCellPixels));
    setFormat(RGBA8);
    setHasTransparency(true);
    setTextureData(pixels);
}

} // namespace qtd::client
