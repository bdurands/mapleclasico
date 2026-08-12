package net.server.channel.handlers;

import client.Client;
import net.AbstractPacketHandler;
import net.packet.InPacket;
import server.maps.HyperTeleportRockService;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public final class HyperTeleportRockHandler extends AbstractPacketHandler {
    private static final Logger log = LoggerFactory.getLogger(HyperTeleportRockHandler.class);

    @Override
    public void handlePacket(InPacket p, Client c) {
        log.info("Received HYPER_TELEPORT_ROCK packet! targetMapId bytes available: " + p.available());
        if (p.available() < Integer.BYTES) {
            HyperTeleportRockService.enableActions(c);
            return;
        }

        int targetMapId = p.readInt();
        c.getPlayer().setHyperTeleportRockTargetMapId(targetMapId);
        scripting.npc.NPCScriptManager.getInstance().start(c, 9010000, "hyperteleportrock", c.getPlayer());
    }
}
