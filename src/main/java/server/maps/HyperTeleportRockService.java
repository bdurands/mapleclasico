package server.maps;

import client.Character;
import client.Client;
import constants.id.ItemId;
import constants.id.MapId;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import scripting.event.EventInstanceManager;
import tools.PacketCreator;

public final class HyperTeleportRockService {
    private static final Logger log = LoggerFactory.getLogger(HyperTeleportRockService.class);

    private HyperTeleportRockService() {
    }

    public static boolean tryTeleport(Client c, int targetMapId, boolean requireItem) {
        Character player = c.getPlayer();
        MapleMap targetMap = getTargetMap(c, targetMapId);

        if (!canTeleport(c, player, targetMap, requireItem)) {
            return false;
        }

        String mapName = targetMap.getMapName();
        if (mapName != null && !mapName.isEmpty()) {
            player.dropMessage(5, "The Hyper Teleport Rock warps you to " + mapName + ".");
        } else {
            player.dropMessage(5, "The Hyper Teleport Rock warps you to the selected map.");
        }
        player.forceChangeMap(targetMap, targetMap.getRandomPlayerSpawnpoint());
        return true;
    }

    private static MapleMap getTargetMap(Client c, int targetMapId) {
        try {
            return c.getChannelServer().getMapFactory().getMap(targetMapId);
        } catch (RuntimeException e) {
            log.warn("Rejected Hyper Teleport Rock request to invalid map {} by {}", targetMapId,
                    c.getPlayer() != null ? c.getPlayer().getName() : "unknown", e);
            return null;
        }
    }

    private static boolean canTeleport(Client c, Character player, MapleMap targetMap, boolean requireItem) {
        if (player == null || !c.isLoggedIn() || !player.isLoggedinWorld()) {
            return false;
        }

        if (player.isChangingMaps() || player.isBanned()) {
            player.dropMessage(1, "You cannot use Hyper Teleport Rocks right now.");
            return false;
        }

        if (!player.isAlive()) {
            player.dropMessage(1, "You cannot use Hyper Teleport Rocks while dead.");
            return false;
        }

        if (player.getCashShop().isOpened() || player.getTrade() != null || player.getShop() != null
                || player.getPlayerShop() != null || player.getMiniGame() != null || player.getHiredMerchant() != null) {
            player.dropMessage(1, "You cannot use Hyper Teleport Rocks right now.");
            return false;
        }

        EventInstanceManager eim = player.getEventInstance();
        if (eim != null) {
            player.dropMessage(1, "Hyper Teleport Rocks cannot be used inside event instances.");
            return false;
        }

        if (requireItem && !player.haveItem(ItemId.HYPER_TELEPORT_ROCK)) {
            player.dropMessage(1, "You need a Hyper Teleport Rock to use this.");
            return false;
        }

        if (MapId.isTimeTemple(player.getMapId())) {
            player.dropMessage(1, "You cannot use Hyper Teleport Rocks here.");
            return false;
        }

        if (MapId.isBossExpeditionMap(player.getMapId())) {
            player.dropMessage(1, "Hyper Teleport Rocks cannot be used from boss expedition maps.");
            return false;
        }

        if (targetMap == null) {
            player.dropMessage(1, "You cannot teleport to this map.");
            return false;
        }

        int targetMapId = targetMap.getId();

        if (MapId.isBossExpeditionMap(targetMapId)) {
            player.dropMessage(1, "Hyper Teleport Rocks cannot be used to enter boss expedition maps.");
            return false;
        }

        if (MapId.isTimeTemple(targetMapId)) {
            player.dropMessage(1, "You cannot teleport to this map.");
            return false;
        }

        if (FieldLimit.CANNOTVIPROCK.check(targetMap.getFieldLimit())
                || (targetMap.getForcedReturnId() != MapId.NONE && !MapId.isMapleIsland(targetMapId))) {
            player.dropMessage(1, "You cannot teleport to this map.");
            return false;
        }

        return true;
    }

    public static void enableActions(Client c) {
        c.sendPacket(PacketCreator.enableActions());
    }
}
