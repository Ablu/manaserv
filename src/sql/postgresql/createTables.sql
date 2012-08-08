--
-- The Mana Server
-- Copyright (C) 2009  The Mana World Development Team
--
-- This file is part of The Mana Server.
--
-- The Mana Server is free software; you can redistribute it and/or modify
-- it under the terms of the GNU General Public License as published by
-- the Free Software Foundation; either version 2 of the License, or
-- any later version.
--
-- The Mana Server is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU General Public License for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with The Mana Server.  If not, see <http://www.gnu.org/licenses/>.
--

CREATE TABLE mana_accounts
(
    "id"            SERIAL      PRIMARY KEY,
    "username"      TEXT        NOT NULL UNIQUE,
    "password"      TEXT        NOT NULL,
    "email"         TEXT        NOT NULL,
    "level"         SMALLINT    NOT NULL,
    "banned"        INTEGER     NOT NULL,
    "registration"  INTEGER     NOT NULL,
    "lastlogin"     INTEGER     NOT NULL,
    "authorization" TEXT        DEFAULT NULL,
    "expiration"    INTEGER     DEFAULT NULL
);

CREATE TABLE mana_characters
(
    "id"            SERIAL      PRIMARY KEY,
    "user_id"       INTEGER     NOT NULL,
    "name"          TEXT        NOT NULL UNIQUE,
    "gender"        SMALLINT    NOT NULL,
    "hair_style"    SMALLINT    NOT NULL,
    "hair_color"    SMALLINT    NOT NULL,
    "level"         INTEGER     NOT NULL,
    "char_pts"      INTEGER     NOT NULL,
    "correct_pts"   INTEGER     NOT NULL,
    "x"             SMALLINT    NOT NULL,
    "y"             SMALLINT    NOT NULL,
    "map_id"        SMALLINT    NOT NULL,
    "slot"          SMALLINT    NOT NULL,
    FOREIGN KEY (user_id) REFERENCES mana_accounts(id)
        ON DELETE CASCADE
);

CREATE TABLE mana_char_attr
(
    "char_id"       INTEGER     NOT NULL,
    "attr_id"       INTEGER     NOT NULL,
    "attr_base"     FLOAT       NOT NULL,
    "attr_mod"      FLOAT       NOT NULL,
    FOREIGN KEY ("char_id") REFERENCES mana_characters("id")
        ON DELETE CASCADE
);

CREATE TABLE mana_char_skills
(
    "char_id"       INTEGER     NOT NULL,
    "skill_id"      SMALLINT    NOT NULL,
    "skill_exp"     SMALLINT    NOT NULL,
    FOREIGN KEY ("char_id") REFERENCES mana_characters ("id")
        ON DELETE CASCADE
);

CREATE TABLE mana_char_status_effects
(
    "char_id"       INTEGER     NOT NULL,
    "monster_id"    INTEGER     NOT NULL,
    "kills"         INTEGER     NOT NULL,
    FOREIGN KEY ("char_id") REFERENCES mana_characters ("id")
        ON DELETE CASCADE
);

CREATE TABLE mana_char_kill_stats
(
    "char_id"       INTEGER     NOT NULL,
    "monster_id"    INTEGER     NOT NULL,
    "kills"         INTEGER     NOT NULL,
    FOREIGN KEY ("char_id") REFERENCES mana_characters ("id")
        ON DELETE CASCADE
);

CREATE TABLE mana_char_specials
(
    "char_id"               INTEGER     NOT NULL,
    "special_id"            INTEGER     NOT NULL,
    "special_current_mana"  INTEGER     NOT NULL,
    FOREIGN KEY ("char_id") REFERENCES mana_characters ("id")
        ON DELETE CASCADE
);

CREATE TABLE mana_items
(
    "id"            INTEGER     PRIMARY KEY,
    "name"          TEXT        NOT NULL,
    "description"   TEXT        NOT NULL,
    "image"         TEXT        NOT NULL,
    "weigth"        SMALLINT    NOT NULL,
    "itemtype"      TEXT        NOT NULL,
    "effect"        TEXT        NOT NULL,
    "dyestring"     TEXT        NOT NULL
);

