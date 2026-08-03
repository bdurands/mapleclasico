/* 
 * NPC ID: 9906200
 * Description: Hace al jugador GM Nivel 3.
 */

var status = 0;

function start() {
    status = -1;
    action(1, 0, 0);
}

function action(mode, type, selection) {
    if (mode == -1) {
        cm.dispose();
    } else {
        if (mode == 0 && status == 0) {
            cm.dispose();
            return;
        }
        if (mode == 1)
            status++;
        else
            status--;

        if (status == 0) {
            cm.sendYesNo("Hola #b#h ##k, soy el administrador de GMs. \r\n¿Quieres que te convierta en un #rGame Master (Nivel 3)#k ahora mismo?");
        } else if (status == 1) {
            cm.getPlayer().setGMLevel(3);
            cm.getClient().setGMLevel(3);
            cm.sendOk("¡Felicidades! Ahora eres un GM Nivel 3. \r\nPor favor, #bcierra sesión y vuelve a entrar#k para que se activen todos tus comandos y privilegios correctamente.");
            cm.dispose();
        }
    }
}
