var status = 0;
var category = -1; // -1: menu principal, 0: votos, 1: pq, 2: boss

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
        category = -1;
        var votePoints = cm.getClient().getVotePoints();
        var pqPoints = cm.getPlayer().getPqPoints();
        var bossPoints = cm.getPlayer().getBossPoints();
        var donorPoints = cm.getPlayer().getCashShop().getCash(2);

        var text = "\t\t\t#d#e Centro de Puntos EllinMS #n#k\r\n\r\n";
        text += "¡Hola, #b#h ##k! Aquí tienes el resumen de tus puntos acumulados:\r\n\r\n";
        
        text += "  - #e#bVotación:#n#k \t\t" + votePoints + " Puntos\r\n";
        text += "  - #e#dParty Quest:#n#k \t" + pqPoints + " Puntos\r\n";
        text += "  - #e#rBoss Points:#n#k \t" + bossPoints + " Puntos\r\n";
        text += "  - #e#bDonador:#n#k \t\t" + donorPoints + " Puntos\r\n\r\n";
        
        text += "#b------------------------------------------------------#k\r\n";
        text += "#L0##bCanjear Puntos de Votación#k#l\r\n";
        text += "#L1##dCanjear PQ Points#k#l\r\n";
        text += "#L2##rCanjear Boss Points#k#l\r\n";

        cm.sendSimple(text);
        
    } else if (status == 1) {
        category = selection;
        
        if (category == 0) { // Menú de Votación
            var votePoints = cm.getClient().getVotePoints();
            var text = "#e#b[ Canjear Puntos de Votación ]#k#n\r\n";
            text += "Actualmente tienes: #r" + votePoints + "#k puntos.\r\n\r\n";
            text += "#L0#Canjear 10 puntos por #b4,000 NX Credit#k#l\r\n";
            text += "#L1#Canjear 5 puntos por #b1,500 NX Credit#k#l\r\n";
            text += "#L2#Canjear 5 puntos por #b500,000 Mesos#k#l\r\n";
            text += "#L3#Canjear 10 puntos por 1 #i4037000# #bEllin Coin#k#l\r\n";
            cm.sendSimple(text);
            
        } else if (category == 1) { // Menú de PQ
            var pqPoints = cm.getPlayer().getPqPoints();
            var text = "#e#d[ Canjear PQ Points ]#k#n\r\n";
            text += "Actualmente tienes: #r" + pqPoints + "#k puntos.\r\n\r\n";
            text += "#L0#Canjear 3 puntos por 1 #i2022644# #dEllin Box#k#l\r\n";
            cm.sendSimple(text);
            
        } else if (category == 2) { // Menú de Boss
            var text = "#e#r[ Canjear Boss Points ]#k#n\r\n\r\n";
            text += "Próximamente...\r\n";
            cm.sendOk(text);
            cm.dispose();
        }
        
    } else if (status == 2) {
        if (category == 0) { // Procesar Votación
            var votePoints = cm.getClient().getVotePoints();
            if (selection == 0) {
                if (votePoints >= 10) {
                    cm.getClient().useVotePoints(10);
                    cm.getPlayer().getCashShop().gainCash(1, 4000); // 1 = NX Credit
                    cm.sendOk("Has canjeado 10 Puntos de Votación por #b4,000 NX Credit#k.");
                } else {
                    cm.sendOk("No tienes suficientes puntos.");
                }
            } else if (selection == 1) {
                if (votePoints >= 5) {
                    cm.getClient().useVotePoints(5);
                    cm.getPlayer().getCashShop().gainCash(1, 1500);
                    cm.sendOk("Has canjeado 5 Puntos de Votación por #b1,500 NX Credit#k.");
                } else {
                    cm.sendOk("No tienes suficientes puntos.");
                }
            } else if (selection == 2) {
                if (votePoints >= 5) {
                    cm.getClient().useVotePoints(5);
                    cm.gainMeso(500000);
                    cm.sendOk("Has canjeado 5 Puntos de Votación por #b500,000 Mesos#k.");
                } else {
                    cm.sendOk("No tienes suficientes puntos.");
                }
            } else if (selection == 3) {
                if (votePoints >= 10) {
                    if (cm.canHold(4037000, 1)) {
                        cm.getClient().useVotePoints(10);
                        cm.gainItem(4037000, 1);
                        cm.sendOk("Has canjeado 10 Puntos de Votación por 1 #i4037000# #bEllin Coin#k.");
                    } else {
                        cm.sendOk("Por favor revisa que tengas espacio en tu inventario.");
                    }
                } else {
                    cm.sendOk("No tienes suficientes puntos.");
                }
            }
            cm.dispose();
            
        } else if (category == 1) { // Procesar PQ
            var pqPoints = cm.getPlayer().getPqPoints();
            if (selection == 0) {
                if (pqPoints >= 3) {
                    if (cm.canHold(2022644, 1)) {
                        cm.getPlayer().gainPqPoints(-3); // Resta 3 puntos
                        cm.gainItem(2022644, 1);
                        cm.sendOk("Has canjeado 3 PQ Points por 1 #i2022644# #dEllin Box#k.");
                    } else {
                        cm.sendOk("Por favor revisa que tengas espacio en tu inventario.");
                    }
                } else {
                    cm.sendOk("No tienes suficientes puntos.");
                }
            }
            cm.dispose();
        }
    }
}
