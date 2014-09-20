/*
 *  The Mana Server
 *  Copyright (C) 2004-2010  The Mana World Development Team
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

#include "account-server/accounthandler.h"

#include "mana/configuration/interfaces/iconfiguration.h"

#include "mana/persistence/interfaces/istorage.h"

#include "mana/entities/account.h"
#include "mana/entities/character.h"

#include "account-server/accountclient.h"
#include "account-server/serverhandler.h"
#include "chat-server/chathandler.h"
#include "common/manaserv_protocol.h"
#include "common/transaction.h"
#include "net/connectionhandler.h"
#include "net/messagein.h"
#include "net/messageout.h"
#include "net/netcomputer.h"
#include "utils/logger.h"
#include "utils/point.h"
#include "utils/stringfilter.h"
#include "utils/tokencollector.h"
#include "utils/tokendispenser.h"
#include "utils/sha256.h"
#include "utils/string.h"
#include "utils/xml.h"

using namespace ManaServ;

class AccountHandler : public ConnectionHandler
{
public:
    AccountHandler(IConfiguration *configuration,
                   IStorage *storage,
                   const QString &attributesFile);

    /**
     * Called by the token collector in order to associate a client to its
     * account ID.
     */
    void tokenMatched(AccountClient *client, int accountID);

    /**
     * Called by the token collector when a client was not acknowledged for
     * some time and should be disconnected.
     */
    void deletePendingClient(AccountClient *client);

    /**
     * Called by the token collector.
     */
    void deletePendingConnect(int) {}

    /**
     * Token collector for connecting a client coming from a game server
     * without having to provide username and password a second time.
     */
    TokenCollector<AccountHandler, AccountClient *, int> mTokenCollector;

protected:
    /**
     * Processes account related messages.
     */
    void processMessage(NetComputer *client, MessageIn &message);

    NetComputer *computerConnected(ENetPeer *peer);

    void computerDisconnected(NetComputer *comp);

private:
    void handleLoginRandTriggerMessage(AccountClient &client, MessageIn &msg);
    void handleLoginMessage(AccountClient &client, MessageIn &msg);
    void handleLogoutMessage(AccountClient &client);
    void handleReconnectMessage(AccountClient &client, MessageIn &msg);
    void handleRegisterMessage(AccountClient &client, MessageIn &msg);
    void handleUnregisterMessage(AccountClient &client, MessageIn &msg);
    void handleRequestRegisterInfoMessage(AccountClient &client, MessageIn &msg);
    void handleEmailChangeMessage(AccountClient &client, MessageIn &msg);
    void handlePasswordChangeMessage(AccountClient &client, MessageIn &msg);
    void handleCharacterCreateMessage(AccountClient &client, MessageIn &msg);
    void handleCharacterSelectMessage(AccountClient &client, MessageIn &msg);
    void handleCharacterDeleteMessage(AccountClient &client, MessageIn &msg);

    void addServerInfo(MessageOut *msg);

    IConfiguration *mConfiguration;
    IStorage *mStorage;

    /** List of all accounts which requested a random seed, but are not logged
     *  yet. This list will be regularly remove (after timeout) old accounts
     */
    std::map<QString, std::unique_ptr<Account>> mPendingAccounts;

    /** List of attributes that the client can send at account creation. */
    std::vector<int> mModifiableAttributes;

    /**
     * Default attributes and their values, loaded from the attributes file.
     */
    AttributeMap mDefaultAttributes;

    int mStartingPoints;   /**< Character's starting points. */
    int mAttributeMinimum; /**< Minimum value for customized attributes. */
    int mAttributeMaximum; /**< Maximum value for customized attributes. */

    int mNumHairStyles;
    int mNumHairColors;
    int mNumGenders;
    int mMinNameLength;
    int mMaxNameLength;
    int mMaxCharacters;

    bool mRegistrationAllowed;

    QString mUpdateHost;
    QString mDataUrl;

    typedef std::map<int, time_t> IPsToTime;
    IPsToTime mLastLoginAttemptForIP;
};

static AccountHandler *accountHandler;

