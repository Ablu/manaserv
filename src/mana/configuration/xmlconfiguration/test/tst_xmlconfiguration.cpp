#include "tst_xmlconfiguration.h"

#include <QDebug>
#include <QDir>
#include <QTest>
#include <QFileInfo>

#include "mana/configuration/xmlconfiguration/xmlconfiguration.h"

XmlConfigurationTest::XmlConfigurationTest()
{
}

QString directoryPrefix;

void XmlConfigurationTest::initTestCase()
{
    directoryPrefix = QFileInfo(QCoreApplication::applicationFilePath()).absoluteDir().absolutePath();
}

void XmlConfigurationTest::testSimpleConfig()
{
    XmlConfiguration config;
    config.initialize(directoryPrefix.toStdString() + "/testdata/simple_config.xml");
    QVERIFY(config.getValue("test", "") == "test");
    config.deinitialize();
}

QTEST_MAIN(XmlConfigurationTest)
