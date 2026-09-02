/*
	This file is part of the OdinMS Maple Story Server
    Copyright (C) 2008 Patrick Huy <patrick.huy@frz.cc>
		       Matthias Butz <matze@odinms.de>
		       Jan Christian Meyer <vimes@odinms.de>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation version 3 as published by
    the Free Software Foundation. You may not use, modify or distribute
    this program under any other version of the GNU Affero General Public
    License.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
package net.server.channel.handlers;

import client.Character;
import client.Client;
import net.AbstractPacketHandler;
import net.packet.InPacket;
import net.server.Server;
import server.maps.MiniDungeonInfo;
import tools.PacketCreator;


public final class EnterMTSHandler extends AbstractPacketHandler {

    @Override
    public void handlePacket(InPacket p, Client c) {
        try {
            Character chr = c.getPlayer();

            if (chr.cannotEnterCashShop() || chr.getEventInstance() != null || MiniDungeonInfo.isDungeonMap(chr.getMapId())) {
                chr.dropMessage(5, "No puedes entrar al baul del Cash Shop en este momento.");
                c.sendPacket(PacketCreator.enableActions());
                return;
            }

            if (chr.getCashShop().isOpened()) {
                return;
            }

            chr.closePlayerInteractions();
            chr.closePartySearchInteractions();

            chr.unregisterChairBuff();
            Server.getInstance().getPlayerBuffStorage().addBuffsToStorage(chr.getId(), chr.getAllBuffs());
            Server.getInstance().getPlayerBuffStorage().addDiseasesToStorage(chr.getId(), chr.getAllDiseases());
            chr.setAwayFromChannelWorld();
            chr.notifyMapTransferToPartner(-1);
            chr.removeIncomingInvites();
            chr.cancelAllBuffs(true);
            chr.cancelAllDebuffs();
            chr.cancelBuffExpireTask();
            chr.cancelDiseaseExpireTask();
            chr.cancelSkillCooldownTask();
            chr.cancelExpirationTask();

            chr.forfeitExpirableQuests();
            chr.cancelQuestExpirationTask();

            c.sendPacket(PacketCreator.openCashShop(c, false));
            c.sendPacket(PacketCreator.showCashInventory(c));
            c.sendPacket(PacketCreator.showGifts(chr.getCashShop().loadGifts()));
            c.sendPacket(PacketCreator.showWishList(chr, false));
            c.sendPacket(PacketCreator.showCash(chr));

            c.getChannelServer().removePlayer(chr);
            chr.getMap().removePlayer(chr);
            chr.getCashShop().open(true);
            chr.saveCharToDB();
        } catch (Exception e) {
            e.printStackTrace();
            c.getPlayer().dropMessage(5, "Ocurrio un error al intentar entrar al baul del Cash Shop.");
        }
    }
}