AccountHandler::AccountHandler(IConfiguration *configuration,
                               IStorage *storage,
                               const QString &attributesFile):
    ConnectionHandler(configuration),
    mTokenCollector(this),
    mConfiguration(configuration),
    mStorage(storage),
    mStartingPoints(0),
    mAttributeMinimum(0),
    mAttributeMaximum(0),
    mNumHairStyles(mConfiguration->getValue("char_numHairStyles", 17)),
    mNumHairColors(mConfiguration->getValue("char_numHairColors", 11)),
    mNumGenders(mConfiguration->getValue("char_numGenders", 2)),
    mMinNameLength(mConfiguration->getValue("char_minNameLength", 4)),
    mMaxNameLength(mConfiguration->getValue("char_maxNameLength", 25)),
    mMaxCharacters(mConfiguration->getValue("account_maxCharacters", 3)),
    mRegistrationAllowed(mConfiguration->getBoolValue("account_allowRegister", true)),
    mUpdateHost(mConfiguration->getValue("net_defaultUpdateHost", QString())),
    mDataUrl(mConfiguration->getValue("net_clientDataUrl", QString()))
{
    XML::Document doc(attributesFile);
    xmlNodePtr node = doc.rootNode();

    if (!node || !xmlStrEqual(node->name, BAD_CAST "attributes"))
    {
        LOG_FATAL("Account handler: " << attributesFile << ": "
                  << " is not a valid database file!");
        exit(EXIT_XML_BAD_PARAMETER);
    }

    for_each_xml_child_node(attributenode, node)
    {
        if (xmlStrEqual(attributenode->name, BAD_CAST "attribute"))
        {
            int id = XML::getProperty(attributenode, "id", 0);
            if (!id)
            {
                LOG_WARN("Account handler: " << attributesFile << ": "
                         << "An invalid attribute id value (0) has been found "
                         << "and will be ignored.");
                continue;
            }

            if (XML::getBoolProperty(attributenode, "modifiable", false))
                mModifiableAttributes.push_back(id);

            // Store as string initially to check
            // that the property is defined.
            QString defStr = XML::getProperty(attributenode, "default",
                                                  QString());
            if (!defStr.isEmpty())
            {
                const double val = defStr.toDouble();
                mDefaultAttributes.insert(std::make_pair(id, val));
            }
        }
        else if (xmlStrEqual(attributenode->name, BAD_CAST "points"))
        {
            mStartingPoints = XML::getProperty(attributenode, "start", 0);
            mAttributeMinimum = XML::getProperty(attributenode, "minimum", 0);
            mAttributeMaximum = XML::getProperty(attributenode, "maximum", 0);

            // Stops if not all the values are given.
            if (!mStartingPoints || !mAttributeMinimum || !mAttributeMaximum)
            {
                LOG_FATAL("Account handler: " << attributesFile << ": "
                          << " The characters starting points "
                          << "are incomplete or not set!");
                exit(EXIT_XML_BAD_PARAMETER);
            }
        }
    } // End for each XML nodes

    if (mModifiableAttributes.empty())
    {
        LOG_FATAL("Account handler: " << attributesFile << ": "
                  << "No modifiable attributes found!");
        exit(EXIT_XML_BAD_PARAMETER);
    }

    int attributeCount = (int) mModifiableAttributes.size();
    if (attributeCount * mAttributeMaximum < mStartingPoints ||
        attributeCount * mAttributeMinimum > mStartingPoints)
    {
        LOG_FATAL("Account handler: " << attributesFile << ": "
                  << "Character's point values make "
                  << "the character's creation impossible!");
        exit(EXIT_XML_BAD_PARAMETER);
    }

    LOG_DEBUG("Character start points: " << mStartingPoints << " (Min: "
              << mAttributeMinimum << ", Max: " << mAttributeMaximum << ")");
}

bool AccountClientHandler::initialize(const QString &attributesFile, int port,
                                      const QString &host,
                                      IConfiguration *configuration,
                                      IStorage *storage)
{
    accountHandler = new AccountHandler(configuration, storage, attributesFile);
    LOG_INFO("Account handler started:");

    return accountHandler->startListen(port, host);
}

void AccountClientHandler::deinitialize()
{
    accountHandler->stopListen();
    delete accountHandler;
    accountHandler = nullptr;
}

void AccountClientHandler::process()
{
    accountHandler->process(50);
}

void AccountClientHandler::prepareReconnect(const QString &token, int id)
{
    accountHandler->mTokenCollector.addPendingConnect(token, id);
}

NetComputer *AccountHandler::computerConnected(ENetPeer *peer)
{
    return new AccountClient(peer);
}

