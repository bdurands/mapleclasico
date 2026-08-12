package server;

import client.Character;
import constants.id.ItemId;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import provider.Data;
import provider.DataProvider;
import server.life.MonsterDropEntry;
import server.life.MonsterInformationProvider;
import tools.DatabaseConnection;
import tools.Pair;

import java.sql.Connection;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.util.*;

/**
 * Stateless service for drop search queries (@whodrops / @whatdropsfrom).
 * All rendering, search, rates, WZ/DB lookups live here.
 * The NPC scripts only manage conversation state (which page, which mode).
 */
public class DropSearchService {
    private static final Logger log = LoggerFactory.getLogger(DropSearchService.class);

    private static final int MOBS_PER_PAGE = 8;
    private static final int ITEMS_PER_PAGE = 10;
    private static final int DROPS_PER_PAGE = 12;
    private static final int SOURCES_PER_PAGE = 8;

    private static volatile boolean cachesLoaded = false;

    // mob id -> mob name (from String.wz/Mob.img)
    private static final Map<Integer, String> mobNames = new HashMap<>();
    // item id -> normalized item name (from getAllItems)
    private static final List<Pair<Integer, String>> itemNamesNormalized = new ArrayList<>();
    // Also keep original names for display
    private static final Map<Integer, String> itemNamesOriginal = new HashMap<>();

    private static final Object lock = new Object();

    public static void clearCaches() {
        synchronized (lock) {
            cachesLoaded = false;
            mobNames.clear();
            itemNamesNormalized.clear();
            itemNamesOriginal.clear();
        }
    }

    private static void ensureCaches() {
        if (cachesLoaded) return;
        synchronized (lock) {
            if (cachesLoaded) return;
            loadMobNames();
            loadItemNames();
            cachesLoaded = true;
            log.info("DropSearchService caches loaded: {} mobs, {} items", mobNames.size(), itemNamesNormalized.size());
        }
    }

    private static void loadMobNames() {
        DataProvider stringData = ItemInformationProvider.getInstance().stringData;
        if (stringData != null) {
            Data mobStringData = stringData.getData("Mob.img");
            if (mobStringData != null) {
                for (Data d : mobStringData.getChildren()) {
                    try {
                        int id = Integer.parseInt(d.getName());
                        Data nameData = d.getChildByPath("name");
                        if (nameData != null) {
                            mobNames.put(id, (String) nameData.getData());
                        }
                    } catch (NumberFormatException ignored) {}
                }
            }
        }
    }

    private static void loadItemNames() {
        // Force the item name cache to be populated by calling getAllItems()
        List<Pair<Integer, String>> allItems = ItemInformationProvider.getInstance().getAllItems();
        for (Pair<Integer, String> pair : allItems) {
            String name = pair.getRight();
            if (name == null || name.equals("NO-NAME")) continue;
            itemNamesNormalized.add(new Pair<>(pair.getLeft(), normalize(name)));
            itemNamesOriginal.put(pair.getLeft(), name);
        }
    }

