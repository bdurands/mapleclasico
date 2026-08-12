/**
 * whodrops.js — NPC script for @whodrops / @wd
 * Opens in ITEM search mode (find which monsters drop an item).
 *
 * State machine:
 *   status 0 → main menu (sendSimple)
 *   status 1 → handle main menu selection → send text input (sendGetText)
 *   status 2 → process text input → show list or detail (sendSimple)
 *   status 3 → handle list/detail selection
 *
 * The key rule: every cm.sendXxx() call pauses the script.
 * The next user interaction calls action() again.
 * NEVER call action() recursively — set status and let it flow through.
 */
var DropSearchService = Java.type("server.DropSearchService");

var status;
var mobMode;     // false = item search (whodrops), true = mob search (whatdropsfrom)
var query;
var page;
var results;     // int[] from DropSearchService
var detailPages; // String[] from DropSearchService
var detailPage;

function start() {
    status = -1;
    mobMode = false; // whodrops starts in item mode
    query = "";
    page = 0;
    results = null;
    detailPages = null;
    detailPage = 0;

    // Check if the command had a search term
    var cmdMsg = cm.getPlayer().getLastCommandMessage();
    if (cmdMsg != null && cmdMsg.trim().length > 0) {
        query = cmdMsg.trim();
        // Skip to search processing
        doSearch();
    } else {
        // Show main menu
        cm.sendSimple(DropSearchService.mainMenu(""));
        status = 0;
    }
}

function action(mode, type, selection) {
    if (mode == -1) {
        cm.dispose();
        return;
    }

    if (status == 0) {
        // User responded to main menu
        if (mode == 0) { cm.dispose(); return; }
        if (selection == 0) {
            mobMode = false; // item search
        } else if (selection == 1) {
            mobMode = true;  // mob search
        } else {
            cm.dispose();
            return;
        }
        cm.sendGetText(DropSearchService.searchPrompt(mobMode));
        status = 1;

    } else if (status == 1) {
        // User responded to text input
        if (mode == 0) {
            // Cancelled text input → back to main menu
            cm.sendSimple(DropSearchService.mainMenu(""));
            status = 0;
            return;
        }
        query = cm.getText();
        if (query == null || query.trim().length == 0) {
            cm.sendSimple(DropSearchService.mainMenu(""));
            status = 0;
            return;
        }
        query = query.trim();
        doSearch();

    } else if (status == 2) {
        // User responded to search result list
        if (mode == 0) {
            // Back → main menu
            cm.sendSimple(DropSearchService.mainMenu(""));
            status = 0;
            return;
        }
        handleListSelection(selection);

    } else if (status == 3) {
        // User responded to detail view
        if (mode == 0) {
            // Back → show list again if we have results, else main menu
            if (results != null && results.length > 1) {
                showList();
            } else {
                cm.sendSimple(DropSearchService.mainMenu(""));
                status = 0;
            }
            return;
        }
        handleDetailSelection(selection);
    }
}

function doSearch() {
    if (mobMode) {
        results = DropSearchService.findMobs(query);
    } else {
        results = DropSearchService.findItems(query);
    }

    var count = DropSearchService.getResultCount(results);

    if (count == 0) {
        cm.sendSimple(DropSearchService.mainMenu("No results found for '#b" + DropSearchService.escapeUserText(query) + "#k'."));
        status = 0;
        query = "";
    } else if (count == 1) {
        // Auto-select the single result
        showDetail(results[0]);
    } else {
        page = 0;
        showList();
    }
}

function showList() {
    var text;
    if (mobMode) {
        text = DropSearchService.mobListPage(query, results, page);
    } else {
        text = DropSearchService.itemListPage(query, results, page);
    }
    cm.sendSimple(text);
    status = 2;
}

function showDetail(id) {
    if (mobMode) {
        detailPages = DropSearchService.mobDropPages(cm.getPlayer(), id);
    } else {
        detailPages = DropSearchService.itemDropperPages(id);
    }
    detailPage = 0;
    cm.sendSimple(detailPages[detailPage]);
    status = 3;
}

function handleListSelection(selection) {
    if (selection == 10000001) {
        // Prev page
        page = Math.max(0, page - 1);
        showList();
    } else if (selection == 10000002) {
        // Next page
        page++;
        showList();
    } else if (selection >= 0 && selection < 10000000) {
        // Selected a mob or item
        showDetail(selection);
    } else {
        cm.dispose();
    }
}

function handleDetailSelection(selection) {
    if (selection == 10000001) {
        // Prev page
        detailPage = Math.max(0, detailPage - 1);
        cm.sendSimple(detailPages[detailPage]);
        status = 3;
    } else if (selection == 10000002) {
        // Next page
        detailPage++;
        cm.sendSimple(detailPages[detailPage]);
        status = 3;
    } else if (selection == 10000004) {
        // Back to main menu
        query = "";
        results = null;
        cm.sendSimple(DropSearchService.mainMenu(""));
        status = 0;
    } else {
        cm.dispose();
    }
}