void AccountHandler::computerDisconnected(NetComputer *comp)
{
    AccountClient *client = static_cast<AccountClient *>(comp);

    if (client->status == CLIENT_QUEUED)
        // Delete it from the pendingClient list
        mTokenCollector.deletePendingClient(client);

    delete client; // ~AccountClient unsets the account
}

static void sendCharacterData(MessageOut &charInfo, const CharacterData &ch)
{
    charInfo.writeInt8(ch.getCharacterSlot());
    charInfo.writeString(ch.getName());
    charInfo.writeInt8(ch.getGender());
    charInfo.writeInt8(ch.getHairStyle());
    charInfo.writeInt8(ch.getHairColor());
    charInfo.writeInt16(ch.getAttributePoints());
    charInfo.writeInt16(ch.getCorrectionPoints());

    auto &possessions = ch.getPossessions();
    auto &equipData = possessions.getEquipment();
    auto &inventoryData = possessions.getInventory();
    charInfo.writeInt8(equipData.size());

    for (int itemSlot : equipData)
    {
        const auto &it = inventoryData.find(itemSlot);
        charInfo.writeInt16(it->second.equipmentSlot);
        charInfo.writeInt16(it->second.itemId);
    }

    charInfo.writeInt8(ch.getAttributes().size());
    for (auto &it : ch.getAttributes())
    {
        // {id, base value in 256ths, modified value in 256ths }*
        charInfo.writeInt32(it.first);
        charInfo.writeInt32((int) (it.second.base * 256));
        charInfo.writeInt32((int) (it.second.modified * 256));
    }
}

static void sendFullCharacterData(AccountClient *client,
                                  const std::map<unsigned, std::unique_ptr<CharacterData>> &chars)
{
    MessageOut msg(APMSG_CHAR_INFO);
    for (auto &charIt : chars)
        sendCharacterData(msg, *charIt.second);
    client->send(msg);
}

static QString getRandomString(int length)
{
    QString s(length);
    // No need to care about zeros. They can be handled.
    // But care for endianness
    for (int i = 0; i < length; ++i)
        s[i] = (char)rand();

    return s;
}

void AccountHandler::handleLoginRandTriggerMessage(AccountClient &client, MessageIn &msg)
{
    QString salt = getRandomString(4);
    QString username = msg.readString();

    if (auto account = mStorage->getAccount(username))
    {
        account->setRandomSalt(salt);
        mPendingAccounts.emplace(account->getName(), std::move(account));
    }
    MessageOut reply(APMSG_LOGIN_RNDTRGR_RESPONSE);
    reply.writeString(salt);
    client.send(reply);
}

void AccountHandler::handleLoginMessage(AccountClient &client, MessageIn &msg)
{
    MessageOut reply(APMSG_LOGIN_RESPONSE);

    if (client.status != CLIENT_LOGIN)
    {
        reply.writeInt8(ERRMSG_FAILURE);
        client.send(reply);
        return;
    }

    client.version = msg.readInt32();

    if (client.version < MIN_PROTOCOL_VERSION)
    {
        reply.writeInt8(LOGIN_INVALID_VERSION);
        client.send(reply);
        return;
    }

    // Check whether the last login attempt for this IP is still too fresh
    const int address = client.getIP();
    const time_t now = time(nullptr);
    IPsToTime::const_iterator it = mLastLoginAttemptForIP.find(address);
    if (it != mLastLoginAttemptForIP.end())
    {
        const time_t lastAttempt = it->second;
        if (now < lastAttempt + 1)
        {
            reply.writeInt8(LOGIN_INVALID_TIME);
            client.send(reply);
            return;
        }
    }
    mLastLoginAttemptForIP[address] = now;

    const QString username = msg.readString();
    const QString password = msg.readString();

    if (stringFilter->findDoubleQuotes(username))
    {
        reply.writeInt8(ERRMSG_INVALID_ARGUMENT);
        client.send(reply);
        return;
    }

    const unsigned maxClients =
            (unsigned) mConfiguration->getValue("net_maxClients", 1000);

    if (getClientCount() >= maxClients)
    {
        reply.writeInt8(ERRMSG_SERVER_FULL);
        client.send(reply);
        return;
    }

    // Check if the account exists
    std::unique_ptr<Account> account = std::move(mPendingAccounts.at(username));
    mPendingAccounts.erase(username);

    if (!account || QString::fromStdString(
                    sha256((account->getPassword() + account->getRandomSalt())
                               .toStdString())) != password) {
        reply.writeInt8(ERRMSG_INVALID_ARGUMENT);
        client.send(reply);
        account.release();
        return;
    }

    if (account->getLevel() == AL_BANNED)
    {
        reply.writeInt8(LOGIN_BANNED);
        client.send(reply);
        account.release();
        return;
    }

    // The client successfully logged in...

    // Set lastLogin date of the account.
    time_t login;
    time(&login);
    account->setLastLogin(login);
    mStorage->updateLastLogin(*account);

    client.status = CLIENT_CONNECTED;

    reply.writeInt8(ERRMSG_OK);
    addServerInfo(&reply);

    auto &chars = account->getCharacters();

    if (client.version < 10) {
        client.send(reply);
        sendFullCharacterData(&client, chars);
    } else {
        for (auto &charIt : chars)
            sendCharacterData(reply, *charIt.second);
        client.send(reply);
    }

    client.setAccount(std::move(account));
}

