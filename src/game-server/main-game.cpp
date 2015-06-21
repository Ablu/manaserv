/*
 *  The Mana Server
 *  Copyright (C) 2004-2010  The Mana World Development Team
 *  Copyright (C) 2010-2012  The Mana Developers
 *
 *  This file is part of The Mana Server.
 *
 *  The Mana Server is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  any later version.
 *
 *  The Mana Server is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with The Mana Server.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "mana/configuration/interfaces/iconfiguration.h"

#include "mana/configuration/xmlconfiguration/xmlconfiguration.h"

#include "mana/mapreader/tmx/tmxreader.h"

#include "common/defines.h"
#include "common/permissionmanager.h"

#include "game-server/accountconnection.h"
#include "game-server/attributemanager.h"
#include "game-server/commandhandler.h"
#include "game-server/gamehandler.h"
#include "game-server/emotemanager.h"
#include "game-server/item.h"
#include "game-server/itemmanager.h"
#include "game-server/mapmanager.h"
#include "game-server/monstermanager.h"
#include "game-server/abilitymanager.h"
#include "game-server/statusmanager.h"
#include "game-server/postman.h"
#include "game-server/state.h"
#include "game-server/settingsmanager.h"

#include "net/bandwidth.h"
#include "net/connectionhandler.h"
#include "net/messageout.h"
#include "scripting/scriptmanager.h"
#include "utils/processorutils.h"
#include "utils/stringfilter.h"
#include "utils/timer.h"
#include "utils/mathutils.h"

#include <cstdlib>
#include <getopt.h>
#include <iostream>
#include <signal.h>
#include <physfs.h>
#include <enet/enet.h>
#include <unistd.h>

#include <QGuiApplication>

#ifdef __MINGW32__
#include <windows.h>
#include "common/winver.h"
#define usleep(usec) (Sleep ((usec) / 1000), 0)
#endif

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#define DEFAULT_LOG_FILE                    "manaserv-game.log"
#define DEFAULT_MAIN_SCRIPT_FILE            "scripts/main.lua"

static int const WORLD_TICK_SKIP = 2; /** tolerance for lagging behind in world calculation) **/

/** Timer for world ticks */
static utils::Timer worldTimer(WORLD_TICK_MS);
static int currentTick = 0;     /**< Current world time in ticks */
static bool running = true;     /**< Whether the server keeps running */

utils::StringFilter *stringFilter; /**< Slang's Filter */

AbilityManager *abilityManager = new AbilityManager();
AttributeManager *attributeManager = new AttributeManager();
ItemManager *itemManager = new ItemManager();
MonsterManager *monsterManager = new MonsterManager();
EmoteManager *emoteManager = new EmoteManager();

SettingsManager *settingsManager;

/** Core game message handler */
GameHandler *gameHandler;

/** Account server message handler */
AccountConnection *accountHandler;

/** Post Man **/
PostMan *postMan;

/** Bandwidth Monitor */
BandwidthMonitor *gBandwidth;

/** Callback used when SIGQUIT signal is received. */
static void closeGracefully(int)
{
    running = false;
}

XmlConfiguration *configuration;
TmxReader *mapReader;

static void initializeServer()
{
    // Used to close via process signals
#if (defined __USE_UNIX98 || defined __FreeBSD__)
    signal(SIGQUIT, closeGracefully);
#endif
    signal(SIGINT, closeGracefully);
    signal(SIGTERM, closeGracefully);


    QString logFile = configuration->getValue("log_gameServerFile",
                                                 DEFAULT_LOG_FILE);

    Item::initialize(configuration);
    CommandHandler::initalize(configuration);
    GameState::initialize(configuration);

    // Initialize PhysicsFS
    PHYSFS_init("");

    // --- Initialize the managers
    // Initialize the slang's and double quotes filter.
    stringFilter = new utils::StringFilter(configuration);

    ScriptManager::initialize(configuration);   // Depends on ResourceManager

    // load game settings files
    const QString worldPath = configuration->getValue("worldDataPath", "example");
    settingsManager= new SettingsManager(worldPath + "/" + DEFAULT_SETTINGS_FILE);
    settingsManager->initialize(configuration, mapReader);

    PermissionManager::initialize(DEFAULT_PERMISSION_FILE);


    QString mainScript = configuration->getValue("script_mainFile",
                                                     DEFAULT_MAIN_SCRIPT_FILE);
    ScriptManager::loadMainScript(mainScript);

    // --- Initialize the global handlers
    // FIXME: Make the global handlers global vars or part of a bigger
    // singleton or a local variable in the event-loop
    gameHandler = new GameHandler(configuration);
    accountHandler = new AccountConnection(configuration);
    postMan = new PostMan;
    gBandwidth = new BandwidthMonitor;

    // --- Initialize enet.
    if (enet_initialize() != 0)
    {
        qCritical() << "An error occurred while initializing ENet";
        exit(EXIT_NET_EXCEPTION);
    }

    // Pre-calculate the needed trigomic function values
    utils::math::init();

    // Initialize the processor utility functions
    utils::processor::init();

    // Seed the random number generator
    std::srand( time(nullptr) );
}


