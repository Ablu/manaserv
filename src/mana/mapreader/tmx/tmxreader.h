#ifndef TMXREADER_H
#define TMXREADER_H

#include "mana/mapreader/interfaces/imapreader.h"

class TmxReader : public IMapReader
{
public:
    std::unique_ptr<Map> readMap(const QString &path);
};

#endif // TMXREADER_H
