var status = 0;
var ellinBox = 2022644; // ID del Ellin Box
var expReward = 58000;

var mob59 = 59;
var mob57 = 57;

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

    var q1 = cm.getQuestRecord(99907061);
    var q2 = cm.getQuestRecord(99907062);

    if (status == 0) {
        var cd1 = q1.getCustomData();
        var cd2 = q2.getCustomData();

        if (cd1 == null) {
            cm.sendNext("Hola aventurero. Necesito tu ayuda para limpiar este mapa.\n\nPor favor, ve y mata a 999 del Mob 59.");
            status = 10;
        } else if (cd1 != "done") {
            // Analizar progreso
            var parts = cd1.split(":");
            if (parts.length >= 3) {
                var current = parseInt(parts[1]);
                var target = parseInt(parts[2]);
                if (current >= target) {
                    cm.sendNext("¡Increíble! Has matado a los 999 monstruos (Mob 59). Aquí tienes tu recompensa.");
                    status = 11;
                } else {
                    cm.sendOk("Aún no has terminado tu tarea. Has eliminado a " + current + " / " + target + " monstruos.");
                    cm.dispose();
                }
            }
        } else if (cd2 == null) {
            cm.sendNext("Ya has completado mi primera tarea. Ahora, necesito que elimines a 999 del Mob 57.");
            status = 20;
        } else if (cd2 != "done") {
            var parts = cd2.split(":");
            if (parts.length >= 3) {
                var current = parseInt(parts[1]);
                var target = parseInt(parts[2]);
                if (current >= target) {
                    cm.sendNext("¡Excelente trabajo! Has eliminado a los 999 monstruos (Mob 57). Aquí tienes tu segunda recompensa.");
                    status = 21;
                } else {
                    cm.sendOk("Aún no has terminado tu segunda tarea. Has eliminado a " + current + " / " + target + " monstruos.");
                    cm.dispose();
                }
            }
        } else {
            cm.sendOk("Ya has completado todas mis misiones. ¡Gracias por tu ayuda!");
            cm.dispose();
        }
    } 
    else if (status == 11) {
        q1.setCustomData(mob59 + ":0:999");
        cm.sendOk("He iniciado el rastreo para el Mob 59. ¡A cazar!");
        cm.dispose();
    }
    else if (status == 12) {
        if (cm.canHold(ellinBox)) {
            cm.gainItem(ellinBox, 1);
            cm.gainExp(expReward);
            q1.setCustomData("done");
            cm.sendOk("Has recibido un Ellin Box y 58,000 de EXP. ¡Habla conmigo otra vez para tu siguiente misión!");
        } else {
            cm.sendOk("Por favor, asegúrate de tener espacio en tu inventario de Consumibles (USE).");
        }
        cm.dispose();
    }
    else if (status == 21) {
        q2.setCustomData(mob57 + ":0:999");
        cm.sendOk("He iniciado el rastreo para el Mob 57. ¡Mucha suerte!");
        cm.dispose();
    }
    else if (status == 22) {
        if (cm.canHold(ellinBox)) {
            cm.gainItem(ellinBox, 1);
            cm.gainExp(expReward);
            q2.setCustomData("done");
            cm.sendOk("Has completado todas las misiones. Disfruta tu Ellin Box y 58,000 EXP.");
        } else {
            cm.sendOk("Por favor, asegúrate de tener espacio en tu inventario de Consumibles (USE).");
        }
        cm.dispose();
    }
}