static void deinitializeServer()
{
    // Write configuration file
    configuration->deinitialize();

    // Stop world timer
    worldTimer.stop();

    // Quit ENet
    enet_deinitialize();

    // Destroy message handlers
    delete gameHandler;
    gameHandler = nullptr;
    delete accountHandler;
    accountHandler = nullptr;
    delete postMan;
    postMan = nullptr;
    delete gBandwidth;
    gBandwidth = nullptr;

    // Destroy Managers
    delete stringFilter;
    stringFilter = nullptr;
    delete monsterManager;
    monsterManager = nullptr;
    delete abilityManager;
    abilityManager = nullptr;
    delete itemManager;
    itemManager = nullptr;
    delete emoteManager;
    emoteManager = nullptr;
    delete settingsManager;
    settingsManager = nullptr;
    MapManager::deinitialize();
    StatusManager::deinitialize();
    ScriptManager::deinitialize();

    PHYSFS_deinit();
}


/**
 * Show command line arguments.
 */
static void printHelp()
{
    std::cout << "manaserv" << std::endl << std::endl
              << "Options: " << std::endl
              << "  -h --help          : Display this help" << std::endl
              << "     --config <path> : Set the config path to use."
              << " (Default: ./manaserv.xml)" << std::endl
              << "  -v --verbosity <n> : Set the verbosity level" << std::endl
              << "                        - 0. Fatal Errors only." << std::endl
              << "                        - 1. All Errors." << std::endl
              << "                        - 2. Plus warnings." << std::endl
              << "                        - 3. Plus standard information." << std::endl
              << "                        - 4. Plus debugging information." << std::endl
              << "     --port <n>      : Set the default port to listen on."
              << std::endl;
    exit(EXIT_NORMAL);
}

struct CommandLineOptions
{
    CommandLineOptions():
        verbosityChanged(false),
        port(DEFAULT_SERVER_PORT + 3),
        portChanged(false)
    {}

    QString configPath;

    bool verbosityChanged;

    int port;
    bool portChanged;
};

/**
 * Parse the command line arguments
 */
static void parseOptions(int argc, char *argv[], CommandLineOptions &options)
{
    const char *optString = "hv:";

    const struct option longOptions[] = {
        { "help", no_argument, nullptr, 'h' },
        { "config", required_argument, nullptr, 'c' },
        { "port", required_argument, nullptr, 'p' },
        { nullptr, 0, nullptr, 0 }
    };

    while (optind < argc)
    {
        int result = getopt_long(argc, argv, optString, longOptions, nullptr);

        if (result == -1)
            break;

        switch (result)
        {
            default: // Unknown option.
            case 'h':
                // Print help.
                printHelp();
                break;
            case 'c':
                // Change config filename and path.
                options.configPath = optarg;
                break;
            case 'p':
                options.port = atoi(optarg);
                options.portChanged = true;
                break;
        }
    }
}


/**
 * Main function, initializes and runs server.
 */
