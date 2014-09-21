/*
 *  The Mana Server
 *  Copyright (C) 2014-2014  The Mana Developers
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

#include "characterdatautils.h"

void CharacterDataUtils::serialize(CharacterData &data, MessageOut &msg)
{
    // general character properties
    msg.writeInt8(data.getAccountLevel());
    msg.writeInt8(data.getGender());
    msg.writeInt8(data.getHairStyle());
    msg.writeInt8(data.getHairColor());
    msg.writeInt16(data.getAttributePoints());
    msg.writeInt16(data.getCorrectionPoints());


    const AttributeMap &attributes = data.getAttributes();
    msg.writeInt16(attributes.size());
    for (auto attributeIt : attributes)
    {
        msg.writeInt16(attributeIt.first);
        msg.writeDouble(attributeIt.second.getBase());
    }

    // status effects currently affecting the character
    msg.writeInt16(data.getStatusEffectSize());
    std::map<int, Status>::const_iterator status_it;
    for (status_it = data.getStatusEffectBegin(); status_it != data.getStatusEffectEnd(); status_it++)
    {
        msg.writeInt16(status_it->first);
        msg.writeInt16(status_it->second.time);
    }

    // location
    msg.writeInt16(data.getMapId());
    const Point &pos = data.getPosition();
    msg.writeInt16(pos.x);
    msg.writeInt16(pos.y);

    // kill count
    msg.writeInt16(data.getKillCountSize());
    std::map<int, int>::const_iterator kills_it;
    for (kills_it = data.getKillCountBegin(); kills_it != data.getKillCountEnd(); kills_it++)
    {
        msg.writeInt16(kills_it->first);
        msg.writeInt32(kills_it->second);
    }

    // character abilities
    const std::set<int> &abilities = data.getAbilities();
    msg.writeInt16(abilities.size());
    for (auto &abilityId : abilities) {
        msg.writeInt32(abilityId);
    }


    // questlog
    const auto &quests = data.getQuests();
    msg.writeInt16(quests.size());
    for (const QuestInfo &quest : quests) {
        msg.writeInt16(quest.id);
        msg.writeInt8(quest.state);
        msg.writeString(quest.title);
        msg.writeString(quest.description);
    }

    // inventory - must be last because size isn't transmitted
    const Possessions &poss = data.getPossessions();
    const EquipData &equipData = poss.getEquipment();

    const InventoryData &inventoryData = poss.getInventory();
    for (const auto &elem : inventoryData) {
        int slot = elem.first;
        msg.writeInt16(slot);
        msg.writeInt16(elem.second.itemId);
        msg.writeInt16(elem.second.amount);
        if (equipData.find(elem.first) != equipData.end())
            msg.writeInt8(1); // equipped
        else
            msg.writeInt8(0); // not equipped
    }
}

void CharacterDataUtils::deserialize(CharacterData &data, MessageIn &msg)
{
    // general character properties
    data.setAccountLevel(msg.readInt8());
    data.setGender(ManaServ::getGender(msg.readInt8()));
    data.setHairStyle(msg.readInt8());
    data.setHairColor(msg.readInt8());
    data.setAttributePoints(msg.readInt16());
    data.setCorrectionPoints(msg.readInt16());

    // character attributes
    unsigned attrSize = msg.readInt16();
    for (unsigned i = 0; i < attrSize; ++i)
    {
        unsigned id = msg.readInt16();
        double base = msg.readDouble(),
               mod  = msg.readDouble();
        data.setAttribute(id, base);
        data.setModAttribute(id, mod);
    }

    // status effects currently affecting the character
    int statusSize = msg.readInt16();

    for (int i = 0; i < statusSize; i++)
    {
        int status = msg.readInt16();
        int time = msg.readInt16();
        data.applyStatusEffect(status, time);
    }

    // location
    data.setMapId(msg.readInt16());

    Point temporaryPoint;
    temporaryPoint.x = msg.readInt16();
    temporaryPoint.y = msg.readInt16();
    data.setPosition(temporaryPoint);

    // kill count
    int killSize = msg.readInt16();
    for (int i = 0; i < killSize; i++)
    {
        int monsterId = msg.readInt16();
        int kills = msg.readInt32();
        data.setKillCount(monsterId, kills);
    }

    // character abilities
    int abilitiesSize = msg.readInt16();
    data.clearAbilities();
    for (int i = 0; i < abilitiesSize; i++)
    {
        const int id = msg.readInt32();
        data.giveAbility(id);
    }

    // questlog
    int questlogSize = msg.readInt16();
    data.clearQuests();
    for (int i = 0; i < questlogSize; ++i) {
        QuestInfo quest;
        quest.id = msg.readInt16();
        quest.state = msg.readInt8();
        quest.title = msg.readString();
        quest.description = msg.readString();
        data.addQuest(quest);
    }

    // inventory - must be last because size isn't transmitted
    Possessions &poss = data.getPossessions();

    InventoryData inventoryData;
    EquipData equipmentData;
    while (msg.getUnreadLength())
    {
        InventoryItem i;
        i.slot          = msg.readInt16();
        i.itemId        = msg.readInt16();
        i.amount        = msg.readInt16();
        i.equipmentSlot = msg.readInt8();
        inventoryData.insert(std::make_pair(i.slot, i));
        if (i.equipmentSlot != 0)
            equipmentData.insert(i.slot);
    }
    poss.setInventory(inventoryData);
    poss.setEquipment(equipmentData);
}
