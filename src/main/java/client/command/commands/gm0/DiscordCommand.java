package client.command.commands.gm0;

import client.Client;
import client.command.Command;
import tools.PacketCreator;

public class DiscordCommand extends Command {

    public DiscordCommand() {
        setDescription("Abre la ventana de Discord.");
    }

    @Override
    public void execute(Client c, String[] args) {
        c.sendPacket(PacketCreator.openDiscordUI());
    }
}