int main(int argc, char *argv[])
{
    QGuiApplication application(argc, argv);

    // Parse command line options
    CommandLineOptions options;
    parseOptions(argc, argv, options);

    configuration = new XmlConfiguration();
    mapReader = new TmxReader();

    if (!configuration->initialize(options.configPath))
    {
        qCritical() << "Refusing to run without configuration!";
        exit(EXIT_CONFIG_NOT_FOUND);
    }

    // Check inter-server password.
    if (configuration->getValue("net_password", QString()).isEmpty())
    {
        qCritical() << "SECURITY WARNING: 'net_password' not set!";
        exit(EXIT_BAD_CONFIG_PARAMETER);
    }

    // General initialization
    initializeServer();

#ifdef PACKAGE_VERSION
    qDebug() << "The Mana Game Server v" << PACKAGE_VERSION;
#else
    qDebug() << "The Mana Game Server (unknown version)";
#endif
    qDebug() << "Manaserv Protocol version " << ManaServ::PROTOCOL_VERSION
             << ", " << "Enet version " << ENET_VERSION_MAJOR << "."
             << ENET_VERSION_MINOR << "." << ENET_VERSION_PATCH;

    // When the gameListenToClientPort is set, we use it.
    // Otherwise, we use the accountListenToClientPort + 3 if the option is set.
    // If neither, the DEFAULT_SERVER_PORT + 3 is used.
    if (!options.portChanged)
    {
        // Prepare the fallback value
        options.port = configuration->getValue("net_accountListenToClientPort",
                                               0) + 3;
        if (options.port == 3)
            options.port = DEFAULT_SERVER_PORT + 3;

        // Set the actual value of options.port
        options.port = configuration->getValue("net_gameListenToClientPort",
                                               options.port);
    }

    bool debugNetwork = configuration->getBoolValue("net_debugMode", false);
    MessageOut::setDebugModeEnabled(debugNetwork);

    // Make an initial attempt to connect to the account server
    // Try again after longer and longer intervals when connection fails.
    bool isConnected = false;
    int waittime = 0;
    while (!isConnected && running)
    {
        qDebug() << "Connecting to account server";
        isConnected = accountHandler->start(options.port);
        if (!isConnected)
        {
            qDebug() << "Retrying in " << ++waittime << " seconds";
            usleep(waittime * 1000);
        }
    }

    if (!gameHandler->startListen(options.port))
    {
        qCritical() << "Unable to create an ENet server host.";
        return EXIT_NET_EXCEPTION;
    }

    // Initialize world timer
    worldTimer.start();

    // Account connection lost flag
    bool accountServerLost = false;

    while (running)
    {
        int elapsedTicks = worldTimer.poll();

        if (elapsedTicks == 0)
        {
            worldTimer.sleep();
            continue;
        }

        if (elapsedTicks > WORLD_TICK_SKIP)
        {
            qWarning() << "Skipping "<< elapsedTicks - 1 << " ticks.";
            elapsedTicks = 1;
        }

        while (elapsedTicks > 0)
        {
            currentTick++;
            elapsedTicks--;

            // Print world time at 10 second intervals to show we're alive
            if (currentTick % 100 == 0)
                qDebug() << "World time: " << currentTick;

            if (accountHandler->isConnected())
            {
                accountServerLost = false;

                // Handle all messages that are in the message queues
                accountHandler->process();

                if (currentTick % 100 == 0) {
                    accountHandler->syncChanges(true);
                    // force sending changes to the account server every 10 secs.
                }

                if (currentTick % 300 == 0)
                {
                    accountHandler->sendStatistics();
                    qDebug() << "Total Account Output: " << gBandwidth->totalInterServerOut() << " Bytes";
                    qDebug() << "Total Account Input: " << gBandwidth->totalInterServerIn() << " Bytes";
                    qDebug() << "Total Client Output: " << gBandwidth->totalClientOut() << " Bytes";
                    qDebug() << "Total Client Input: " << gBandwidth->totalClientIn() << " Bytes";
                }
            }
            else
            {
                // If the connection to the account server is lost.
                // Every players have to be logged out
                if (!accountServerLost)
                {
                    qWarning() << "The connection to the account server was lost.";
                    accountServerLost = true;
                }

                // Try to reconnect every 200 ticks
                if (currentTick % 200 == 0)
                {
                    accountHandler->start(options.port);
                }
            }
            gameHandler->process();
            // Update all active objects/beings
            GameState::update(currentTick);
            // Send potentially urgent outgoing messages
            gameHandler->flush();
        }
    }

    qDebug() << "Received: Quit signal, closing down...";
    gameHandler->stopListen();
    accountHandler->stop();
    deinitializeServer();

    return EXIT_NORMAL;
}