CREATE TABLE mana_item_instances
(
    "item_id"       SERIAL      PRIMARY KEY,
    "itemclass_id"  INTEGER     NOT NULL,
    "amount"        INTEGER     NOT NULL
);

CREATE TABLE mana_item_attributes
(
    "attribute_id"  INTEGER     NOT NULL,
    "itemclass_id"  INTEGER     NOT NULL,
    "amount"        INTEGER     NOT NULL,
    FOREIGN KEY ("itemclass_id") REFERENCES mana_item_instances ("item_id")
        ON DELETE CASCADE
);

CREATE TABLE mana_floor_items
(
    "id"            INTEGER     PRIMARY KEY,
    "map_id"        INTEGER     NOT NULL,
    "item_id"       INTEGER     NOT NULL,
    "amount"        SMALLINT    NOT NULL,
    "pos_x"         INTEGER     NOT NULL,
    "pos_y"         INTEGER     NOT NULL
);

CREATE TABLE mana_char_equips
(
    "id"            SERIAL      PRIMARY KEY,
    "owner_id"      INTEGER     NOT NULL,
    "slot_type"     INTEGER     NOT NULL,
    "item_id"       INTEGER     NOT NULL,
    "item_instance" INTEGER     NOT NULL,
    FOREIGN KEY ("owner_id") REFERENCES mana_characters ("id")
        ON DELETE CASCADE
);

CREATE TABLE mana_inventories
(
    "id"            SERIAL      PRIMARY KEY,
    "owner_id"      INTEGER     NOT NULL,
    "slot"          SMALLINT    NOT NULL,
    "class_id"      INTEGER     NOT NULL,
    "amount"        SMALLINT    NOT NULL,
    FOREIGN KEY (owner_id) REFERENCES mana_characters(id)
        ON DELETE CASCADE
);

CREATE TABLE mana_guilds
(
    "id"            SERIAL      PRIMARY KEY,
    "name"          TEXT        NOT NULL UNIQUE
);

CREATE TABLE mana_guild_members
(
    "guild_id"      INTEGER     NOT NULL,
    "member_id"     INTEGER     NOT NULL,
    "rights"        INTEGER     NOT NULL,
    FOREIGN KEY (guild_id)  REFERENCES mana_guilds(id)
        ON DELETE CASCADE,
    FOREIGN KEY (member_id) REFERENCES mana_characters(id)
        ON DELETE CASCADE
);

CREATE TABLE mana_quests
(
    "owner_id"      INTEGER     NOT NULL,
    "name"          TEXT        NOT NULL,
    "value"         TEXT        NOT NULL,
    FOREIGN KEY (owner_id) REFERENCES mana_characters(id)
        ON DELETE CASCADE
);

CREATE TABLE mana_world_states
(
    "state_name"    TEXT        NOT NULL,
    "map_id"        INTEGER     NOT NULL,
    "value"         TEXT        NOT NULL,
    "moddate"       TIMESTAMP   NOT NULL,
    PRIMARY KEY ("state_name", "map_id")
);

CREATE TABLE mana_auctions
(
    "auction_id"    SERIAL      PRIMARY KEY,
    "auction_state" SMALLINT    NOT NULL,
    "char_id"       INTEGER     NOT NULL,
    "itemclass_id"  INTEGER     NOT NULL,
    "amount"        INTEGER     NOT NULL,
    "start_time"    INTEGER     NOT NULL,
    "end_time"      INTEGER     NOT NULL,
    "start_price"   INTEGER     NOT NULL,
    "min_price"     INTEGER     NULL,
    "buyout_price"  INTEGER     NULL,
    "description"   TEXT        NULL,
    FOREIGN KEY ("char_id") REFERENCES mana_characters ("id")
        ON DELETE CASCADE
);

CREATE TABLE "mana_auction_bids"
(
    "bid_id"        SERIAL      PRIMARY KEY,
    "auction_id"    INTEGER     NOT NULL,
    "char_id"       INTEGER     NOT NULL,
    "bid_time"      INTEGER     NOT NULL,
    "bid_price"     INTEGER     NOT NULL,
    FOREIGN KEY ("char_id") REFERENCES mana_characters ("id")
        ON DELETE CASCADE
);