    private static String normalize(String s) {
        if (s == null) return "";
        StringBuilder sb = new StringBuilder(s.length());
        for (char c : s.toCharArray()) {
            if (c >= 'A' && c <= 'Z') sb.append((char)(c + 32));
            else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) sb.append(c);
            // Skip spaces, special chars, accents, etc.
        }
        return sb.toString();
    }

    public static String escapeUserText(String text) {
        if (text == null) return "";
        return text.replace("#", "");
    }

    /**
     * Checks if a given item id is safe to render inline with #i markup.
     * Equip items that don't exist in the WZ can crash the client.
     * For simplicity, we only show icons for non-equip items.
     */
    private static boolean hasSafeIcon(int itemId) {
        // Equip items (prefix 1xxxxxx) can crash the client if the WZ entry is missing
        // For safety, only show icon for non-equips (use, etc, cash items)
        return itemId / 1000000 != 1;
    }

    // ─── Search ───

    public static int[] findMobs(String query) {
        ensureCaches();
        String normQuery = normalize(query);
        if (normQuery.isEmpty()) return new int[0];

        List<Pair<Integer, String>> matches = new ArrayList<>();
        for (Map.Entry<Integer, String> entry : mobNames.entrySet()) {
            String normName = normalize(entry.getValue());
            if (normName.contains(normQuery)) {
                matches.add(new Pair<>(entry.getKey(), normName));
            }
        }
        matches.sort((a, b) -> {
            boolean aExact = a.getRight().equals(normQuery);
            boolean bExact = b.getRight().equals(normQuery);
            if (aExact != bExact) return aExact ? -1 : 1;
            boolean aStart = a.getRight().startsWith(normQuery);
            boolean bStart = b.getRight().startsWith(normQuery);
            if (aStart != bStart) return aStart ? -1 : 1;
            return Integer.compare(a.getLeft(), b.getLeft());
        });
        int size = Math.min(matches.size(), 500);
        int[] res = new int[size];
        for (int i = 0; i < size; i++) res[i] = matches.get(i).getLeft();
        return res;
    }

    public static int[] findItems(String query) {
        ensureCaches();
        String normQuery = normalize(query);
        if (normQuery.isEmpty()) return new int[0];

        List<Pair<Integer, String>> matches = new ArrayList<>();
        for (Pair<Integer, String> pair : itemNamesNormalized) {
            if (pair.getRight().contains(normQuery)) {
                matches.add(pair);
            }
        }
        matches.sort((a, b) -> {
            boolean aExact = a.getRight().equals(normQuery);
            boolean bExact = b.getRight().equals(normQuery);
            if (aExact != bExact) return aExact ? -1 : 1;
            boolean aStart = a.getRight().startsWith(normQuery);
            boolean bStart = b.getRight().startsWith(normQuery);
            if (aStart != bStart) return aStart ? -1 : 1;
            return Integer.compare(a.getLeft(), b.getLeft());
        });
        int size = Math.min(matches.size(), 500);
        int[] res = new int[size];
        for (int i = 0; i < size; i++) res[i] = matches.get(i).getLeft();
        return res;
    }

    // ─── Page rendering ───

    public static String mainMenu(String notice) {
        StringBuilder sb = new StringBuilder();
        if (notice != null && !notice.isEmpty()) {
            sb.append(notice).append("\r\n\r\n");
        }
        sb.append("What would you like to search for?\r\n\r\n");
        sb.append("#L0##bSearch by item name (who drops it?)#l\r\n");
        sb.append("#L1#Search by monster name (what does it drop?)#k#l\r\n");
        return sb.toString();
    }

    public static String searchPrompt(boolean mobMode) {
        if (mobMode) {
            return "Enter the name of the #bmonster#k you want to look up:";
        } else {
            return "Enter the name of the #bitem#k you want to look up:";
        }
    }

    public static int getResultCount(int[] ids) {
        return ids == null ? 0 : ids.length;
    }

    // ─── Mob list page (for @whatdropsfrom results) ───

    public static int mobListPageCount(int totalMobs) {
        return Math.max(1, (int) Math.ceil((double) totalMobs / MOBS_PER_PAGE));
    }

    public static String mobListPage(String query, int[] mobIds, int page) {
        ensureCaches();
        StringBuilder sb = new StringBuilder();
        sb.append("Monsters matching #b").append(escapeUserText(query)).append("#k:\r\n\r\n");
        int start = page * MOBS_PER_PAGE;
        int end = Math.min(start + MOBS_PER_PAGE, mobIds.length);
        for (int i = start; i < end; i++) {
            int mobId = mobIds[i];
            String name = escapeUserText(mobNames.getOrDefault(mobId, "Unknown"));
            sb.append("#L").append(mobId).append("##b").append(name).append("#k (ID: ").append(mobId).append(")#l\r\n");
        }
        sb.append("\r\n");
        appendPager(sb, page, mobListPageCount(mobIds.length));
        return sb.toString();
    }

    // ─── Item list page (for @whodrops results) ───

    public static int itemListPageCount(int totalItems) {
        return Math.max(1, (int) Math.ceil((double) totalItems / ITEMS_PER_PAGE));
    }

    public static String itemListPage(String query, int[] itemIds, int page) {
        ensureCaches();
        StringBuilder sb = new StringBuilder();
        sb.append("Items matching #b").append(escapeUserText(query)).append("#k:\r\n\r\n");
        int start = page * ITEMS_PER_PAGE;
        int end = Math.min(start + ITEMS_PER_PAGE, itemIds.length);
        for (int i = start; i < end; i++) {
            int itemId = itemIds[i];
            if (hasSafeIcon(itemId)) {
                sb.append("#L").append(itemId).append("##v").append(itemId).append("# #b#z").append(itemId).append("##k#l\r\n");
            } else {
                String name = getItemName(itemId);
                sb.append("#L").append(itemId).append("##b").append(escapeUserText(name)).append("#k (").append(itemId).append(")#l\r\n");
            }
        }
        sb.append("\r\n");
        appendPager(sb, page, itemListPageCount(itemIds.length));
        return sb.toString();
    }

    // ─── Detail: what a mob drops ───

    public static String[] mobDropPages(Character chr, int mobId) {
        ensureCaches();
        List<MonsterDropEntry> drops = MonsterInformationProvider.getInstance().retrieveDrop(mobId);
        List<String> pages = new ArrayList<>();

        String mobName = escapeUserText(mobNames.getOrDefault(mobId, "Unknown"));

        if (drops == null || drops.isEmpty()) {
            pages.add("#b" + mobName + "#k does not drop anything.\r\n\r\n#L10000004#[Back to main menu]#l");
            return pages.toArray(new String[0]);
        }

        boolean isBoss = MonsterInformationProvider.getInstance().isBoss(mobId);

        // Build header
        StringBuilder header = new StringBuilder();
        header.append("#e").append(mobName).append("#n\r\n");
        header.append("Boss: ").append(isBoss ? "Yes" : "No").append("\r\n\r\n");

        // Categorize drops
        List<String> formattedRows = new ArrayList<>();
        List<MonsterDropEntry> equips = new ArrayList<>();
        List<MonsterDropEntry> use = new ArrayList<>();
        List<MonsterDropEntry> etc = new ArrayList<>();
        List<MonsterDropEntry> quest = new ArrayList<>();
        MonsterDropEntry meso = null;

        for (MonsterDropEntry d : drops) {
            if (d.itemId == 0) {
                meso = d;
                continue;
            }
            if (d.questid > 0) {
                quest.add(d);
            } else {
                int prefix = d.itemId / 1000000;
                if (prefix == 1) equips.add(d);
                else if (prefix == 2) use.add(d);
                else etc.add(d);
            }
        }

        if (meso != null && meso.chance > 0) {
            formattedRows.add("#eMesos#n\r\n");
            formattedRows.add("Mesos: " + meso.Minimum + " ~ " + meso.Maximum + "\r\n");
        }

        addSectionRows(formattedRows, "Equipment", equips, chr, isBoss);
        addSectionRows(formattedRows, "USE Items", use, chr, isBoss);
        addSectionRows(formattedRows, "ETC Items", etc, chr, isBoss);
        addSectionRows(formattedRows, "Quest Items", quest, chr, isBoss);

        int totalPages = Math.max(1, (int) Math.ceil((double) formattedRows.size() / DROPS_PER_PAGE));

        for (int p = 0; p < totalPages; p++) {
            StringBuilder sb = new StringBuilder();
            sb.append(header);
            int start = p * DROPS_PER_PAGE;
            int end = Math.min(start + DROPS_PER_PAGE, formattedRows.size());
            for (int i = start; i < end; i++) {
                sb.append(formattedRows.get(i));
            }
            sb.append("\r\n");
            appendPager(sb, p, totalPages);
            sb.append("#L10000004#[Back to main menu]#l\r\n");
            pages.add(sb.toString());
        }

        return pages.toArray(new String[0]);
    }

    // ─── Detail: who drops an item ───

    public static String[] itemDropperPages(int itemId) {
        ensureCaches();
        List<Integer> dropperIds = new ArrayList<>();
        try (Connection con = DatabaseConnection.getConnection();
             PreparedStatement ps = con.prepareStatement("SELECT dropperid FROM drop_data WHERE itemid = ? LIMIT 100")) {
            ps.setInt(1, itemId);
            try (ResultSet rs = ps.executeQuery()) {
                while (rs.next()) {
                    dropperIds.add(rs.getInt("dropperid"));
                }
            }
        } catch (SQLException e) {
            log.error("Failed to load droppers for item {}", itemId, e);
        }

        List<String> pages = new ArrayList<>();
        String itemName = getItemName(itemId);

        if (dropperIds.isEmpty()) {
            pages.add("#b" + escapeUserText(itemName) + "#k is not dropped by any monster.\r\n\r\n#L10000004#[Back to main menu]#l");
            return pages.toArray(new String[0]);
        }

        StringBuilder header = new StringBuilder();
        if (hasSafeIcon(itemId)) {
            header.append("#v").append(itemId).append("# #b#z").append(itemId).append("##k is dropped by:\r\n\r\n");
        } else {
            header.append("#b").append(escapeUserText(itemName)).append("#k is dropped by:\r\n\r\n");
        }

        int totalPages = Math.max(1, (int) Math.ceil((double) dropperIds.size() / SOURCES_PER_PAGE));
        for (int p = 0; p < totalPages; p++) {
            StringBuilder sb = new StringBuilder();
            sb.append(header);
            int start = p * SOURCES_PER_PAGE;
            int end = Math.min(start + SOURCES_PER_PAGE, dropperIds.size());
            for (int i = start; i < end; i++) {
                int mobId = dropperIds.get(i);
                String mobName = escapeUserText(mobNames.getOrDefault(mobId, "Unknown"));
                sb.append("#b").append(mobName).append("#k (").append(mobId).append(")\r\n");
            }
            sb.append("\r\n");
            appendPager(sb, p, totalPages);
            sb.append("#L10000004#[Back to main menu]#l\r\n");
            pages.add(sb.toString());
        }

        return pages.toArray(new String[0]);
    }

    // ─── Helpers ───

    private static void addSectionRows(List<String> rows, String title, List<MonsterDropEntry> entries, Character chr, boolean isBoss) {
        if (entries.isEmpty()) return;
        rows.add("#e" + title + "#n\r\n");
        for (MonsterDropEntry d : entries) {
            rows.add(formatDropRow(chr, d, isBoss));
        }
    }

    private static String formatDropRow(Character chr, MonsterDropEntry entry, boolean mobIsBoss) {
        int dropRate = mobIsBoss ? chr.getBossDropRate() : chr.getDropRate();
        double eff = (double) entry.chance * dropRate;
        long oneIn = Math.max(1, Math.round(1_000_000.0 / eff));
        double pct = eff / 10_000.0;

        String pctStr;
        if (pct >= 100) pctStr = "100%";
        else if (pct >= 10) pctStr = String.format(Locale.US, "%.1f%%", pct);
        else if (pct >= 1) pctStr = String.format(Locale.US, "%.2f%%", pct);
        else if (pct >= 0.01) pctStr = String.format(Locale.US, "%.3f%%", pct);
        else pctStr = String.format(Locale.US, "%.4f%%", pct);

        String name = getItemName(entry.itemId);
        String nameInfo;
        if (hasSafeIcon(entry.itemId)) {
            nameInfo = "#v" + entry.itemId + "# #b#z" + entry.itemId + "##k";
        } else {
            nameInfo = "#b" + escapeUserText(name) + "#k";
        }

        return nameInfo + " - 1/" + oneIn + " (" + pctStr + ")\r\n";
    }

    private static String getItemName(int itemId) {
        // Try our own cache first
        String name = itemNamesOriginal.get(itemId);
        if (name != null) return name;
        // Fallback to IIP
        name = ItemInformationProvider.getInstance().getName(itemId);
        return name != null ? name : "Unknown Item";
    }

    private static void appendPager(StringBuilder sb, int currentPage, int totalPages) {
        if (totalPages <= 1) return;
        sb.append("Page ").append(currentPage + 1).append("/").append(totalPages).append("  ");
        if (currentPage > 0) {
            sb.append("#L10000001#[< Prev]#l ");
        }
        if (currentPage < totalPages - 1) {
            sb.append("#L10000002#[Next >]#l ");
        }
        sb.append("\r\n");
    }
}
