package server;

import client.Character;
import constants.game.GameConstants;
import constants.id.ItemId;
import client.inventory.WeaponType;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import provider.Data;
import provider.DataDirectoryEntry;
import provider.DataFileEntry;
import provider.DataProvider;
import provider.wz.WZFiles;
import server.life.LifeFactory;
import server.life.MonsterDropEntry;
import server.life.MonsterInformationProvider;
import tools.DatabaseConnection;
import tools.Pair;

import java.sql.Connection;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.util.*;

public class DropSearchService {
    private static final Logger log = LoggerFactory.getLogger(DropSearchService.class);

    public static final int MAX_LIST_PAGES = 10;
    private static final int MOBS_PER_PAGE = 8;
    private static final int ITEMS_PER_PAGE = 10;
    private static final int DROPS_PER_PAGE = 12;
    private static final int SOURCES_PER_PAGE = 8;
    private static final int PAGE_LINKS_PER_ROW = 5;

    private static volatile boolean cachesLoaded = false;

    private static final Map<Integer, String> mobNames = new HashMap<>();
    private static final List<Pair<Integer, String>> itemNamesNormalized = new ArrayList<>();
    private static final Map<Integer, Integer> mobCards = new HashMap<>();
    private static final Set<Integer> equipIdsInWz = new HashSet<>();
    private static final Set<Integer> cardArtIds = new HashSet<>();

    private static final Object lock = new Object();

    public static void clearCaches() {
        synchronized (lock) {
            cachesLoaded = false;
            mobNames.clear();
            itemNamesNormalized.clear();
            mobCards.clear();
            equipIdsInWz.clear();
            cardArtIds.clear();
        }
    }