CREATE TABLE mana_post
(
    "letter_id"         SERIAL      PRIMARY KEY,
    "sender_id"         INTEGER     NOT NULL,
    "receiver_id"       INTEGER     NOT NULL,
    "letter_type"       INTEGER     NOT NULL,
    "expiration_date"   INTEGER     NOT NULL,
    "sending_date"      INTEGER     NOT NULL,
    "letter_text"       TEXT        NULL,
    FOREIGN KEY ("sender_id") REFERENCES mana_characters ("id")
        ON DELETE CASCADE,
    FOREIGN KEY ("receiver_id") REFERENCES mana_characters("id")
        ON DELETE CASCADE
);

CREATE TABLE mana_post_attachments
(
    "attachment_id"     SERIAL      PRIMARY KEY,
    "letter_id"         INTEGER     NOT NULL,
    "item_id"           INTEGER     NOT NULL,
    FOREIGN KEY ("letter_id") REFERENCES mana_post ("letter_id")
        ON DELETE CASCADE,
    FOREIGN KEY ("item_id") REFERENCES mana_item_instances ("item_id")
        ON DELETE RESTRICT
);

CREATE TABLE mana_transaction_codes
(
    "id"            SERIAL      PRIMARY KEY,
    "description"   TEXT        NOT NULL,
    "category"      TEXT        NOT NULL
);

CREATE TABLE mana_online_list
(
    "char_id"       INTEGER     PRIMARY KEY,
    "login_date"    INTEGER     NOT NULL,
    FOREIGN KEY ("char_id") REFERENCES mana_characters ("id")
        ON DELETE CASCADE
);

CREATE VIEW mana_v_online_chars
AS
   SELECT l.char_id    as char_id,
          l.login_date as login_date,
          c.user_id    as user_id,
          c.name       as name,
          c.gender     as gender,
          c.level      as level,
          c.map_id     as map_id
     FROM mana_online_list l
     JOIN mana_characters c
       ON l.char_id = c.id;

CREATE TABLE mana_transactions
(
    "id"            SERIAL      PRIMARY KEY,
    "char_id"       INTEGER     NOT NULL,
    "action"        INTEGER     NOT NULL,
    "message"       TEXT        NULL,
    "time"          INTEGER     NOT NULL
);

INSERT INTO mana_world_states VALUES('accountserver_startup',-1,'0', NOW());
INSERT INTO mana_world_states VALUES('accountserver_version',-1,'0', NOW());
INSERT INTO mana_world_states VALUES('database_version',     -1,'21', NOW());

