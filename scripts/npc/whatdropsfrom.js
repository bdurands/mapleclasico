var DropSearchService = Java.type("server.DropSearchService");

var status = -1;
var mobMode = true;
var query = "";
var page = 0;
var results = [];
var selectedId = -1;
var detailPages = [];

function start() {
    mobMode = true;
    query = cm.getPlayer().getLastCommandMessage();
    if (query != null && query.trim() !== "") {
        query = query.trim();
        status = 1; // start from list step
        action(1, 0, 0);
    } else {
        query = "";
        status = -1;
        action(1, 0, 0);
    }
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
        // Main Menu state
        cm.sendSimple(DropSearchService.mainMenu(""));
    } else if (status == 1) {
        // Selection from Main Menu
        if (query === "") {
            if (selection == 10000006) {
                mobMode = true;
            } else if (selection == 10000007) {
                mobMode = false;
            } else {
                cm.dispose();
                return;
            }
            cm.sendGetText(DropSearchService.searchPrompt(mobMode));
        } else {
            // Processing query from quick search or main menu jump
            status++; // go to processing
            action(1, 0, 0);
        }
    } else if (status == 2) {
        // Process Input
        if (query === "") {
            query = cm.getText();
        }
        if (query == null || query.trim() === "") {
            status = -1;
            query = "";
            action(1, 0, 0);
            return;
        }
        query = query.trim();

        if (mobMode) {
            results = DropSearchService.findMobs(query);
        } else {
            results = DropSearchService.findItems(query);
        }

        if (results.length === 0) {
            cm.sendSimple(DropSearchService.mainMenu("No results found for '" + DropSearchService.escapeUserText(query) + "'."));
            status = 0;
            query = ""; // reset
        } else if (results.length === 1) {
            // Auto-select if only 1 result
            selectedId = results[0];
            page = 0;
            if (mobMode) {
                detailPages = DropSearchService.mobDropPages(cm.getPlayer(), selectedId, false);
            } else {
                detailPages = DropSearchService.itemDropperPages(cm.getPlayer(), selectedId, false);
            }
            status = 4; // jump to Detail View display
            action(1, 0, 0);
        } else {
            // List View
            page = 0;
            status = 3;
            action(1, 0, 0);
        }
    } else if (status == 3) {
        // Display List View
        var text = mobMode ? DropSearchService.mobListPage(query, results, page) : DropSearchService.itemListPage(query, results, page);
        cm.sendSimple(text);
    } else if (status == 4) {
        // Handle List View selection
        if (selection == 10000001) {
            page--;
            status = 2; // back to List View rendering
            action(1, 0, 0);
            return;
        } else if (selection == 10000002) {
            page++;
            status = 2;
            action(1, 0, 0);
            return;
        } else if (selection >= 0 && selection < 10000000) {
            // User selected an item/mob from the list
            selectedId = selection;
            page = 0;
            if (mobMode) {
                detailPages = DropSearchService.mobDropPages(cm.getPlayer(), selectedId, true);
            } else {
                detailPages = DropSearchService.itemDropperPages(cm.getPlayer(), selectedId, true);
            }
            status = 5; // move to Detail View display
            action(1, 0, 0);
        } else {
            cm.dispose();
        }
    } else if (status == 5) {
        // Display Detail View
        cm.sendSimple(detailPages[page]);
    } else if (status == 6) {
        // Handle Detail View selection
        if (selection == 10000001) {
            page--;
            status = 4; // loop back to detail display
            action(1, 0, 0);
        } else if (selection == 10000002) {
            page++;
            status = 4;
            action(1, 0, 0);
        } else if (selection == 10000004) {
            // Back to main menu
            query = "";
            status = -1;
            action(1, 0, 0);
        } else if (selection == 10000005) {
            // Back to results
            page = 0;
            status = 2;
            action(1, 0, 0);
        } else if (selection >= 0 && selection < 10000000) {
            // Selected an item inside a mob's drop list, or a mob inside an item's dropper list.
            // Switch modes and search for that exactly.
            selectedId = selection;
            mobMode = !mobMode;
            page = 0;
            if (mobMode) {
                detailPages = DropSearchService.mobDropPages(cm.getPlayer(), selectedId, true);
            } else {
                detailPages = DropSearchService.itemDropperPages(cm.getPlayer(), selectedId, true);
            }
            status = 4; // loop back to detail display
            action(1, 0, 0);
        } else {
            cm.dispose();
        }
    }
}
