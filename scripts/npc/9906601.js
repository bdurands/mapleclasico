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
        var votePoints = cm.getClient().getVotePoints();
        var pqPoints = cm.getPlayer().getPqPoints();
        var bossPoints = cm.getPlayer().getBossPoints();
        var donorPoints = cm.getPlayer().getCashShop().getCash(2);

        var text = "\t\t\t#d#e Centro de Puntos EllinMS #n#k\r\n\r\n";
        text += "¡Hola, #b#h ##k! Aquí tienes el resumen de tus puntos acumulados. ¡Sigue así!\r\n\r\n";
        
        text += "  - #e#bVotación:#n#k \t\t" + votePoints + " Puntos\r\n";
        text += "  - #e#dParty Quest:#n#k \t" + pqPoints + " Puntos\r\n";
        text += "  - #e#rBoss Points:#n#k \t" + bossPoints + " Puntos\r\n";
        text += "  - #e#bDonador:#n#k \t\t" + donorPoints + " Puntos\r\n\r\n"; // Cambiado a azul (#b) para evitar el verde chillón
        
        text += "#b------------------------------------------------------#k\r\n";
        text += "#L0##eCanjear#n 10 Puntos de Votación por #b5,000 NX Credit#k#l\r\n";

        cm.sendSimple(text);
    } else if (status == 1) {
        if (selection == 0) {
            var votePoints = cm.getClient().getVotePoints();
            if (votePoints >= 10) {
                cm.getClient().useVotePoints(10);
                cm.getPlayer().getCashShop().gainCash(1, 5000); // 1 = NX Credit
                cm.sendOk("¡Felicidades! Has canjeado #r10 Puntos de Votación#k por #b5,000 NX Credit#k.\r\n\r\n¡Disfrútalos en el Cash Shop!");
                cm.dispose();
            } else {
                cm.sendOk("Lo siento, no tienes suficientes #b10 Puntos de Votación#k.\r\n\r\nActualmente tienes: #r" + votePoints + "#k.");
                cm.dispose();
            }
        }
    }
}
