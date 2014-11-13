#ifndef TMXREADERTEST_H
#define TMXREADERTEST_H

#include <QObject>

class TmxReaderTest : public QObject
{
    Q_OBJECT

private slots:
    void collisionTest();
    void objectTest();
    void propertiesTest();
};

#endif // TMXREADERTEST_H
