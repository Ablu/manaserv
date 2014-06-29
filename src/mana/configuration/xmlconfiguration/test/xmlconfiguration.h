#ifndef TST_XMLCONFIGURATION_H
#define TST_XMLCONFIGURATION_H

#include <QObject>

class XmlConfigurationTest : public QObject
{
    Q_OBJECT
public:
    XmlConfigurationTest();

private slots:
    void initTestCase();

    void testSimpleConfig();
    void testDefaultValues();
    void testSimpleInclude();
    void testCircleInclude();
    void testHiddenCircleInclude();
    void testValueOverride();
};

#endif // TST_XMLCONFIGURATION_H