void AccountHandler::handleLogoutMessage(AccountClient &client)
{
    MessageOut reply(APMSG_LOGOUT_RESPONSE);

    if (client.status == CLIENT_LOGIN)
    {
        reply.writeInt8(ERRMSG_NO_LOGIN);
    }
    else if (client.status == CLIENT_CONNECTED)
    {
        client.releaseAccount();
        client.status = CLIENT_LOGIN;
        reply.writeInt8(ERRMSG_OK);
    }
    else if (client.status == CLIENT_QUEUED)
    {
        // Delete it from the pendingClient list
        mTokenCollector.deletePendingClient(&client);
        client.status = CLIENT_LOGIN;
        reply.writeInt8(ERRMSG_OK);
    }
    client.send(reply);
}

void AccountHandler::handleReconnectMessage(AccountClient &client,
                                            MessageIn &msg)
{
    if (client.status != CLIENT_LOGIN)
    {
        LOG_DEBUG("Account tried to reconnect, but was already logged in "
                  "or queued.");
        return;
    }

    QString magic_token = msg.readString(MAGIC_TOKEN_LENGTH);
    client.status = CLIENT_QUEUED; // Before the addPendingClient
    mTokenCollector.addPendingClient(magic_token, &client);
}

static bool checkCaptcha(AccountClient &, const QString & /* captcha */)
{
    // TODO
    return true;
}

void AccountHandler::handleRegisterMessage(AccountClient &client,
                                           MessageIn &msg)
{
    int clientVersion = msg.readInt32();
    QString username = msg.readString();
    QString password = msg.readString();
    QString email = msg.readString();
    QString captcha = msg.readString();

    MessageOut reply(APMSG_REGISTER_RESPONSE);

    if (client.status != CLIENT_LOGIN)
    {
        reply.writeInt8(ERRMSG_FAILURE);
    }
    else if (!mRegistrationAllowed)
    {
        reply.writeInt8(ERRMSG_FAILURE);
    }
    else if (clientVersion < MIN_PROTOCOL_VERSION)
    {
        reply.writeInt8(REGISTER_INVALID_VERSION);
    }
    else if (stringFilter->findDoubleQuotes(username)
             || stringFilter->findDoubleQuotes(email)
             || username.length() < mMinNameLength
             || username.length() > mMaxNameLength
             || !stringFilter->isEmailValid(email)
             || !stringFilter->filterContent(username))
    {
        reply.writeInt8(ERRMSG_INVALID_ARGUMENT);
    }
    else if (mStorage->doesUserNameExist(username))
    {
        reply.writeInt8(REGISTER_EXISTS_USERNAME);
    }
    else if (mStorage->doesEmailAddressExist(QString::fromStdString(sha256(email.toStdString()))))
    {
        reply.writeInt8(REGISTER_EXISTS_EMAIL);
    }
    else if (!checkCaptcha(client, captcha))
    {
        reply.writeInt8(REGISTER_CAPTCHA_WRONG);
    }
    else
    {
        std::unique_ptr<Account> account(new Account);
        account->setName(username);
        account->setPassword(QString::fromStdString(sha256(password.toStdString())));
        // We hash email server-side for additional privacy
        // we ask for it again when we need it and verify it
        // through comparing it with the hash.
        account->setEmail(QString::fromStdString(sha256(email.toStdString())));
        account->setLevel(AL_PLAYER);

        // Set the date and time of the account registration, and the last login
        time_t regdate;
        time(&regdate);
        account->setRegistrationDate(regdate);
        account->setLastLogin(regdate);

        mStorage->addAccount(*account);
        reply.writeInt8(ERRMSG_OK);
        addServerInfo(&reply);

        // Associate account with connection
        client.setAccount(std::move(account));
        client.status = CLIENT_CONNECTED;
    }

    client.send(reply);
}

