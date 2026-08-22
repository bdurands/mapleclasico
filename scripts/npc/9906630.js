/*
 * NPC: 9906630
 * Propósito: NPC de cambio de Cabello (Hair) por Mesos.
 */
var status = 0;
var price = 10000000; // 10 Millones
var hair_m = [30030, 30020, 30000, 30130, 30190, 30110, 30180, 30050, 30040, 30160, 30230, 30240, 30290, 30350, 30450];
var hair_f = [31040, 31000, 31050, 31030, 31070, 31150, 31160, 31100, 31120, 31140, 31230, 31270, 31480, 31590, 31690];
var hair_list;

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
        cm.sendYesNo("¡Hola! Soy el estilista de #bEllinMS#k. \r\n¿Te gustaría un nuevo estilo de cabello VIP?\r\n\r\nEl costo es de #r10,000,000 Mesos (10M)#k.");
    } else if (status == 1) {
        if (cm.getMeso() < price) {
            cm.sendOk("Lo siento, no tienes suficientes mesos. Necesitas al menos #r10,000,000 Mesos#k para un cambio de look.");
            cm.dispose();
        } else {
            hair_list = (cm.getPlayer().getGender() == 0) ? hair_m : hair_f;
            cm.sendStyle("Por favor, elige el estilo de cabello que más te guste:", hair_list);
        }
    } else if (status == 2) {
        if (cm.getMeso() >= price) {
            cm.gainMeso(-price);
            cm.setHair(hair_list[selection]);
            cm.sendOk("¡Disfruta tu nuevo estilo de cabello!");
        } else {
            cm.sendOk("Hubo un error con tus mesos.");
        }
        cm.dispose();
    }
}
