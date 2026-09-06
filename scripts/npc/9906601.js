var status = 0;
var questId = 32100; // Must fit in Java short (max 32767)
var rewardItem = 1112940; // The item ID requested
var reqItem = 4001126; // Maple Leaf
var reqAmount = 200;

var voteIcon = "#v4000313#"; // Hoja de Arce Dorada
var pqIcon = "#v4001158#";   // Lazo de Confianza (PQ)
var bossIcon = "#v4001086#"; // Certificado de Zakum
var donorIcon = "#v5200002#"; // Tarjeta de 10k NX
var nxIcon = "#v5200001#";   // Tarjeta de 5k NX

function start() {
    status = -1;
    action(1, 0, 0);
}

function action(mode, type, selection) {
    if (mode == -1 || mode == 0) {
        cm.dispose();
        return;
    }

    if (mode == 1) {
        status++;
    }

    if (status == 0) {
        var votePoints = cm.getClient().getVotePoints();
        var pqPoints = cm.getPlayer().getPqPoints();
        var bossPoints = cm.getPlayer().getBossPoints();
        var donorPoints = cm.getPlayer().getCashShop().getCash(2);

        var text = "\t\t\t\t#d#e ¡Centro de Puntos EllinMS! #n#k\r\n\r\n";
        text += "¡Hola, #b#h ##k! Aquí tienes el resumen detallado de tus puntos acumulados. ¡Sigue así!\r\n\r\n";

        text += " " + voteIcon + " #e#bVotación:#n#k \t\t" + votePoints + " Puntos\r\n";
        text += " " + pqIcon + " #e#dParty Quest:#n#k \t" + pqPoints + " Puntos\r\n";
        text += " " + bossIcon + " #e#rBoss Points:#n#k \t" + bossPoints + " Puntos\r\n";
        text += " " + donorIcon + " #e#gDonador:#n#k \t\t" + donorPoints + " Puntos\r\n\r\n";

        text += "#b------------------------------------------------------#k\r\n";
        text += "#L0# " + nxIcon + " #eCanjear#n 10 Puntos de Votación por #b4,000 NX Credit#k#l\r\n";

        // Agregar la opción del evento si cumple fecha y nivel
        var limitDate = new Date(2026, 8, 25, 23, 59, 59).getTime();
        var currentDate = new Date().getTime();

        if (currentDate < limitDate && cm.getPlayer().getLevel() >= 30) {
            var record = cm.getQuestRecord(questId);
            var isCompleted = (record != null && (record.getStatus() == 2 || (record.getCustomData() != null && ("" + record.getCustomData()) == "done")));
            if (!isCompleted) {
                text += "#L1# #r[Evento]#k Canjear 200 Maple Leafs por el Artefacto Saiyajin#l\r\n";
            } else {
                text += "#L2# #r[Evento]#k Mejorar tu Artefacto Saiyajin con Puntos PQ#l\r\n";
            }
        }

        cm.sendSimple(text);
    } else if (status == 1) {
        if (selection == 0) {
            var votePoints = cm.getClient().getVotePoints();
            if (votePoints >= 10) {
                cm.getClient().useVotePoints(10);
                cm.getPlayer().getCashShop().gainCash(1, 4000); // 1 = NX Credit
                cm.sendOk(nxIcon + " ¡Felicidades! Has canjeado #r10 Puntos de Votación#k por #b4,000 NX Credit#k.\r\n\r\n¡Disfrútalos en el Cash Shop!");
                cm.dispose();
            } else {
                cm.sendOk(voteIcon + " Lo siento, no tienes suficientes #b10 Puntos de Votación#k.\r\n\r\nActualmente tienes: #r" + votePoints + "#k.");
                cm.dispose();
            }
        } else if (selection == 1) {
            if (cm.haveItem(reqItem, reqAmount)) {
                if (cm.canHold(rewardItem)) {
                    cm.gainItem(reqItem, -reqAmount);

                    var ii = Packages.server.ItemInformationProvider.getInstance();
                    var equip = ii.getEquipById(rewardItem);

                    var jobId = cm.getPlayer().getJob().getId();
                    var jobBase = Math.floor(jobId / 100);

                    if (jobBase == 1 || jobBase == 3 || jobBase == 5) {
                        equip.setStr(10);
                        equip.setDex(10);
                        equip.setWatk(2);
                        equip.setWdef(10);
                        equip.setMdef(5);
                    } else if (jobBase == 2) {
                        equip.setInt(10);
                        equip.setMatk(2);
                        equip.setWdef(5);
                        equip.setMdef(10);
                    } else if (jobBase == 4) {
                        equip.setLuk(10);
                        equip.setWatk(2);
                        equip.setWdef(5);
                        equip.setMdef(10);
                    } else {
                        equip.setStr(5); equip.setDex(5); equip.setInt(5); equip.setLuk(5);
                        equip.setWatk(1); equip.setMatk(1); equip.setWdef(5); equip.setMdef(5);
                    }

                    equip.setLevel(0);
                    Packages.client.inventory.manipulator.InventoryManipulator.addFromDrop(cm.getClient(), equip, true);
                    cm.forceStartQuest(questId);
                    var r = cm.getQuestRecord(questId);
                    if (r != null) r.setCustomData("done");
                    cm.forceCompleteQuest(questId);

                    cm.sendOk("Aquí tienes. Úsalo bien y no me avergüences.");
                    cm.dispose();
                } else {
                    cm.sendOk("Asegúrate de tener espacio en tu inventario de equipamiento.");
                    cm.dispose();
                }
            } else {
                cm.sendOk("Aún no tienes los #b200 #t" + reqItem + "##k. ¡No me hagas perder el tiempo!");
                cm.dispose();
            }
        } else if (selection == 2) {
            var inv = cm.getPlayer().getInventory(Packages.client.inventory.InventoryType.EQUIP);
            var found = false;
            var targetEquip = null;
            var targetSlot = 0;

            var iter = inv.list().iterator();
            while (iter.hasNext()) {
                var it = iter.next();
                if (it.getItemId() == rewardItem) {
                    targetEquip = it;
                    targetSlot = it.getPosition();
                    found = true;
                    break;
                }
            }

            if (!found) {
                cm.sendOk("No encuentro el artefacto en tu inventario de Equipos. Asegúrate de tenerlo allí y NO tenerlo equipado. Si lo tienes puesto, quítatelo primero.");
                cm.dispose();
                return;
            }

            var currentLevel = targetEquip.getLevel();
            if (currentLevel >= 3) {
                cm.sendOk("El artefacto ya ha alcanzado su máximo poder, ya no hay nada más que hacer.");
                cm.dispose();
                return;
            }

            var cost = 0;
            if (currentLevel == 0) cost = 5;
            else if (currentLevel == 1) cost = 10;
            else if (currentLevel == 2) cost = 10;

            if (cm.getPqPoints() < cost) {
                cm.sendOk("Necesitas #r" + cost + " PQ Points#k para esta mejora. Solo tienes " + cm.getPqPoints() + ".");
                cm.dispose();
                return;
            }

            Packages.client.inventory.manipulator.InventoryManipulator.removeFromSlot(cm.getClient(), Packages.client.inventory.InventoryType.EQUIP, targetSlot, 1, false);
            cm.gainPqPoints(-cost);

            var jobId = cm.getPlayer().getJob().getId();
            var jobBase = Math.floor(jobId / 100);
            var newEquip = targetEquip.copy();

            if (currentLevel == 0) {
                if (jobBase == 2) newEquip.setMatk(newEquip.getMatk() + 1);
                else newEquip.setWatk(newEquip.getWatk() + 1);

                if (jobBase == 4) {
                    newEquip.setWdef(newEquip.getWdef() + 2);
                    newEquip.setMdef(newEquip.getMdef() + 3);
                } else {
                    newEquip.setWdef(newEquip.getWdef() + 3);
                    newEquip.setMdef(newEquip.getMdef() + 2);
                }
                newEquip.setAcc(newEquip.getAcc() + 2);
            }
            else if (currentLevel == 1) {
                if (jobBase == 2) newEquip.setMatk(newEquip.getMatk() + 2);
                else newEquip.setWatk(newEquip.getWatk() + 2);

                if (jobBase == 4) {
                    newEquip.setWdef(newEquip.getWdef() + 2);
                    newEquip.setMdef(newEquip.getMdef() + 3);
                } else {
                    newEquip.setWdef(newEquip.getWdef() + 3);
                    newEquip.setMdef(newEquip.getMdef() + 2);
                }
                newEquip.setAcc(newEquip.getAcc() + 2);
                newEquip.setSpeed(newEquip.getSpeed() + 5);
            }
            else if (currentLevel == 2) {
                if (jobBase == 2) newEquip.setMatk(newEquip.getMatk() + 2);
                else newEquip.setWatk(newEquip.getWatk() + 2);

                newEquip.setWdef(newEquip.getWdef() + 1);
                newEquip.setMdef(newEquip.getMdef() + 1);
                newEquip.setAcc(newEquip.getAcc() + 2);
                newEquip.setSpeed(newEquip.getSpeed() + 5);
                newEquip.setJump(newEquip.getJump() + 5);
            }

            newEquip.setLevel(currentLevel + 1);
            Packages.client.inventory.manipulator.InventoryManipulator.addFromDrop(cm.getClient(), newEquip, true);

            cm.sendOk("¡El artefacto ha incrementado su poder! (Nivel " + (currentLevel + 1) + "/3)");
            cm.dispose();
        }
    }
}
