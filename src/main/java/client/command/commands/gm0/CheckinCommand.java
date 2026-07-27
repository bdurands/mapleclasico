package client.command.commands.gm0;

import client.Character;
import client.Client;
import client.command.Command;
import tools.PacketCreator;

/**
 * @checkin / @daily — opens the Daily Check-In window. refreshCheckin() is idempotent (keyed by the
 * 24h timestamp), so running this any time is safe; sending the snapshot opens the window even when
 * there's nothing to claim yet (and reports the remaining cooldown).
 */
public class CheckinCommand extends Command {
    {
        setDescription("Open the Daily Check-In window.");
    }

    @Override
    public void execute(Client c, String[] params) {
        Character player = c.getPlayer();
        if (player.getLevel() < server.DailyCheckinRewards.MIN_LEVEL) {
            player.dropMessage(6, "Daily Check-In unlocks at Level " + server.DailyCheckinRewards.MIN_LEVEL + ".");
            return;
        }
        int claimable = player.refreshCheckin();
        int viewDay = claimable >= 1 ? claimable : player.getCheckinDay();
        c.sendPacket(PacketCreator.dailyCheckinSnapshot(viewDay, player.getCheckinClaimed(), 0));
        if (claimable < 1) {
            long secs = player.getCheckinCooldownSeconds();
            long h = secs / 3600, m = (secs % 3600) / 60;
            player.dropMessage(6, "Daily Check-In: your next reward unlocks in " + h + "h " + m + "m.");
        }
    }
}
