/*
    This file is part of the HeavenMS MapleStory Server, commands OdinMS-based
    Copyleft (L) 2016 - 2019 RonanLana

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

/*
   @Author: Arthur L - Refactored command content into modules
*/
package client.command.commands.gm1;

import client.Character;
import client.Client;
import client.command.Command;
import scripting.npc.NPCScriptManager;

public class WhatDropsFromCommand extends Command {
    public static final int DROP_SEARCH_NPC = 2040052;

    {
        setDescription("Show what items drop from a mob.");
    }

    @Override
    public void execute(Client c, String[] params) {
        Character player = c.getPlayer();
        String query = player.getLastCommandMessage();
        if (params.length < 1 || query == null || query.trim().isEmpty()) {
            player.dropMessage(5, "Please do @whatdropsfrom <monster name>");
            return;
        }

        c.removeClickedNPC();
        NPCScriptManager.getInstance().dispose(c);
        NPCScriptManager.getInstance().start(c, DROP_SEARCH_NPC, "whatdropsfrom", player);
    }
}