void AccountHandler::handleUnregisterMessage(AccountClient &client,
                                             MessageIn &msg)
{
    LOG_DEBUG("AccountHandler::handleUnregisterMessage");

    MessageOut reply(APMSG_UNREGISTER_RESPONSE);

    if (client.status != CLIENT_CONNECTED)
    {
        reply.writeInt8(ERRMSG_FAILURE);
        client.send(reply);
        return;
    }

    QString username = msg.readString();
    QString password = msg.readString();

    if (stringFilter->findDoubleQuotes(username))
    {
        reply.writeInt8(ERRMSG_INVALID_ARGUMENT);
        client.send(reply);
        return;
    }

    // See whether the account exists
    const std::unique_ptr<Account> account = mStorage->getAccount(username);

    if (!account || account->getPassword() != QString::fromStdString(sha256(password.toStdString())))
    {
        reply.writeInt8(ERRMSG_INVALID_ARGUMENT);
        client.send(reply);
        return;
    }

    // Delete account and associated characters
    LOG_INFO("Unregistered \"" << username
             << "\", AccountID: " << account->getId());
    mStorage->delAccount(*account);
    reply.writeInt8(ERRMSG_OK);

    client.send(reply);
}

void AccountHandler::handleRequestRegisterInfoMessage(AccountClient &client,
                                                      MessageIn &)
{
    LOG_INFO("AccountHandler::handleRequestRegisterInfoMessage");
    MessageOut reply(APMSG_REGISTER_INFO_RESPONSE);
    if (!mConfiguration->getBoolValue("account_allowRegister", true))
    {
        reply.writeInt8(false);
        reply.writeString(mConfiguration->getValue(
                              "account_denyRegisterReason", QString()));
    }
    else
    {
        reply.writeInt8(true);
        reply.writeInt8(mMinNameLength);
        reply.writeInt8(mMaxNameLength);
        reply.writeString("http://www.server.example/captcha.png");
        reply.writeString("<instructions for solving captcha>");
    }
    client.send(reply);
}

void AccountHandler::handleEmailChangeMessage(AccountClient &client,
                                              MessageIn &msg)
{
    MessageOut reply(APMSG_EMAIL_CHANGE_RESPONSE);

    const std::unique_ptr<Account> &account = client.getAccount();
    if (!account)
    {
        reply.writeInt8(ERRMSG_NO_LOGIN);
        client.send(reply);
        return;
    }

    const QString email = msg.readString();
    const QString emailHash = QString::fromStdString(sha256(email.toStdString()));

    if (!stringFilter->isEmailValid(email))
    {
        reply.writeInt8(ERRMSG_INVALID_ARGUMENT);
    }
    else if (stringFilter->findDoubleQuotes(email))
    {
        reply.writeInt8(ERRMSG_INVALID_ARGUMENT);
    }
    else if (mStorage->doesEmailAddressExist(emailHash))
    {
        reply.writeInt8(ERRMSG_EMAIL_ALREADY_EXISTS);
    }
    else
    {
        account->setEmail(emailHash);
        // Keep the database up to date otherwise we will go out of sync
        mStorage->flush(*account);
        reply.writeInt8(ERRMSG_OK);
    }
    client.send(reply);
}

void AccountHandler::handlePasswordChangeMessage(AccountClient &client,
                                                 MessageIn &msg)
{
    QString oldPassword = QString::fromStdString(sha256(msg.readString().toStdString()));
    QString newPassword = QString::fromStdString(sha256(msg.readString().toStdString()));

    MessageOut reply(APMSG_PASSWORD_CHANGE_RESPONSE);

    const std::unique_ptr<Account> &account = client.getAccount();
    if (!account)
    {
        reply.writeInt8(ERRMSG_NO_LOGIN);
    }
    else if (stringFilter->findDoubleQuotes(newPassword))
    {
        reply.writeInt8(ERRMSG_INVALID_ARGUMENT);
    }
    else if (oldPassword != account->getPassword())
    {
        reply.writeInt8(ERRMSG_FAILURE);
    }
    else
    {
        account->setPassword(newPassword);
        // Keep the database up to date otherwise we will go out of sync
        mStorage->flush(*account);
        reply.writeInt8(ERRMSG_OK);
    }

    client.send(reply);
}

