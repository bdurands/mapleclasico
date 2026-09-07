var status = 0;
var reward1 = 1112194;
var reward2 = 1115007;
var questId = 99903000; // ID de QuestRecord inventado para guardar si ya lo reclamó

function start() {
    status = -1;
    action(1, 0, 0);
}

function action(mode, type, selection) {
    if (mode == -1 || mode == 0) {
        cm.dispose();
        return;
    }
    status++;

    // Revisar si el jugador ya reclamó el premio antes usando su QuestRecord
    var questRecord = cm.getQuestRecord(questId);
    var hasClaimed = (questRecord.getCustomData() == "done");

    if (status == 0) {
        if (hasClaimed) {
            cm.sendOk("Ya has reclamado tu recompensa especial. ¡Disfruta de tus nuevos anillos y sigue explorando el mundo de MapleStory!");
            cm.dispose();
        } else {
            cm.sendNext("¡Felicidades por llegar hasta aquí aventurero! Como recompensa por tu gran esfuerzo, te haré entrega de unos anillos muy especiales.\r\n\r\n#v" + reward1 + "# y #v" + reward2 + "#");
        }
    } else if (status == 1) {
        // Verificar espacio en inventario EQUIP
        if (cm.canHold(reward1) && cm.canHold(reward2)) {
            cm.gainItem(reward1, 1);
            cm.gainItem(reward2, 1);
            
            // Marcar la recompensa como reclamada para este personaje (de por vida)
            questRecord.setCustomData("done");
            
            cm.sendOk("¡Aquí tienes! Has recibido tus anillos especiales. Úsalos con sabiduría.");
        } else {
            cm.sendOk("Por favor, asegúrate de tener al menos **2 espacios libres** en tu inventario de Equipamiento (EQUIP).");
        }
        cm.dispose();
    }
}
