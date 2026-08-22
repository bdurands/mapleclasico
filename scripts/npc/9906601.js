var status = 0;

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
        // Obtenemos los puntos disponibles nativamente en tu source (Kaentake)
        var votePoints = cm.getClient().getVotePoints();
        var pqPoints = cm.getPlayer().getPqPoints();
        
        // Boss points ahora implementados en el Source
        var bossPoints = cm.getPlayer().getBossPoints();
        
        // Donor Points = Maple Points (Tipo 2 en el CashShop)
        var donorPoints = cm.getPlayer().getCashShop().getCash(2);

        var text = "¡Hola #b#h ##k! Aquí tienes un resumen de tus puntos actuales:\r\n\r\n";
        text += "#ePuntos de Votación:#n " + votePoints + "\r\n";
        text += "#ePuntos de Party Quest (PQ):#n " + pqPoints + "\r\n";
        text += "#ePuntos de Boss:#n " + bossPoints + "\r\n";
        text += "#ePuntos de Donador:#n " + donorPoints + "\r\n\r\n";
        text += "------------------------------------------------------\r\n";
        text += "#L0##bCanjear 10 Puntos de Votación por 5,000 NX Credit#k#l\r\n";

        cm.sendSimple(text);
    } else if (status == 1) {
        if (selection == 0) {
            var votePoints = cm.getClient().getVotePoints();
            if (votePoints >= 10) {
                cm.getClient().useVotePoints(10);
                cm.getPlayer().getCashShop().gainCash(1, 5000); // 1 = NX Credit
                cm.sendOk("¡Felicidades! Has canjeado #r10 Puntos de Votación#k por #b5,000 NX Credit#k. ¡Disfrútalos!");
                cm.dispose();
            } else {
                cm.sendOk("Lo siento, no tienes suficientes Puntos de Votación. Necesitas al menos 10 y actualmente tienes #r" + votePoints + "#k.");
                cm.dispose();
            }
        }
    }
}
