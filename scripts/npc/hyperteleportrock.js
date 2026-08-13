var status = -1;

function start() {
    status = -1;
    action(1, 0, 0);
}

function action(mode, type, selection) {
    if (mode == 1) {
        status++;
    } else {
        cm.dispose();
        return;
    }

    var targetMapId = cm.getPlayer().getHyperTeleportRockTargetMapId();
    if (targetMapId <= 0) {
        cm.dispose();
        return;
    }

    var mapFactory = cm.getClient().getChannelServer().getMapFactory();
    var targetMap = mapFactory.getMap(targetMapId);

    if (targetMap == null) {
        cm.dispose();
        return;
    }

    if (status == 0) {
        cm.sendYesNo("Estas seguro de que quieres viajar a #b" + targetMap.getMapName() + "#k?");
    } else if (status == 1) {
        cm.dispose();
        Packages.server.maps.HyperTeleportRockService.tryTeleport(cm.getClient(), targetMapId, true);
    }
}
