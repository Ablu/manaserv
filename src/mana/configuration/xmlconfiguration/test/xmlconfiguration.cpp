#include "xmlconfiguration.h"

#include <QDebug>
#include <QDir>
#include <QRegularExpression>
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

void verifySimpleConfig(XmlConfiguration &config)
{
    QVERIFY(config.getValue("test", "") == "test");
}

void XmlConfigurationTest::testSimpleConfig()
{
    XmlConfiguration config;
    QTest::ignoreMessage(QtDebugMsg, QRegularExpression("Using config file:.*"));
    QVERIFY(config.initialize(directoryPrefix + "/testdata/simple_config.xml"));
    verifySimpleConfig(config);
    config.deinitialize();
}

void XmlConfigurationTest::testDefaultValues()
{
    XmlConfiguration config;
    QTest::ignoreMessage(QtDebugMsg, QRegularExpression("Using config file:.*"));
    QVERIFY(config.initialize(directoryPrefix + "/testdata/simple_config.xml"));

    QVERIFY(config.getValue("nonexistingvalue", "default") == "default");
    QVERIFY(config.getValue("nonexistingvalue", 100) == 100);
    QVERIFY(config.getBoolValue("nonexistingvalue", false) == false);

    config.deinitialize();
}

void XmlConfigurationTest::testSimpleInclude()
{
    XmlConfiguration config;
    QTest::ignoreMessage(QtDebugMsg, QRegularExpression("Using config file:.*"));
    QVERIFY(config.initialize(directoryPrefix + "/testdata/simple_include.xml"));
    verifySimpleConfig(config);
    config.deinitialize();
}

void XmlConfigurationTest::testCircleInclude()
{
    XmlConfiguration config;
    QTest::ignoreMessage(QtDebugMsg, QRegularExpression("Using config file:.*"));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Cycle include in configuration.*"));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Error ocurred while parsing included configuration file.*"));
    QVERIFY(!config.initialize(directoryPrefix + "/testdata/circle_include.xml"));
    config.deinitialize();
}

void XmlConfigurationTest::testHiddenCircleInclude()
{
    XmlConfiguration config;
    QTest::ignoreMessage(QtDebugMsg, QRegularExpression("Using config file:.*"));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Cycle include in configuration.*"));

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Error ocurred while parsing included configuration file.*"));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Error ocurred while parsing included configuration file.*"));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Error ocurred while parsing included configuration file.*"));
    QVERIFY(!config.initialize(directoryPrefix + "/testdata/hidden_circle_include.xml"));
    config.deinitialize();
}

void XmlConfigurationTest::testValueOverride()
{
    XmlConfiguration config;
    QTest::ignoreMessage(QtDebugMsg, QRegularExpression("Using config file:.*"));
    QVERIFY(config.initialize(directoryPrefix + "/testdata/overriden_value.xml"));
    QCOMPARE(config.getValue("test", ""), QString("overriden"));
    config.deinitialize();
}

QTEST_MAIN(XmlConfigurationTest)
