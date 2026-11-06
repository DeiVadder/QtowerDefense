#include "client/terrain_mesh.h"

#include <QByteArray>
#include <QVector3D>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <vector>

namespace qtd::client {

namespace {

constexpr int kStepsPerField = 2;
constexpr float kHighestGround = 4; // fields, for the bounds of the mesh

struct Vertex
{
    QVector3D position;
    QVector3D normal;
    float u = 0;
    float v = 0;
};

std::vector<Vertex> verticesOf(const GroundField &field, int columns, int rows, float fieldSize)
{
    const float start = -static_cast<float>(kGroundMargin) - 0.5F;
    std::vector<float> heights;
    heights.reserve(static_cast<std::size_t>(columns) * static_cast<std::size_t>(rows));
    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < columns; ++column) {
            heights.push_back(field.heightAt(start + static_cast<float>(column) / kStepsPerField,
                                             start + static_cast<float>(row) / kStepsPerField));
        }
    }
    const auto heightAt = [&](int column, int row) {
        return heights[static_cast<std::size_t>(std::clamp(row, 0, rows - 1)) *
                           static_cast<std::size_t>(columns) +
                       static_cast<std::size_t>(std::clamp(column, 0, columns - 1))];
    };
    std::vector<Vertex> vertices;
    vertices.reserve(heights.size());
    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < columns; ++column) {
            const float x = start + static_cast<float>(column) / kStepsPerField;
            const float y = start + static_cast<float>(row) / kStepsPerField;
            const float across = heightAt(column + 1, row) - heightAt(column - 1, row);
            const float down = heightAt(column, row + 1) - heightAt(column, row - 1);
            vertices.push_back({QVector3D(x, heightAt(column, row), y) * fieldSize,
                                QVector3D(-across, 2.0F / kStepsPerField, -down).normalized(),
                                static_cast<float>(column) / static_cast<float>(columns - 1),
                                1 - static_cast<float>(row) / static_cast<float>(rows - 1)});
        }
    }
    return vertices;
}

QByteArray bytesOf(const auto &items)
{
    const std::size_t size = items.size() * sizeof(items[0]);
    QByteArray bytes(static_cast<qsizetype>(size), Qt::Uninitialized);
    std::memcpy(bytes.data(), items.data(), size);
    return bytes;
}

std::vector<quint32> indicesOf(int columns, int rows)
{
    std::vector<quint32> indices;
    indices.reserve(static_cast<std::size_t>(columns - 1) * static_cast<std::size_t>(rows - 1) * 6);
    for (int row = 0; row + 1 < rows; ++row) {
        for (int column = 0; column + 1 < columns; ++column) {
            const quint32 first = static_cast<quint32>(row * columns + column);
            const quint32 below = first + static_cast<quint32>(columns);
            indices.insert(indices.end(), {first, below, first + 1, first + 1, below, below + 1});
        }
    }
    return indices;
}

} // namespace

LandscapeView *TerrainMesh::view() const
{
    return view_;
}

void TerrainMesh::setView(LandscapeView *view)
{
    if (view_ == view) {
        return;
    }
    view_ = view;
    rebuild();
    emit viewChanged();
}

float TerrainMesh::fieldSize() const
{
    return fieldSize_;
}

void TerrainMesh::setFieldSize(float size)
{
    if (!qFuzzyCompare(fieldSize_, size)) {
        fieldSize_ = size;
        rebuild();
        emit fieldSizeChanged();
    }
}

void TerrainMesh::rebuild()
{
    clear();
    const GroundField *field = view_ ? view_->field() : nullptr;
    if (field == nullptr) {
        update();
        return;
    }
    const int fieldsAcross = field->width() + 2 * kGroundMargin;
    const int fieldsDown = field->height() + 2 * kGroundMargin;
    const int columns = fieldsAcross * kStepsPerField + 1;
    const int rows = fieldsDown * kStepsPerField + 1;
    const std::vector<Vertex> vertices = verticesOf(*field, columns, rows, fieldSize_);
    const std::vector<quint32> indices = indicesOf(columns, rows);

    setStride(sizeof(Vertex));
    setPrimitiveType(PrimitiveType::Triangles);
    addAttribute(Attribute::PositionSemantic, 0, Attribute::F32Type);
    addAttribute(Attribute::NormalSemantic, sizeof(QVector3D), Attribute::F32Type);
    addAttribute(Attribute::TexCoord0Semantic, 2 * sizeof(QVector3D), Attribute::F32Type);
    addAttribute(Attribute::IndexSemantic, 0, Attribute::U32Type);
    setVertexData(bytesOf(vertices));
    setIndexData(bytesOf(indices));
    const float start = -static_cast<float>(kGroundMargin) - 0.5F;
    setBounds(QVector3D(start, -kHighestGround, start) * fieldSize_,
              QVector3D(start + static_cast<float>(fieldsAcross),
                        kHighestGround,
                        start + static_cast<float>(fieldsDown)) *
                  fieldSize_);
    update();
}

} // namespace qtd::client
