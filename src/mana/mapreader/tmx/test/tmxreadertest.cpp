#include "tmxreadertest.h"

#include <QFile>
#include <QTest>

#include "mana/mapreader/tmx/tmxreader.h"

std::unique_ptr<Map> readTestDataMap(const QString &path)
{
    QString mapFile =
        QCoreApplication::applicationDirPath() + "/TestData/" + path;
    TmxReader mapReader;
    std::unique_ptr<Map> map = mapReader.readMap(mapFile);
    return map;
}

void TmxReaderTest::collisionTest()
{
    auto map = readTestDataMap("simple_map_zlib.tmx");
    QVERIFY2(map, "Parsing the map failed!");

    char wallBlockMask = Map::BLOCKMASK_WALL;
    QVERIFY2(!map->getWalk(0, 0, wallBlockMask),
             "Tile 0, 0 should not be walkable!");
    QVERIFY2(map->getWalk(9, 9, wallBlockMask),
             "Tile 9, 9 should be walkable!");
}

void TmxReaderTest::objectTest()
{
    auto map = readTestDataMap("simple_map_zlib.tmx");
    QVERIFY2(map, "Parsing the map failed!");

    MapObject *testObject = nullptr;
    for (MapObject *object : map->getObjects()) {
        if (object->getName() == "Test") {
            if (testObject != nullptr) {
                QFAIL("Found testobject twice!");
            }

            testObject = object;
        }
    }

    QVERIFY2(testObject, "Unable to find testobject!");
    QCOMPARE(testObject->getX(), 32);
    QCOMPARE(testObject->getY(), 64);
    QCOMPARE(testObject->getBounds().w, 64);
    QCOMPARE(testObject->getBounds().h, 96);
    QCOMPARE(testObject->getType(), QStringLiteral("TestType"));
    QVERIFY2(testObject->hasProperty("ObjectProperty"),
             "Unable to find property!");
    QCOMPARE(testObject->getProperty("ObjectProperty"),
             QStringLiteral("Value1"));
}

void TmxReaderTest::propertiesTest()
{
    auto map = readTestDataMap("simple_map_zlib.tmx");
    QVERIFY2(map, "Parsing the map failed!");

    QCOMPARE(map->getProperty("TestProperty"), QStringLiteral("Value"));
}

QTEST_MAIN(TmxReaderTest)
