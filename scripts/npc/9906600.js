var status = -1;
var selectionTier = -1;

// ID base para los registros de progreso de la misión
var QUEST_BASE_ID = 9999000;

// Función auxiliar para formatear números con comas (GraalVM no soporta regex literals)
function formatNumber(num) {
    var str = "" + num;
    var result = "";
    var count = 0;
    for (var i = str.length - 1; i >= 0; i--) {
        result = str.charAt(i) + result;
        count++;
        if (count % 3 == 0 && i > 0) {
            result = "," + result;
        }
    }
    return result;
}
var tiers = [
    { 
        level: 50, 
        hp: 500, 
        mp: 500, 
        item: 4000019, // Zombie's Lost Tooth
        count: 500, 
        meso: 500000, 
        questId: QUEST_BASE_ID + 50 
    },
    { 
        level: 70, 
        hp: 1000, 
        mp: 1000, 
        item: 4000024, // Hector's Tail
        count: 1000, 
        meso: 1000000, 
        questId: QUEST_BASE_ID + 70 
    },
    { 
        level: 90, 
        hp: 1000, 
        mp: 1000, 
        item: 4000273, // Squid Tentacle
        count: 1000, 
        meso: 2000000, 
        questId: QUEST_BASE_ID + 90 
    },
    { 
        level: 100, 
        hp: 1500, 
        mp: 1500, 
        item: 4000313, // Piece of Time
        count: 1, // Extremadamente raro
        meso: 5000000, 
        questId: QUEST_BASE_ID + 100 
    }
];