void AccountHandler::handleCharacterCreateMessage(AccountClient &client,
                                                  MessageIn &msg)
{
    QString name = msg.readString();
    int hairStyle = msg.readInt8();
    int hairColor = msg.readInt8();
    int gender = msg.readInt8();

    // Avoid creation of character from old clients.
    int slot = -1;
    if (msg.getUnreadLength() > 7)
        slot = msg.readInt8();

    MessageOut reply(APMSG_CHAR_CREATE_RESPONSE);

    const std::unique_ptr<Account> &account = client.getAccount();
    if (!account)
    {
        reply.writeInt8(ERRMSG_NO_LOGIN);
    }
    else if (!stringFilter->filterContent(name))
    {
        reply.writeInt8(ERRMSG_INVALID_ARGUMENT);
    }
    else if (stringFilter->findDoubleQuotes(name))
    {
        reply.writeInt8(ERRMSG_INVALID_ARGUMENT);
    }
    else if (hairStyle > mNumHairStyles)
    {
        reply.writeInt8(CREATE_INVALID_HAIRSTYLE);
    }
    else if (hairColor > mNumHairColors)
    {
        reply.writeInt8(CREATE_INVALID_HAIRCOLOR);
    }
    else if (gender > mNumGenders)
    {
        reply.writeInt8(CREATE_INVALID_GENDER);
    }
    else if ((name.length() < mMinNameLength) ||
             (name.length() > mMaxNameLength))
    {
        reply.writeInt8(ERRMSG_INVALID_ARGUMENT);
    }
    else
    {
        if (mStorage->doesCharacterNameExist(name))
        {
            reply.writeInt8(CREATE_EXISTS_NAME);
            client.send(reply);
            return;
        }

        // An account shouldn't have more
        // than <account_maxCharacters> characters.
        auto &chars = account->getCharacters();
        if (slot < 1 || slot > mMaxCharacters
            || !account->isSlotEmpty((unsigned) slot))
        {
            reply.writeInt8(CREATE_INVALID_SLOT);
            client.send(reply);
            return;
        }

        if ((int)chars.size() >= mMaxCharacters)
        {
            reply.writeInt8(CREATE_TOO_MUCH_CHARACTERS);
            client.send(reply);
            return;
        }

        // TODO: Add race, face and maybe special attributes.

        // Customization of character's attributes...
        std::vector<int> attributes = std::vector<int>(mModifiableAttributes.size(), 0);
        for (unsigned i = 0; i < mModifiableAttributes.size(); ++i)
            attributes[i] = msg.readInt16();

        int totalAttributes = 0;
        for (unsigned i = 0; i < mModifiableAttributes.size(); ++i)
        {
            // For good total attributes check.
            totalAttributes += attributes.at(i);

            // For checking if all stats are >= min and <= max.
            if (attributes.at(i) < mAttributeMinimum
                || attributes.at(i) > mAttributeMaximum)
            {
                reply.writeInt8(CREATE_ATTRIBUTES_OUT_OF_RANGE);
                client.send(reply);
                return;
            }
        }

        if (totalAttributes > mStartingPoints)
        {
            reply.writeInt8(CREATE_ATTRIBUTES_TOO_HIGH);
        }
        else if (totalAttributes < mStartingPoints)
        {
            reply.writeInt8(CREATE_ATTRIBUTES_TOO_LOW);
        }
        else
        {
            std::unique_ptr<CharacterData> newCharacter(new CharacterData(name));

            // Set the initial attributes provided by the client
            for (unsigned i = 0; i < mModifiableAttributes.size(); ++i)
            {
                newCharacter->mAttributes.insert(
                            std::make_pair(mModifiableAttributes.at(i), attributes[i]));
            }

            newCharacter->mAttributes.insert(mDefaultAttributes.begin(),
                                             mDefaultAttributes.end());
            newCharacter->setAccountID(account->getId());
            newCharacter->setAccountLevel(account->getLevel());
            newCharacter->setCharacterSlot(slot);
            newCharacter->setGender(gender);
            newCharacter->setHairStyle(hairStyle);
            newCharacter->setHairColor(hairColor);
            newCharacter->setMapId(mConfiguration->getValue("char_startMap", 1));
            Point startingPos(mConfiguration->getValue("char_startX", 1024),
                              mConfiguration->getValue("char_startY", 1024));
            newCharacter->setPosition(startingPos);

            // log transaction
            Transaction trans;
            trans.mCharacterId = newCharacter->getDatabaseId();
            trans.mAction = TRANS_CHAR_CREATE;
            trans.mMessage = account->getName() + " created character ";
            trans.mMessage.append("called " + name);
            mStorage->addTransaction(trans);

            reply.writeInt8(ERRMSG_OK);

            sendCharacterData(reply, *newCharacter);
            client.send(reply);

            account->addCharacter(std::move(newCharacter));

            LOG_INFO("Character " << name << " was created for "
                     << account->getName() << "'s account.");

            mStorage->flush(*account); // flush changes
            return;
        }
    }

    client.send(reply);
}

