import qbs 1.0

import BuildHelper

CppApplication {
    name: "manaserv-game"

    Depends {
        name: "Qt"
        submodules: [
            "core",
        ]
    }

    Depends {
        name: "ManaConfigurationXmlConfiguration"
    }

    Depends {
        name: "ManaMapReaderTmx"
    }

    Depends {
        name: "ManaEntities"
    }

    Group {
        name: "Game server sources"
        prefix: "game-server/"
        files: [
            "abilitycomponent.cpp",
            "abilitycomponent.h",
            "abilitymanager.cpp",
            "abilitymanager.h",
            "accountconnection.cpp",
            "accountconnection.h",
            "actorcomponent.cpp",
            "actorcomponent.h",
            "attribute.cpp",
            "attribute.h",
            "attributeinfo.cpp",
            "attributeinfo.h",
            "attributemanager.cpp",
            "attributemanager.h",
            "being.cpp",
            "being.h",
            "buysell.cpp",
            "buysell.h",
            "charactercomponent.cpp",
            "charactercomponent.h",
            "collisiondetection.cpp",
            "collisiondetection.h",
            "commandhandler.cpp",
            "commandhandler.h",
            "component.h",
            "effect.cpp",
            "effect.h",
            "emotemanager.cpp",
            "emotemanager.h",
            "entity.cpp",
            "entity.h",
            "gamehandler.cpp",
            "gamehandler.h",
            "idmanager.h",
            "inventory.cpp",
            "inventory.h",
            "item.cpp",
            "item.h",
            "itemmanager.cpp",
            "itemmanager.h",
            "main-game.cpp",
            "mapcomposite.cpp",
            "mapcomposite.h",
            "mapmanager.cpp",
            "mapmanager.h",
            "monster.cpp",
            "monster.h",
            "monstermanager.cpp",
            "monstermanager.h",
            "npc.cpp",
            "npc.h",
            "postman.h",
            "quest.cpp",
            "quest.h",
            "settingsmanager.cpp",
            "settingsmanager.h",
            "spawnareacomponent.cpp",
            "spawnareacomponent.h",
            "state.cpp",
            "state.h",
            "statuseffect.cpp",
            "statuseffect.h",
            "statusmanager.cpp",
            "statusmanager.h",
            "timeout.cpp",
            "timeout.h",
            "trade.cpp",
            "trade.h",
            "triggerareacomponent.cpp",
            "triggerareacomponent.h",
        ]
    }

    Group {
        name: "Common sources"
        prefix: "common/"
        files: [
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
            "mathutils.cpp",
            "mathutils.h",
            "point.h",
            "processorutils.cpp",
            "processorutils.h",
            "speedconv.cpp",
            "speedconv.h",
            "string.cpp",
            "stringfilter.cpp",
            "stringfilter.h",
            "string.h",
            "throwerror.h",
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
        name: "Script sources"
        prefix: "scripting/"
        files: [
            "lua.cpp",
            "luascript.cpp",
            "luascript.h",
            "luautil.cpp",
            "luautil.h",
            "script.cpp",
            "script.h",
            "scriptmanager.cpp",
            "scriptmanager.h",
        ]
    }

    Group {
        name: "Scripting libraries"
        prefix: "../scripts/"
        files: [
            "lua/libmana.lua",
            "lua/libmana-constants.lua",
        ]
    }

    Group {
        name: "Binaries"
        fileTagsFilter: "application"
        qbs.install: true
        qbs.installDir: "bin/"
    }

    cpp.rpaths: BuildHelper.buildRpathForPrefix(project, qbs.targetOS, "..")

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
        "lua",
    ]
}
