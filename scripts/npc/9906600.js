var status = -1;
var selectionTier = -1;

// ID base para los registros de progreso de la misión
var QUEST_BASE_ID = 9999000;

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
        
        var hasItem = cm.haveItem(tier.item, tier.count);
        var hasMeso = cm.getMeso() >= tier.meso;
        
        text += (hasItem ? "#b" : "#r") + " - " + tier.count + "x #i" + tier.item + "# #t" + tier.item + "##k\r\n";
        text += (hasMeso ? "#b" : "#r") + " - " + cm.formatNumber(tier.meso) + " Mesos#k\r\n\r\n";
        
        text += "¿Tienes todo listo para el intercambio?";
        
        cm.sendYesNo(text);
        
    } else if (status == 2) {
        var tier = tiers[selectionTier];
        
        if (cm.haveItem(tier.item, tier.count) && cm.getMeso() >= tier.meso) {
            
            // Consumir items y mesos
            cm.gainItem(tier.item, -tier.count);
            cm.gainMeso(-tier.meso);
            
            // Aumentar HP y MP
            var newHp = cm.getPlayer().getMaxHp() + tier.hp;
            var newMp = cm.getPlayer().getMaxMp() + tier.mp;
            
            cm.getPlayer().setMaxHp(newHp);
            cm.getPlayer().setMaxMp(newMp);
            
            // Actualizar stats visualmente para el jugador
            cm.getPlayer().updateSingleStat(Packages.client.Stat.MAXHP, newHp);
            cm.getPlayer().updateSingleStat(Packages.client.Stat.MAXMP, newMp);
            
            // Marcar tier como completado usando QuestRecord (muy seguro en Kaentake/v83)
            setTierCompleted(tier.questId);
            
            // Efecto visual y de sonido opcional
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
    return record != null && record.getCustomData() != null && record.getCustomData().equals("done");
}

function setTierCompleted(qId) {
    var record = cm.getQuestRecord(qId);
    if (record != null) {
        record.setCustomData("done");
    }
}
