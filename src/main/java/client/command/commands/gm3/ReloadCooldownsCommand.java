package client.command.commands.gm3;

import client.Client;
import client.command.Command;
import server.SkillCooldownOverrides;

public class ReloadCooldownsCommand extends Command {
    {
        setDescription("Reload custom skill cooldowns from skill-cooldowns.properties.");
    }

    @Override
    public void execute(Client c, String[] params) {
        SkillCooldownOverrides.reload();
        c.getPlayer().dropMessage(5, "Skill cooldown overrides reloaded.");
    }
}
