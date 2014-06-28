import qbs 1.0

CppApplication {
    name: "manaserv-account"

    Depends {
        name: "Qt"
        submodules: [
            "sql",
        ]
    }

    Depends {
        name: "ManaEntities"
    }

    Group {
        name: "Account server sources"
        prefix: "account-server/"
        files: [
            "accountclient.cpp",
            "accountclient.h",
            "accounthandler.cpp",
            "accounthandler.h",
            "characterdatautils.cpp",
            "characterdatautils.h",
            "flooritem.h",
            "main-account.cpp",
            "mapmanager.cpp",
            "mapmanager.h",
            "serverhandler.cpp",
            "serverhandler.h",
            "storage.cpp",
            "storage.h",
        ]
    }

    Group {
        name: "Common sources"
        prefix: "common/"
        files: [
            "configuration.cpp",
            "configuration.h",
            "defines.h",
            "inventorydata.h",
            "manaserv_protocol.h",
            "permissionmanager.cpp",
            "permissionmanager.h",
            "resourcemanager.cpp",
            "resourcemanager.h",
            "transaction.h",

        ]
    }

    Group {
        name: "Utils sources"
        prefix: "utils/"
        files: [
            "base64.cpp",
            "base64.h",
            "functors.h",
            "logger.cpp",
            "logger.h",
            "mathutils.cpp",
            "mathutils.h",
            "point.h",
            "processorutils.cpp",
            "processorutils.h",
            "sha256.cpp",
            "sha256.h",
            "speedconv.cpp",
            "speedconv.h",
            "string.cpp",
            "stringfilter.cpp",
            "stringfilter.h",
            "string.h",
            "throwerror.h",
            "time.h",
            "timer.cpp",
            "timer.h",
            "tokencollector.cpp",
            "tokencollector.h",
            "tokendispenser.cpp",
            "tokendispenser.h",
            "xml.cpp",
            "xml.h",
            "zlib.cpp",
            "zlib.h",
        ]
    }

    Group {
        name: "Net sources"
        prefix: "net/"
        files: [
            "bandwidth.cpp",
            "bandwidth.h",
            "connection.cpp",
            "connection.h",
            "connectionhandler.cpp",
            "connectionhandler.h",
            "messagein.cpp",
            "messagein.h",
            "messageout.cpp",
            "messageout.h",
            "netcomputer.cpp",
            "netcomputer.h",
        ]
    }

    Group {
        name: "Chat sources"
        prefix: "chat-server/"
        files: [
            "chatchannel.cpp",
            "chatchannel.h",
            "chatchannelmanager.cpp",
            "chatchannelmanager.h",
            "chatclient.h",
            "chathandler.cpp",
            "chathandler.h",
            "guildhandler.cpp",
            "guildmanager.cpp",
            "guildmanager.h",
            "party.cpp",
            "party.h",
            "partyhandler.cpp",
            "postmanager.cpp",
            "postmanager.h",
        ]
    }

    cpp.includePaths: [
        ".",
        "/usr/include/libxml2/",
        "/usr/include/",
    ]
    cpp.cxxFlags: [
        "-std=c++0x",
    ]
    cpp.dynamicLibraries: [
        "xml2",
        "enet",
        "physfs",
        "z",
    ]
}
