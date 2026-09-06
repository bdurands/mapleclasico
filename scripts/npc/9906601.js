var status = -1;
var questId = 9999901;
var rewardItem = 1112940; // The item ID requested
var reqItem = 4001126; // Maple Leaf
var reqAmount = 200;

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

    // Limit date to Sept 25, 2026 23:59:59 (Month is 0-indexed in JS, 8 = Sept)
    var limitDate = new Date(2026, 8, 25, 23, 59, 59).getTime(); 
    var currentDate = new Date().getTime();

    if (currentDate >= limitDate) {
        cm.sendOk("El evento ha finalizado. ¡Gracias por participar, insecto!");
        cm.dispose();
        return;
    }

    if (cm.getPlayer().getLevel() < 30) {
        cm.sendOk("Necesitas ser al menos nivel 30 para hablar conmigo, insecto.");
        cm.dispose();
        return;
    }

    var record = cm.getQuestRecord(questId);
    var isCompleted = (record != null && record.getStatus() == 2);

    if (!isCompleted) {
        if (status == 0) {
            cm.sendNext("Soy el príncipe de los Saiyajin. He preparado un artefacto especial para aquellos que demuestren su valía.\r\nTráeme #b200 #t" + reqItem + "##k y te entregaré algo increíble, insecto.");
        } else if (status == 1) {
            if (cm.haveItem(reqItem, reqAmount)) {
                if (cm.canHold(rewardItem)) {
                    cm.gainItem(reqItem, -reqAmount);
                    
                    var ii = Packages.server.ItemInformationProvider.getInstance();
                    var equip = ii.getEquipById(rewardItem);
                    
                    var jobId = cm.getPlayer().getJob().getId();
                    var jobBase = Math.floor(jobId / 100);
                    
                    // Caso 1 y Caso 4 (Warriors 1xx, Bowmen 3xx, Pirates 5xx)
                    if (jobBase == 1 || jobBase == 3 || jobBase == 5) {
                        equip.setStr(10);
                        equip.setDex(10);
                        equip.setWatk(2);
                        equip.setWdef(10);
                        equip.setMdef(5);
                    } 
                    // Caso 2 (Magicians 2xx)
                    else if (jobBase == 2) {
                        equip.setInt(10);
                        equip.setMatk(2);
                        equip.setWdef(5);
                        equip.setMdef(10);
                    } 
                    // Caso 3 (Thieves 4xx)
                    else if (jobBase == 4) {
                        equip.setLuk(10);
                        equip.setWatk(2);
                        equip.setWdef(5);
                        equip.setMdef(10);
                    } else { 
                        // Principiantes u otros
                        equip.setStr(5);
                        equip.setDex(5);
                        equip.setInt(5);
                        equip.setLuk(5);
                        equip.setWatk(1);
                        equip.setMatk(1);
                        equip.setWdef(5);
                        equip.setMdef(5);
                    }
                    
                    equip.setLevel(0); // Nivel inicial de mejora
                    
                    Packages.client.inventory.manipulator.InventoryManipulator.addFromDrop(cm.getC(), equip, true);
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
        }
    } else {
        // Fase de mejoras (Upgrades)
        if (status == 0) {
            cm.sendSimple("Veo que ya tienes el artefacto. Puedo aumentar su poder si me entregas Puntos de Party Quest (PQ Points).\r\nTus PQ Points: #b" + cm.getPqPoints() + "#k\r\n\r\n#L0##bMejorar artefacto#l");
        } else if (status == 1) {
            if (selection == 0) {
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
                
                // Descontar el ítem original
                Packages.client.inventory.manipulator.InventoryManipulator.removeFromSlot(cm.getC(), Packages.client.inventory.InventoryType.EQUIP, targetSlot, 1, false);
                
                // Descontar los PQ points
                cm.gainPqPoints(-cost);
                
                var jobId = cm.getPlayer().getJob().getId();
                var jobBase = Math.floor(jobId / 100);
                
                // Crear copia del ítem para modificar sus stats
                var newEquip = targetEquip.copy();
                
                if (currentLevel == 0) { // Mejora 1
                    if (jobBase == 2) newEquip.setMatk(newEquip.getMatk() + 1);
                    else newEquip.setWatk(newEquip.getWatk() + 1);

                    if (jobBase == 4) { // Thieves
                        newEquip.setWdef(newEquip.getWdef() + 2);
                        newEquip.setMdef(newEquip.getMdef() + 3);
                    } else { // Rest
                        newEquip.setWdef(newEquip.getWdef() + 3);
                        newEquip.setMdef(newEquip.getMdef() + 2);
                    }
                    newEquip.setAcc(newEquip.getAcc() + 2);
                } 
                else if (currentLevel == 1) { // Mejora 2
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
                else if (currentLevel == 2) { // Mejora 3
                    if (jobBase == 2) newEquip.setMatk(newEquip.getMatk() + 2);
                    else newEquip.setWatk(newEquip.getWatk() + 2);

                    newEquip.setWdef(newEquip.getWdef() + 1);
                    newEquip.setMdef(newEquip.getMdef() + 1);
                    newEquip.setAcc(newEquip.getAcc() + 2);
                    newEquip.setSpeed(newEquip.getSpeed() + 5);
                    newEquip.setJump(newEquip.getJump() + 5);
                }
                
                // Actualizar nivel interno de mejora
                newEquip.setLevel(currentLevel + 1);
                
                // Entregar nuevo ítem
                Packages.client.inventory.manipulator.InventoryManipulator.addFromDrop(cm.getC(), newEquip, true);
                
                cm.sendOk("¡El artefacto ha incrementado su poder! (Nivel " + (currentLevel + 1) + "/3)");
                cm.dispose();
            }
        }
    }
}