    private static void ensureCaches() {
        if (cachesLoaded) return;
        synchronized (lock) {
            if (cachesLoaded) return;
            loadMobNames();
            loadItemNames();
            loadMobCards();
            loadEquipIds();
            loadCardArts();
            cachesLoaded = true;
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
        for (Pair<Integer, String> pair : ItemInformationProvider.getInstance().itemNameCache) {
            itemNamesNormalized.add(new Pair<>(pair.getLeft(), normalize(pair.getRight())));
        }
    }

    private static void loadMobCards() {
        try (Connection con = DatabaseConnection.getConnection();
             PreparedStatement ps = con.prepareStatement("SELECT mobid, cardid FROM monstercarddata");
             ResultSet rs = ps.executeQuery()) {
            while (rs.next()) {
                mobCards.put(rs.getInt("mobid"), rs.getInt("cardid"));
            }
        } catch (SQLException e) {
            log.error("Failed to load monstercarddata", e);
        }
    }

    private static void loadEquipIds() {
        DataProvider equipData = ItemInformationProvider.getInstance().equipData;
        if (equipData != null) {
            for (DataDirectoryEntry typeDir : equipData.getRoot().getSubdirectories()) {
                for (DataFileEntry file : typeDir.getFiles()) {
                    String name = file.getName();
                    if (name.endsWith(".img")) {
                        try {
                            equipIdsInWz.add(Integer.parseInt(name.substring(0, name.length() - 4)));
                        } catch (NumberFormatException ignored) {}
                    }
                }
            }
        }
    }

    private static void loadCardArts() {
        DataProvider itemData = ItemInformationProvider.getInstance().itemData;
        if (itemData != null) {
            Data consume0238 = itemData.getData("Consume/0238.img");
            if (consume0238 != null) {
                for (Data d : consume0238.getChildren()) {
                    try {
                        int cardId = Integer.parseInt(d.getName());
                        if (d.getChildByPath("info/iconRaw") != null) {
                            cardArtIds.add(cardId);
                        }
                    } catch (NumberFormatException ignored) {}
                }
            }
        }
        
        int missing = 0;
        for (int cardId : mobCards.values()) {
            if (!cardArtIds.contains(cardId)) {
                missing++;
            }
        }
        if (missing > 0) {
            log.warn("{} of {} monstercarddata rows point at a card id with no info/iconRaw in Item.wz/Consume/0238.img", missing, mobCards.size());
        }
    }

    private static String normalize(String s) {
        if (s == null) return "";
        StringBuilder sb = new StringBuilder(s.length());
        for (char c : s.toCharArray()) {
            if (c >= 'A' && c <= 'Z') sb.append((char)(c + 32));
            else if (c >= 'a' && c <= 'z' || c >= '0' && c <= '9') sb.append(c);
        }
        return sb.toString();
    }

    public static String escapeUserText(String text) {
        if (text == null) return "";
        return text.replace("#", "");
    }

    private static Set<Integer> equipIdsInWz() {
        ensureCaches();
        return equipIdsInWz;
    }

    private static boolean hasSafeIcon(int itemId) {
        if (itemId / 1000000 != 1) return true;
        if (!equipIdsInWz().contains(itemId)) return false;
        int prefix = itemId / 10000;
        if (prefix >= 121 && prefix <= 169) {
            return ItemInformationProvider.getInstance().getWeaponType(itemId) != WeaponType.NOT_A_WEAPON;
        }
        return true;
    }

    private static String getMobCardArt(int mobId) {
        ensureCaches();
        Integer cardId = mobCards.get(mobId);
        if (cardId == null || !cardArtIds.contains(cardId)) {
            return "Item/Consume/0238.img/02380000/info/icon";
        }
        return "Item/Consume/0238.img/" + String.format("%08d", cardId) + "/info/iconRaw";
    }

    public static int[] findMobs(String query) {
        ensureCaches();
        String normQuery = normalize(query);
        List<Pair<Integer, String>> matches = new ArrayList<>();
        for (Map.Entry<Integer, String> entry : mobNames.entrySet()) {
            String normName = normalize(entry.getValue());
            if (normName.contains(normQuery)) {
                matches.add(new Pair<>(entry.getKey(), normName));
            }
        }
        matches.sort((a, b) -> {
            boolean aStart = a.getRight().startsWith(normQuery);
            boolean bStart = b.getRight().startsWith(normQuery);
            if (aStart != bStart) return aStart ? -1 : 1;
            boolean aExact = a.getRight().equals(normQuery);
            boolean bExact = b.getRight().equals(normQuery);
            if (aExact != bExact) return aExact ? -1 : 1;
            return a.getLeft().compareTo(b.getLeft());
        });
        int size = Math.min(matches.size(), 2000);
        int[] res = new int[size];
        for (int i = 0; i < size; i++) res[i] = matches.get(i).getLeft();
        return res;
    }

    public static int[] findItems(String query) {
        ensureCaches();
        String normQuery = normalize(query);
        List<Pair<Integer, String>> matches = new ArrayList<>();
        for (Pair<Integer, String> pair : itemNamesNormalized) {
            if (pair.getRight().contains(normQuery)) {
                matches.add(pair);
            }
        }
        matches.sort((a, b) -> {
            boolean aStart = a.getRight().startsWith(normQuery);
            boolean bStart = b.getRight().startsWith(normQuery);
            if (aStart != bStart) return aStart ? -1 : 1;
            boolean aExact = a.getRight().equals(normQuery);
            boolean bExact = b.getRight().equals(normQuery);
            if (aExact != bExact) return aExact ? -1 : 1;
            return a.getLeft().compareTo(b.getLeft());
        });
        int size = Math.min(matches.size(), 2000);
        int[] res = new int[size];
        for (int i = 0; i < size; i++) res[i] = matches.get(i).getLeft();
        return res;
    }

    public static String mainMenu(String notice) {
        StringBuilder sb = new StringBuilder();
        if (notice != null && !notice.isEmpty()) {
            sb.append(notice).append("\r\n\r\n");
        }
        sb.append("What would you like to search for?\r\n");
        sb.append("#L10000006##bMonster (who drops X)#l\r\n");
        sb.append("#L10000007#Item (what drops from X)#k#l\r\n");
        return sb.toString();
    }

    public static String searchPrompt(boolean mobMode) {
        if (mobMode) {
            return "Enter the name of the monster you want to look up:";
        } else {
            return "Enter the name of the item you want to look up:";
        }
    }

    public static int mobListPageCount(int totalMobs) {
        int pages = (int) Math.ceil((double) totalMobs / MOBS_PER_PAGE);
        return Math.min(pages, MAX_LIST_PAGES);
    }

    public static String mobListPage(String query, int[] mobIds, int page) {
        ensureCaches();
        StringBuilder sb = new StringBuilder();
        sb.append("Monsters matching #b").append(escapeUserText(query)).append("#k:\r\n");
        int start = page * MOBS_PER_PAGE;
        int end = Math.min(start + MOBS_PER_PAGE, mobIds.length);
        for (int i = start; i < end; i++) {
            int mobId = mobIds[i];
            String name = escapeUserText(mobNames.getOrDefault(mobId, "Unknown"));
            sb.append("#L").append(mobId).append("##f").append(getMobCardArt(mobId)).append("##b").append(name).append(" (").append(mobId).append(")#k#l\r\n");
        }
        sb.append("\r\n");
        appendPager(sb, page, mobListPageCount(mobIds.length), mobIds.length, true, MOBS_PER_PAGE);
        return sb.toString();
    }

    public static int itemListPageCount(int totalItems) {
        int pages = (int) Math.ceil((double) totalItems / ITEMS_PER_PAGE);
        return Math.min(pages, MAX_LIST_PAGES);
    }

    public static String itemListPage(String query, int[] itemIds, int page) {
        ensureCaches();
        StringBuilder sb = new StringBuilder();
        sb.append("Items matching #b").append(escapeUserText(query)).append("#k:\r\n");
        int start = page * ITEMS_PER_PAGE;
        int end = Math.min(start + ITEMS_PER_PAGE, itemIds.length);
        for (int i = start; i < end; i++) {
            int itemId = itemIds[i];
            String name = escapeUserText(ItemInformationProvider.getInstance().getName(itemId));
            if (hasSafeIcon(itemId)) {
                sb.append("#L").append(itemId).append("##i").append(itemId).append(":# #b#z").append(itemId).append("##k #l\r\n");
            } else {
                sb.append("#L").append(itemId).append("##b").append(name).append(" (").append(itemId).append(")#k#l\r\n");
            }
        }
        sb.append("\r\n");
        appendPager(sb, page, itemListPageCount(itemIds.length), itemIds.length, true, ITEMS_PER_PAGE);
        return sb.toString();
    }

    public static String[] mobDropPages(Character chr, int mobId, boolean withList) {
        ensureCaches();
        List<MonsterDropEntry> drops = MonsterInformationProvider.getInstance().retrieveDrop(mobId);
        List<String> pages = new ArrayList<>();
        
        if (drops == null || drops.isEmpty()) {
            pages.add("This monster does not drop anything.\r\n\r\n#L10000004#[Back to main menu]#l");
            return pages.toArray(new String[0]);
        }
        
        List<MonsterDropEntry> equips = new ArrayList<>();
        List<MonsterDropEntry> use = new ArrayList<>();
        List<MonsterDropEntry> etc = new ArrayList<>();
        List<MonsterDropEntry> quest = new ArrayList<>();
        List<MonsterDropEntry> cards = new ArrayList<>();
        MonsterDropEntry meso = null;

        for (MonsterDropEntry d : drops) {
            if (d.itemId == 0) {
                meso = d;
                continue;
            }
            if (ItemId.isMonsterCard(d.itemId)) {
                cards.add(d);
            } else if (d.questid > 0 || ItemInformationProvider.getInstance().isQuestItem(d.itemId)) {
                quest.add(d);
            } else {
                int prefix = d.itemId / 1000000;
                if (prefix == 1) equips.add(d);
                else if (prefix == 2) use.add(d);
                else etc.add(d);
            }
        }

        StringBuilder header = new StringBuilder();
        String mobName = escapeUserText(mobNames.getOrDefault(mobId, "Unknown"));
        header.append("#f").append(getMobCardArt(mobId)).append("##b").append(mobName).append("#k\r\n");
        boolean isBoss = MonsterInformationProvider.getInstance().isBoss(mobId);
        header.append("Boss: ").append(isBoss ? "Yes" : "No").append("\r\n\r\n");

        if (meso != null) {
            int dropRate = isBoss ? chr.getBossDropRate() : chr.getDropRate();
            double eff = (double) meso.chance * dropRate;
            long oneIn = Math.max(1, Math.round(1_000_000.0 / eff));
            double pct = eff / 10_000.0;
            String pctStr;
            if (pct >= 10) pctStr = String.format("%.1f%%", pct);
            else if (pct >= 1) pctStr = String.format("%.2f%%", pct);
            else if (pct >= 0.01) pctStr = String.format("%.3f%%", pct);
            else pctStr = String.format("%.4f%%", pct);
            header.append("Mesos: ").append((int)(meso.Minimum * chr.getMesoRate())).append(" - ").append((int)(meso.Maximum * chr.getMesoRate())).append(" (1/").append(oneIn).append(", ").append(pctStr).append(")\r\n\r\n");
        }

        List<Pair<String, List<MonsterDropEntry>>> sections = new ArrayList<>();
        if (!equips.isEmpty()) sections.add(new Pair<>("Equipments", equips));
        if (!use.isEmpty()) sections.add(new Pair<>("USE", use));
        if (!etc.isEmpty()) sections.add(new Pair<>("ETC", etc));
        if (!quest.isEmpty()) sections.add(new Pair<>("Quest Items (May require quest active)", quest));
        if (!cards.isEmpty()) sections.add(new Pair<>("Card drop rate", cards));

        List<String> formattedRows = new ArrayList<>();
        for (Pair<String, List<MonsterDropEntry>> section : sections) {
            formattedRows.add("#e" + section.getLeft() + "#n\r\n");
            for (MonsterDropEntry d : section.getRight()) {
                formattedRows.add(formatDropRow(chr, d, isBoss));
            }
        }

        int totalPages = (int) Math.ceil((double) formattedRows.size() / DROPS_PER_PAGE);
        if (totalPages == 0) totalPages = 1;

        for (int p = 0; p < totalPages; p++) {
            StringBuilder sb = new StringBuilder();
            sb.append(header);
            int start = p * DROPS_PER_PAGE;
            int end = Math.min(start + DROPS_PER_PAGE, formattedRows.size());
            for (int i = start; i < end; i++) {
                sb.append(formattedRows.get(i));
            }
            sb.append("\r\n");
            appendPager(sb, p, totalPages, -1, false, DROPS_PER_PAGE);
            if (withList) sb.append("#L10000005#[Back to the results]#l\r\n");
            sb.append("#L10000004#[Back to main menu]#l\r\n");
            pages.add(sb.toString());
        }

        return pages.toArray(new String[0]);
    }

    public static String[] itemDropperPages(Character chr, int itemId, boolean withList) {
        ensureCaches();
        List<Integer> mobIds = new ArrayList<>();
        try (Connection con = DatabaseConnection.getConnection();
             PreparedStatement ps = con.prepareStatement("SELECT dropperid FROM drop_data WHERE itemid = ?")) {
            ps.setInt(1, itemId);
            try (ResultSet rs = ps.executeQuery()) {
                while (rs.next()) {
                    mobIds.add(rs.getInt("dropperid"));
                }
            }
        } catch (SQLException e) {
            log.error("Failed to load droppers for item " + itemId, e);
        }

        List<String> pages = new ArrayList<>();
        if (mobIds.isEmpty()) {
            pages.add("No monsters drop this item.\r\n\r\n#L10000004#[Back to main menu]#l");
            return pages.toArray(new String[0]);
        }

        StringBuilder header = new StringBuilder();
        if (hasSafeIcon(itemId)) {
            header.append("#i").append(itemId).append(":# #b#z").append(itemId).append("##k\r\n\r\n");
        } else {
            header.append("#b").append(escapeUserText(ItemInformationProvider.getInstance().getName(itemId))).append("#k\r\n\r\n");
        }

        int totalPages = (int) Math.ceil((double) mobIds.size() / SOURCES_PER_PAGE);
        for (int p = 0; p < totalPages; p++) {
            StringBuilder sb = new StringBuilder();
            sb.append(header);
            int start = p * SOURCES_PER_PAGE;
            int end = Math.min(start + SOURCES_PER_PAGE, mobIds.size());
            for (int i = start; i < end; i++) {
                int mobId = mobIds.get(i);
                String mobName = escapeUserText(mobNames.getOrDefault(mobId, "Unknown"));
                sb.append("#L").append(mobId).append("##f").append(getMobCardArt(mobId)).append("##b").append(mobName).append(" (").append(mobId).append(")#k#l\r\n");
            }
            sb.append("\r\n");
            appendPager(sb, p, totalPages, -1, false, SOURCES_PER_PAGE);
            if (withList) sb.append("#L10000005#[Back to the results]#l\r\n");
            sb.append("#L10000004#[Back to main menu]#l\r\n");
            pages.add(sb.toString());
        }

        return pages.toArray(new String[0]);
    }

    private static String formatDropRow(Character chr, MonsterDropEntry entry, boolean mobIsBoss) {
        int dropRate = mobIsBoss ? chr.getBossDropRate() : chr.getDropRate();
        double eff = (double) entry.chance * dropRate * chr.getCardRate(entry.itemId);
        if (ItemId.isMonsterCard(entry.itemId)) {
            // eff *= chr.getWorldServer().getCardDropRate(); // method not available
        }
        long oneIn = Math.max(1, Math.round(1_000_000.0 / eff));
        double pct = eff / 10_000.0;
        
        String pctStr;
        if (pct >= 10) pctStr = String.format(Locale.US, "%.1f%%", pct);
        else if (pct >= 1) pctStr = String.format(Locale.US, "%.2f%%", pct);
        else if (pct >= 0.01) pctStr = String.format(Locale.US, "%.3f%%", pct);
        else pctStr = String.format(Locale.US, "%.4f%%", pct);

        String nameInfo;
        if (hasSafeIcon(entry.itemId)) {
            nameInfo = "#i" + entry.itemId + ":# #b#z" + entry.itemId + "##k ";
        } else {
            nameInfo = "#b" + escapeUserText(ItemInformationProvider.getInstance().getName(entry.itemId)) + "#k ";
        }

        return "#L" + entry.itemId + "#" + nameInfo + "- 1/" + oneIn + " (" + pctStr + ")#l\r\n";
    }

    private static void appendPager(StringBuilder sb, int currentPage, int totalPages, int totalItems, boolean isSearchList, int perPage) {
        if (totalPages <= 1) return;
        int startPage = Math.max(0, currentPage - (PAGE_LINKS_PER_ROW / 2));
        int endPage = Math.min(totalPages - 1, startPage + PAGE_LINKS_PER_ROW - 1);
        startPage = Math.max(0, endPage - PAGE_LINKS_PER_ROW + 1);

        for (int i = startPage; i <= endPage; i++) {
            if (i == currentPage) {
                sb.append("#r[").append(i + 1).append("]#k ");
            } else {
                sb.append("#L").append(i).append("#[").append(i + 1).append("]#l ");
            }
        }
        sb.append("\r\n");
        if (isSearchList && totalItems > 0) {
            if (totalPages == MAX_LIST_PAGES && totalItems > totalPages * perPage) {
                sb.append("showing the first ").append(totalPages * perPage).append(" of ").append(totalItems).append(" — narrow your search\r\n");
            }
        }
        if (currentPage > 0) {
            sb.append("#L10000001#[Prev]#l ");
        }
        if (currentPage < totalPages - 1) {
            sb.append("#L10000002#[Next]#l ");
        }
        sb.append("\r\n");
    }
}
