package client.command.commands.gm0;

import client.Client;
import client.command.Command;
import tools.PacketCreator;

public class DiscordCommand extends Command {
    
    public DiscordCommand() {
        super("discord", "Opens the Discord UI.", "", null);
    }
    
    @Override
    public void execute(Client c, String[] args) {
        c.sendPacket(PacketCreator.openDiscordUI());
    }
}
