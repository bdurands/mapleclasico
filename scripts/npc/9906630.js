/*
 * NPC: 9906630
 * Propósito: NPC de cambio de Estética (Hair, Hair Color, Face, Face Color)
 */
var status = 0;
var category = -1;
var mainSelection = -1;
var price = 10000000; // 10 Millones

// Arrays de estilos VIP
var hair_m = [30030, 30020, 30000, 30130, 30190, 30110, 30180, 30050, 30040, 30160, 30230, 30240, 30290, 30350, 30450];
var hair_f = [31040, 31000, 31050, 31030, 31070, 31150, 31160, 31100, 31120, 31140, 31230, 31270, 31480, 31590, 31690];
var face_m = [20000, 20001, 20002, 20003, 20004, 20005, 20006, 20007, 20008, 20012, 20014, 20031];
var face_f = [21000, 21001, 21002, 21003, 21004, 21005, 21006, 21007, 21008, 21012, 21014, 21016];

var style_list;

function start() {
    status = -1;
    category = -1;
    mainSelection = -1;
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
        var text = "¡Hola! Soy el estilista de #bEllinMS#k. \r\n¿Qué te gustaría hacer hoy?\r\n\r\n";
        text += "#L0##bCambiar mi estilo#k#l\r\n";
        text += "#L1##dReclamar Premios Gratis#k#l\r\n";
        cm.sendSimple(text);
        
    } else if (status == 1) {
        mainSelection = selection;
        
        if (mainSelection == 0) { // Estilos
            var text = "Cualquier cambio estético cuesta #r10,000,000 Mesos (10M)#k.\r\n¿Qué te gustaría cambiar?\r\n\r\n";
            text += "#L0##bCambiar estilo de Cabello#k#l\r\n";
            text += "#L1##dTeñir el Cabello (Color)#k#l\r\n";
            text += "#L2##rCambiar Ojos/Rostro (Face)#k#l\r\n";
            text += "#L3##gCambiar color de Ojos#k#l\r\n";
            cm.sendSimple(text);
        } else if (mainSelection == 1) { // Premios
            var text = "¿Qué premio gratuito deseas reclamar?\r\n\r\n";
            text += "#L0##bReclamar Smegas Gratis (x50)#k#l\r\n";
            cm.sendSimple(text);
        }
        
    } else if (status == 2) {
        if (mainSelection == 0) { // Estilos -> Send Style
            category = selection;
            
            if (cm.getMeso() < price) {
                cm.sendOk("Lo siento, necesitas al menos #r10,000,000 Mesos#k para cualquier cambio de look.");
                cm.dispose();
                return;
            }

            if (category == 0) { // Cabello
                style_list = (cm.getPlayer().getGender() == 0) ? hair_m : hair_f;
                cm.sendStyle("Elige el estilo de cabello que más te guste:", style_list);
            } else if (category == 1) { // Color de Cabello
                var currentHair = cm.getPlayer().getHair();
                var baseHair = Math.floor(currentHair / 10) * 10;
                style_list = [];
                for (var i = 0; i < 8; i++) {
                    style_list.push(baseHair + i);
                }
                cm.sendStyle("Elige el color que deseas para tu cabello actual:", style_list);
            } else if (category == 2) { // Face
                style_list = (cm.getPlayer().getGender() == 0) ? face_m : face_f;
                cm.sendStyle("Elige el estilo de rostro que más te guste:", style_list);
            } else if (category == 3) { // Color de Face
                var currentFace = cm.getPlayer().getFace();
                var faceType = currentFace % 100; // Extrae el ID base de la cara (0 a 99)
                var genderFaceBase = (cm.getPlayer().getGender() == 0) ? 20000 : 21000;
                
                style_list = [];
                for (var i = 0; i < 8; i++) {
                    style_list.push(genderFaceBase + (i * 100) + faceType);
                }
                cm.sendStyle("Elige el color que deseas para tus ojos:", style_list);
            }
        } else if (mainSelection == 1) { // Premios -> Reclamar
            if (selection == 0) { // Smegas
                var smegaId = 5390001;
                var currentSmegas = cm.getItemQuantity(smegaId);
                if (currentSmegas <= 5) {
                    if (cm.canHold(smegaId, 50)) {
                        cm.gainItem(smegaId, 50);
                        cm.sendOk("¡Aquí tienes tus 50 Smegas gratis! Úsalos sabiamente.");
                    } else {
                        cm.sendOk("No tienes espacio suficiente en tu inventario Cash.");
                    }
                } else {
                    cm.sendOk("Ya tienes suficientes Smegas. Solo puedes reclamar más si tienes 5 o menos en tu inventario.");
                }
            }
            cm.dispose();
        }
        
    } else if (status == 3) {
        if (mainSelection == 0) { // Estilos -> Aplicar
            if (cm.getMeso() >= price) {
                cm.gainMeso(-price);
                
                if (category == 0 || category == 1) {
                    cm.setHair(style_list[selection]);
                } else if (category == 2 || category == 3) {
                    cm.setFace(style_list[selection]);
                }
                
                cm.sendOk("¡Disfruta tu nuevo estilo VIP!");
            } else {
                cm.sendOk("Hubo un error con tus mesos.");
            }
        }
        cm.dispose();
    }
}