function start() {
    if (cm.getPlayer().gmLevel() <= 2) {
        cm.sendOk("Este NPC no está disponible en este momento.");
        cm.dispose();
        return;
    }
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
        var text = "#e#d[ Alquimia de Sangre y Maná ]#k#n\r\n\r\n";
        text += "Hola #h #, puedo ayudarte a romper los límites de tu cuerpo físico aumentando tu #bMax HP#k y #bMax MP#k si me traes los materiales adecuados. Cada poción solo puede ser consumida #rUNA VEZ#k por rango de nivel.\r\n\r\n";
        
        for (var i = 0; i < tiers.length; i++) {
            var tier = tiers[i];
            var isCompleted = isTierCompleted(tier.questId);
            
            if (isCompleted) {
                text += "#L" + i + "##r[Completado] Bono Nivel " + tier.level + " (+"+tier.hp+" HP / +"+tier.mp+" MP)#k#l\r\n";
            } else {
                if (cm.getPlayer().getLevel() >= tier.level) {
                    text += "#L" + i + "##b[Disponible] Bono Nivel " + tier.level + " (+"+tier.hp+" HP / +"+tier.mp+" MP)#k#l\r\n";
                } else {
                    text += "#L" + i + "##d[Bloqueado] Bono Nivel " + tier.level + " (Requiere Nivel "+tier.level+")#k#l\r\n";
                }
            }
        }
        
        cm.sendSimple(text);
        
    } else if (status == 1) {
        selectionTier = selection;
        var tier = tiers[selectionTier];
        
        if (isTierCompleted(tier.questId)) {
            cm.sendOk("Ya has consumido la poción de este rango de nivel. Tu cuerpo no soportaría otra.");
            cm.dispose();
            return;
        }
        
        if (cm.getPlayer().getLevel() < tier.level) {
            cm.sendOk("Aún eres muy débil. Necesitas ser al menos nivel #r" + tier.level + "#k para resistir esta poción.");
            cm.dispose();
            return;
        }
        
        var text = "#e#b[ Bono Nivel " + tier.level + " ]#k#n\r\n\r\n";
        text += "Para preparar la poción de este nivel que te dará #r+" + tier.hp + " MaxHP#k y #b+" + tier.mp + " MaxMP#k, necesito que me consigas lo siguiente:\r\n\r\n";
        
        var hasMeso = cm.getMeso() >= tier.meso;
        var reqMet = hasMeso;
        
        if (tier.level == 50) {
            text += "Demuestra tu fuerza derrotando a 100 de los siguientes monstruos:\r\n";
            var record = cm.getQuestRecord(tier.questId);
            var data = record != null ? ("" + record.getCustomData()) : "";
            var counts = [0,0,0,0,0,0,0,0,0,0,0,0,0];
            if (data != "" && data != "done") {
                var parts = data.split(",");
                for (var j = 0; j < parts.length && j < 13; j++) {
                    counts[j] = parseInt(parts[j]) || 0;
                }
            }
            
            var allKilled = true;
            var mobNamesArr = ["Samiho", "Hector", "Firebomb", "Malady", "Bellflower Root", "Hodori", "Mixed Golem", "Croco", "Skeleton Soldier", "Dark Jr. Yeti", "Miner Zombie", "Stone Golem", "Drake"];
            for (var k = 0; k < 13; k++) {
                var c = counts[k];
                if (c < 100) allKilled = false;
                text += (c >= 100 ? "#b" : "#r") + " - " + mobNamesArr[k] + " (" + c + " / 100)#k\r\n";
            }
            text += "\r\n";
            reqMet = reqMet && allKilled;
        } else {
            var hasItem = cm.haveItem(tier.item, tier.count);
            reqMet = reqMet && hasItem;
            text += (hasItem ? "#b" : "#r") + " - " + tier.count + "x #i" + tier.item + "# #t" + tier.item + "##k\r\n";
        }
        
        text += (hasMeso ? "#b" : "#r") + " - " + formatNumber(tier.meso) + " Mesos#k\r\n\r\n";
        
        if (!reqMet) {
            cm.sendOk(text + "Aún no cumples con los requisitos. Regresa cuando estés listo.");
            cm.dispose();
            return;
        }
        
        text += "¿Tienes todo listo para el intercambio?";
        
        cm.sendYesNo(text);
        
    } else if (status == 2) {
        var tier = tiers[selectionTier];
        var reqMet = cm.getMeso() >= tier.meso;
        
        if (tier.level == 50) {
            var record = cm.getQuestRecord(tier.questId);
            var data = record != null ? ("" + record.getCustomData()) : "";
            var allKilled = true;
            if (data != "" && data != "done") {
                var parts = data.split(",");
                if (parts.length >= 13) {
                    for (var k = 0; k < 13; k++) {
                        if ((parseInt(parts[k]) || 0) < 100) allKilled = false;
                    }
                } else {
                    allKilled = false;
                }
            } else {
                allKilled = false;
            }
            reqMet = reqMet && allKilled;
        } else {
            reqMet = reqMet && cm.haveItem(tier.item, tier.count);
        }
        
        if (reqMet) {
            
            // Consumir items (solo si no es nivel 50) y mesos
            if (tier.level != 50) {
                cm.gainItem(tier.item, -tier.count);
            }
            cm.gainMeso(-tier.meso);
            
            // Calcular nuevos valores de MaxHP y MaxMP
            var newMaxHp = cm.getPlayer().getMaxHp() + tier.hp;
            var newMaxMp = cm.getPlayer().getMaxMp() + tier.mp;
            
            // Aplicar el incremento usando el método correcto del engine
            // updateMaxHpMaxMp actualiza internamente: maxhp, maxmp, localmaxhp, localmaxmp
            // y envía el paquete de stats al cliente automáticamente
            cm.getPlayer().updateMaxHpMaxMp(newMaxHp, newMaxMp);
            
            // Guardar en base de datos para que persista al relogear
            cm.getPlayer().saveCharToDB();
            
            // Marcar tier como completado usando QuestRecord
            setTierCompleted(tier.questId);
            
            // Mensaje al jugador
            cm.getPlayer().dropMessage(5, "¡Tus límites físicos han aumentado! +" + tier.hp + " MaxHP / +" + tier.mp + " MaxMP.");
            
            cm.sendOk("¡La poción ha sido un éxito! Siente cómo la energía recorre tus venas. Tu HP y MP máximo han aumentado de forma permanente.");
            
        } else {
            cm.sendOk("No tienes los requisitos necesarios. Regresa cuando hayas recolectado todo el material.");
        }
        
        cm.dispose();
    }
}

// Funciones Auxiliares para manejo de progreso
function isTierCompleted(qId) {
    var record = cm.getQuestRecord(qId);
    return record != null && record.getCustomData() != null && ("" + record.getCustomData()) == "done";
}

function setTierCompleted(qId) {
    var record = cm.getQuestRecord(qId);
    if (record != null) {
        record.setCustomData("done");
    }
}