void AccountHandler::handleCharacterSelectMessage(AccountClient &client,
                                                  MessageIn &msg)
{
    MessageOut reply(APMSG_CHAR_SELECT_RESPONSE);

    const std::unique_ptr<Account> &account = client.getAccount();
    if (!account)
    {
        reply.writeInt8(ERRMSG_NO_LOGIN);
        client.send(reply);
        return; // not logged in
    }

    int slot = msg.readInt8();
    auto &chars = account->getCharacters();

    if (chars.find(slot) == chars.end())
    {
        // Invalid char selection
        reply.writeInt8(ERRMSG_INVALID_ARGUMENT);
        client.send(reply);
        return;
    }

    CharacterData &selectedChar = *chars[slot];

    QString address;
    int port;
    if (!GameServerHandler::getGameServerFromMap
            (selectedChar.getMapId(), address, port))
    {
        LOG_ERROR("Character Selection: No game server for map #"<<selectedChar.getMapId());
        reply.writeInt8(ERRMSG_FAILURE);
        client.send(reply);
        return;
    }

    reply.writeInt8(ERRMSG_OK);

    LOG_DEBUG(selectedChar.getName() << " is trying to enter the servers.");

    QString magic_token(utils::getMagicToken());
    reply.writeString(magic_token, MAGIC_TOKEN_LENGTH);
    reply.writeString(address);
    reply.writeInt16(port);

    // Give address and port for the chat server
    reply.writeString(mConfiguration->getValue("net_publicChatHost",
                         mConfiguration->getValue("net_chatHost", "localhost")));

    // When the chatListenToClientPort is set, we use it.
    // Otherwise, we use the accountListenToClientPort + 2 if the option is set.
    // If neither, the DEFAULT_SERVER_PORT + 2 is used.
    const int alternativePort =
        mConfiguration->getValue("net_accountListenToClientPort",
                                DEFAULT_SERVER_PORT) + 2;
    reply.writeInt16(mConfiguration->getValue("net_chatListenToClientPort",
                                             alternativePort));

    GameServerHandler::registerClient(magic_token, selectedChar);
    registerChatClient(magic_token, selectedChar.getName(), account->getLevel());

    client.send(reply);

    // log transaction
    Transaction trans;
    trans.mCharacterId = selectedChar.getDatabaseId();
    trans.mAction = TRANS_CHAR_SELECTED;
    mStorage->addTransaction(trans);
}

void AccountHandler::handleCharacterDeleteMessage(AccountClient &client,
                                                  MessageIn &msg)
{
    MessageOut reply(APMSG_CHAR_DELETE_RESPONSE);

    const std::unique_ptr<Account> &account = client.getAccount();
    if (!account)
    {
        reply.writeInt8(ERRMSG_NO_LOGIN);
        client.send(reply);
        return; // not logged in
    }

    int slot = msg.readInt8();
    auto &chars = account->getCharacters();

    if (slot < 1 || account->isSlotEmpty(slot))
    {
        // Invalid char selection
        reply.writeInt8(ERRMSG_INVALID_ARGUMENT);
        client.send(reply);
        return;
    }

    QString characterName = chars[slot]->getName();
    LOG_INFO("Character deleted:" << characterName);

    // Log transaction
    Transaction trans;
    trans.mCharacterId = chars[slot]->getDatabaseId();
    trans.mAction = TRANS_CHAR_DELETED;
    trans.mMessage = chars[slot]->getName() + " deleted by ";
    trans.mMessage.append(account->getName());
    mStorage->addTransaction(trans);

    account->delCharacter(slot);
    mStorage->flush(*account);

    reply.writeInt8(ERRMSG_OK);
    client.send(reply);
}

