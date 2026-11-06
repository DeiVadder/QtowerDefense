#pragma once

#include "client/game_session.h"
#include "client/map_instances.h"

#include <QByteArray>

#include <cstring>

/// Opens the table of a layer. Its real user is the renderer.
class OpenInstances : public qtd::client::MapInstances
{
public:
    OpenInstances() = default;

    OpenInstances(qtd::client::GameSession *game, Layer layer)
    {
        setLayer(layer);
        setFieldSize(50);
        setGame(game);
    }

    int count()
    {
        int instances = -1;
        getInstanceBuffer(&instances);
        return instances;
    }

    InstanceTableEntry entry(int index)
    {
        const QByteArray buffer = getInstanceBuffer(nullptr);
        InstanceTableEntry found{};
        std::memcpy(&found, buffer.constData() + index * qsizetype{sizeof(found)}, sizeof(found));
        return found;
    }

    InstanceTableEntry first()
    {
        return entry(0);
    }
};