INSERT INTO mana_transaction_codes VALUES (  1, 'Character created',         'Character' );
INSERT INTO mana_transaction_codes VALUES (  2, 'Character selected',        'Character' );
INSERT INTO mana_transaction_codes VALUES (  3, 'Character deleted',         'Character' );
INSERT INTO mana_transaction_codes VALUES (  4, 'Public message sent',       'Chat' );
INSERT INTO mana_transaction_codes VALUES (  5, 'Public message annouced',   'Chat' );
INSERT INTO mana_transaction_codes VALUES (  6, 'Private message sent',      'Chat' );
INSERT INTO mana_transaction_codes VALUES (  7, 'Channel joined',            'Chat' );
INSERT INTO mana_transaction_codes VALUES (  8, 'Channel kicked',            'Chat' );
INSERT INTO mana_transaction_codes VALUES (  9, 'Channel MODE',              'Chat' );
INSERT INTO mana_transaction_codes VALUES ( 10, 'Channel QUIT',              'Chat' );
INSERT INTO mana_transaction_codes VALUES ( 11, 'Channel LIST',              'Chat' );
INSERT INTO mana_transaction_codes VALUES ( 12, 'Channel USERLIST',          'Chat' );
INSERT INTO mana_transaction_codes VALUES ( 13, 'Channel TOPIC',             'Chat' );
INSERT INTO mana_transaction_codes VALUES ( 14, 'Command BAN',               'Commands' );
INSERT INTO mana_transaction_codes VALUES ( 15, 'Command DROP',              'Commands' );
INSERT INTO mana_transaction_codes VALUES ( 16, 'Command ITEM',              'Commands' );
INSERT INTO mana_transaction_codes VALUES ( 17, 'Command MONEY',             'Commands' );
INSERT INTO mana_transaction_codes VALUES ( 18, 'Command SETGROUP',          'Commands' );
INSERT INTO mana_transaction_codes VALUES ( 19, 'Command SPAWN',             'Commands' );
INSERT INTO mana_transaction_codes VALUES ( 20, 'Command WARP',              'Commands' );
INSERT INTO mana_transaction_codes VALUES ( 21, 'Item picked up',            'Actions' );
INSERT INTO mana_transaction_codes VALUES ( 22, 'Item used',                 'Actions' );
INSERT INTO mana_transaction_codes VALUES ( 23, 'Item dropped',              'Actions' );
INSERT INTO mana_transaction_codes VALUES ( 24, 'Item moved',                'Actions' );
INSERT INTO mana_transaction_codes VALUES ( 25, 'Target attacked',           'Actions' );
INSERT INTO mana_transaction_codes VALUES ( 26, 'ACTION Changed',            'Actions' );
INSERT INTO mana_transaction_codes VALUES ( 27, 'Trade requested',           'Actions' );
INSERT INTO mana_transaction_codes VALUES ( 28, 'Trade ended',               'Actions' );
INSERT INTO mana_transaction_codes VALUES ( 29, 'Trade money',               'Actions' );
INSERT INTO mana_transaction_codes VALUES ( 30, 'Trade items',               'Actions' );
INSERT INTO mana_transaction_codes VALUES ( 31, 'Attribute increased',       'Character' );
INSERT INTO mana_transaction_codes VALUES ( 32, 'Attribute decreased',       'Character' );
INSERT INTO mana_transaction_codes VALUES ( 33, 'Command MUTE',              'Commands' );
INSERT INTO mana_transaction_codes VALUES ( 34, 'Command EXP',               'Commands' );
INSERT INTO mana_transaction_codes VALUES ( 35, 'Command INVISIBLE',         'Commands' );
INSERT INTO mana_transaction_codes VALUES ( 36, 'Command COMBAT',            'Commands' );
INSERT INTO mana_transaction_codes VALUES ( 37, 'Command ANNOUNCE',          'Commands' );
INSERT INTO mana_transaction_codes VALUES ( 38, 'Command ANNOUNCE_LOCAL',    'Commands' );
INSERT INTO mana_transaction_codes VALUES ( 39, 'Command KILL',              'Commands' );
INSERT INTO mana_transaction_codes VALUES ( 40, 'Command FX',                'Commands' );
INSERT INTO mana_transaction_codes VALUES ( 41, 'Command LOG',               'Commands' );
INSERT INTO mana_transaction_codes VALUES ( 42, 'Command KILLMONSTER',       'Commands' );
INSERT INTO mana_transaction_codes VALUES ( 43, 'Command GOTO',              'Commands' );
INSERT INTO mana_transaction_codes VALUES ( 44, 'Command GONEXT',            'Commands' );
INSERT INTO mana_transaction_codes VALUES ( 45, 'Command GOPREV',            'Commands' );
INSERT INTO mana_transaction_codes VALUES ( 46, 'Command IPBAN',             'Commands' );
INSERT INTO mana_transaction_codes VALUES ( 47, 'Command WIPE_ITEMS',        'Commands' );
INSERT INTO mana_transaction_codes VALUES ( 48, 'Command WIPE_LEVEL',        'Commands' );
INSERT INTO mana_transaction_codes VALUES ( 49, 'Command SHUTDOWN_THIS',     'Commands' );
INSERT INTO mana_transaction_codes VALUES ( 50, 'Command SHUTDOWN_ALL',      'Commands' );
INSERT INTO mana_transaction_codes VALUES ( 51, 'Command RESTART_THIS',      'Commands' );
INSERT INTO mana_transaction_codes VALUES ( 52, 'Command RESTART_ALL',       'Commands' );
INSERT INTO mana_transaction_codes VALUES ( 53, 'Command ATTRIBUTE',         'Commands' );
INSERT INTO mana_transaction_codes VALUES ( 54, 'Command KICK',              'Commands' );
