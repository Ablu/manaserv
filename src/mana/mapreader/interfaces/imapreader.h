#ifndef IMAPREADER_H
#define IMAPREADER_H

#include <memory>

#include <QIODevice>

#include "mana/entities/map.h"

class IMapReader
{
public:
    virtual std::unique_ptr<Map> readMap(const QString &path) = 0;
};

#endif // IMAPREADER_H