/**
 * Adds server specific info to the current message
 *
 * The info are made of:
 * (String) Update Host URL (or "")
 * (String) Client Data URL (or "")
 * (Byte)   Number of maximum character slots (empty or not)
 */
void AccountHandler::addServerInfo(MessageOut *msg)
{
    msg->writeString(mUpdateHost);
    msg->writeString(mDataUrl);
    msg->writeInt8(mMaxCharacters);
}

void AccountHandler::tokenMatched(AccountClient *client, int accountId)
{
    MessageOut reply(APMSG_RECONNECT_RESPONSE);

    // Associate account with connection.
    std::unique_ptr<Account> account = mStorage->getAccount(accountId);
    client->status = CLIENT_CONNECTED;

    reply.writeInt8(ERRMSG_OK);
    client->send(reply);

    // Return information about available characters
    auto &chars = account->getCharacters();

    // Send characters list
    sendFullCharacterData(client, chars);

    client->setAccount(std::move(account));
}

void AccountHandler::deletePendingClient(AccountClient *client)
{
    MessageOut msg(APMSG_RECONNECT_RESPONSE);
    msg.writeInt8(ERRMSG_TIME_OUT);
    client->disconnect(msg);
    // The client will be deleted when the disconnect event is processed
}

void AccountHandler::processMessage(NetComputer *comp, MessageIn &message)
{
    AccountClient &client = *static_cast< AccountClient * >(comp);

    switch (message.getId())
    {
        case PAMSG_LOGIN_RNDTRGR:
            LOG_DEBUG("Received msg ... PAMSG_LOGIN_RANDTRIGGER");
            handleLoginRandTriggerMessage(client, message);
            break;

        case PAMSG_LOGIN:
            LOG_DEBUG("Received msg ... PAMSG_LOGIN");
            handleLoginMessage(client, message);
            break;

        case PAMSG_LOGOUT:
            LOG_DEBUG("Received msg ... PAMSG_LOGOUT");
            handleLogoutMessage(client);
            break;

        case PAMSG_RECONNECT:
            LOG_DEBUG("Received msg ... PAMSG_RECONNECT");
            handleReconnectMessage(client, message);
            break;

        case PAMSG_REGISTER:
            LOG_DEBUG("Received msg ... PAMSG_REGISTER");
            handleRegisterMessage(client, message);
            break;

        case PAMSG_UNREGISTER:
            LOG_DEBUG("Received msg ... PAMSG_UNREGISTER");
            handleUnregisterMessage(client, message);
            break;

        case PAMSG_REQUEST_REGISTER_INFO :
            LOG_DEBUG("Received msg ... REQUEST_REGISTER_INFO");
            handleRequestRegisterInfoMessage(client, message);
            break;

        case PAMSG_EMAIL_CHANGE:
            LOG_DEBUG("Received msg ... PAMSG_EMAIL_CHANGE");
            handleEmailChangeMessage(client, message);
            break;

        case PAMSG_PASSWORD_CHANGE:
            LOG_DEBUG("Received msg ... PAMSG_PASSWORD_CHANGE");
            handlePasswordChangeMessage(client, message);
            break;

        case PAMSG_CHAR_CREATE:
            LOG_DEBUG("Received msg ... PAMSG_CHAR_CREATE");
            handleCharacterCreateMessage(client, message);
            break;

        case PAMSG_CHAR_SELECT:
            LOG_DEBUG("Received msg ... PAMSG_CHAR_SELECT");
            handleCharacterSelectMessage(client, message);
            break;

        case PAMSG_CHAR_DELETE:
            LOG_DEBUG("Received msg ... PAMSG_CHAR_DELETE");
            handleCharacterDeleteMessage(client, message);
            break;

        default:
            LOG_WARN("AccountHandler::processMessage, Invalid message type "
                     << message.getId());
            MessageOut result(XXMSG_INVALID);
            client.send(result);
            break;
    }
}
