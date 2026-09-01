package client.command.commands.gm0;

import client.Character;
import client.Client;
import client.command.Command;
import net.server.Server;
import tools.PacketCreator;

public class CashStorageCommand extends Command {
    {
        setDescription("Abre el baul original del Cash Shop para transferir items entre personajes de la cuenta.");
    }

    @Override
    public void execute(Client c, String[] params) {
        try {
            Character mc = c.getPlayer();

            if (mc.cannotEnterCashShop() || mc.getEventInstance() != null || server.maps.MiniDungeonInfo.isDungeonMap(mc.getMapId())) {
                mc.dropMessage(5, "No puedes usar el baul del Cash Shop en este momento.");
                return;
            }

            if (mc.getCashShop().isOpened()) {
                return;
            }

            mc.closePlayerInteractions();
            mc.closePartySearchInteractions();

            mc.unregisterChairBuff();
            Server.getInstance().getPlayerBuffStorage().addBuffsToStorage(mc.getId(), mc.getAllBuffs());
            Server.getInstance().getPlayerBuffStorage().addDiseasesToStorage(mc.getId(), mc.getAllDiseases());
            mc.setAwayFromChannelWorld();
            mc.notifyMapTransferToPartner(-1);
            mc.removeIncomingInvites();
            mc.cancelAllBuffs(true);
            mc.cancelAllDebuffs();
            mc.cancelBuffExpireTask();
            mc.cancelDiseaseExpireTask();
            mc.cancelSkillCooldownTask();
            mc.cancelExpirationTask();

            mc.forfeitExpirableQuests();
            mc.cancelQuestExpirationTask();

            c.sendPacket(PacketCreator.openCashShop(c, false));
            c.sendPacket(PacketCreator.showCashInventory(c));
            c.sendPacket(PacketCreator.showGifts(mc.getCashShop().loadGifts()));
            c.sendPacket(PacketCreator.showWishList(mc, false));
            c.sendPacket(PacketCreator.showCash(mc));

            c.getChannelServer().removePlayer(mc);
            mc.getMap().removePlayer(mc);
            mc.getCashShop().open(true);
            mc.saveCharToDB();
        } catch (Exception e) {
            e.printStackTrace();
            c.getPlayer().dropMessage(5, "Ocurrio un error al intentar entrar al baul del Cash Shop.");
        }
    }
}
