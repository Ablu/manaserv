#include "tmxreader.h"

#include <QDebug>

#include "map.h"
#include "layer.h"
#include "mapobject.h"
#include "mapreader.h"
#include "objectgroup.h"
#include "tilelayer.h"

#include "mana/entities/map.h"

void populateCollisionInformation(Map &map, Tiled::Map &mapData)
{
    for (Tiled::Layer *layer : mapData.layers())
    {
        bool isCollisionLayer =
            layer->name().compare("collision", Qt::CaseInsensitive);
        if (!layer->isTileLayer() || isCollisionLayer) {
            continue;
        }

        Tiled::TileLayer *tileLayer = layer->asTileLayer();
        for (int row = 0; row < tileLayer->width(); ++row) {
            for (int column = 0; column < tileLayer->height(); ++column) {
                const Tiled::Cell &cell = tileLayer->cellAt(row, column);
                if (!cell.isEmpty()) {
                    map.blockTile(row, column, BLOCKTYPE_WALL);
                }
            }
        }
    }
}

void populateObjectInformation(Map &map, Tiled::Map &mapData)
{
    for (Tiled::ObjectGroup *group : mapData.objectGroups()) {
        for (Tiled::MapObject *object : group->objects()) {
            const QRectF &floatingPointBounds = object->bounds();
            Rectangle bounds = {(int)floatingPointBounds.x(),
                                (int)floatingPointBounds.y(),
                                (int)floatingPointBounds.width(),
                                (int)floatingPointBounds.height() };

            auto mapObject =
                new MapObject(bounds, object->name(), object->type());
            for (const QString &key : object->properties().keys()) {
                mapObject->addProperty(key, object->property(key));
            }
            map.addObject(mapObject);
        }
    }
}

void populateMapProperties(Map &map, Tiled::Map &mapData)
{
    for (const QString &property : mapData.properties().keys()) {
        map.setProperty(property, mapData.property(property));
    }
}

void populateMap(Map &map, Tiled::Map &mapData)
{
    populateCollisionInformation(map, mapData);
    populateObjectInformation(map, mapData);
    populateMapProperties(map, mapData);
}

std::unique_ptr<Map> TmxReader::readMap(const QString &path)
{
    std::unique_ptr<Map> map(new Map());
    Tiled::MapReader mapReader;
    Tiled::Map *mapData = mapReader.readMap(path);

    if (mapData == nullptr) {
        qWarning() << "Unable to read Map:" << mapReader.errorString();
        return nullptr;
    }

    map->setTileSize(mapData->tileWidth(), mapData->tileHeight());
    map->setSize(mapData->size());

    populateMap(*map, *mapData);
    return map;
}
