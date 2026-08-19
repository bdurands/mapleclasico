package server;

import client.Client;
import client.inventory.manipulator.InventoryManipulator;

/**
 * Daily Check-In reward table — THE ONE FILE YOU EDIT to define what each of the 28 days grants.
 *
 * ============================== MOCK / SAMPLE DATA ==============================
 * Out of the box every day grants 1 meso, with a generic potion icon (2000000) so the window
 * renders. Replace the DAYS table below with your own rewards. Each day supports:
 *   - iconItemId : the item whose icon is drawn in that day's slot (sent to the client). Must be a
 *                  REAL item id with an icon. For meso-only days, pick any representative item.
 *   - mesos      : mesos granted on claim (0 = none).
 *   - grants     : items granted on claim (id, qty, optional expireDays for timed cash items).
 *   - slotType   : 0 = none, else an inventory type to expand on claim (1=Equip 2=Use 3=Set-up
 *                  4=Etc 5=Cash) by slotCount slots, via Character.gainSlots.
 *
 * The tooltip the client shows on hover is built from this table (item names via
 * ItemInformationProvider.getName + mesos/slot lines).
 * ===============================================================================
 */
public final class DailyCheckinRewards {

    private static final long ONE_DAY_MS = 86_400_000L;
    public static final int CYCLE_DAYS = 28;
    /** Characters below this level can't see or use the Daily Check-In window. */
    public static final int MIN_LEVEL = 10;

    /** A single item grant. expireDays 0 = permanent; >0 = expires that many days from claim. */
    public record Grant(int itemId, int qty, int expireDays) {
        public Grant(int itemId, int qty) {
            this(itemId, qty, 0);
        }
    }

    /** One day's reward. */
    public record Reward(int iconItemId, int mesos, Grant[] grants, int slotType, int slotCount) {
        Reward(int iconItemId, int mesos, Grant[] grants) {
            this(iconItemId, mesos, grants, 0, 0);
        }
    }

    private static final Grant[] NONE = new Grant[0];

    // Placeholder icon for the mock (a basic potion, present in every v83 client). Replace per day.
    private static final int PLACEHOLDER_ICON = 2000000;

    // -------- EDIT THIS TABLE -------- (index 0 = Day 1 ... index 27 = Day 28)
    private static final Reward[] DAYS;
    static {
        DAYS = new Reward[CYCLE_DAYS];
        
        // TODO: Por favor, cambia '2340000' por el ID real de tu "Ellin Scroll" en los días 21 y 28.

        // Bloque 1 (Dias 1 - 7)
        DAYS[0] = new Reward(2000005, 0, new Grant[]{ new Grant(2000005, 50) });          // Power Elixir x50
        DAYS[1] = new Reward(2000000, 0, NONE, 1, 4);                                     // 4 espacios equip
        DAYS[2] = new Reward(5041000, 0, new Grant[]{ new Grant(5041000, 1, 3) });        // Vip teleport rock x1 temporal por 3 dias
        DAYS[3] = new Reward(5000003, 0, new Grant[]{ new Grant(5000003, 1, 7) });        // Pet temporal por 1 semana
        DAYS[4] = new Reward(5130000, 0, new Grant[]{ new Grant(5130000, 1) });           // Safety charm x1
        DAYS[5] = new Reward(2022179, 0, new Grant[]{ new Grant(2022179, 1) });           // Onyx apple x1
        DAYS[6] = new Reward(5510000, 0, new Grant[]{ new Grant(5510000, 1) });           // Wheel of Destiny x1

        // Bloque 2 (Dias 8 - 14)
        DAYS[7] = new Reward(2000005, 0, new Grant[]{ new Grant(2000005, 80) });          // Power Elixir x80
        DAYS[8] = new Reward(2000000, 0, NONE, 2, 4);                                     // 4 espacios use
        DAYS[9] = new Reward(5041000, 0, new Grant[]{ new Grant(5041000, 1, 3) });        // Vip teleport rock x1 temporal por 3 dias
        DAYS[10] = new Reward(4037000, 0, new Grant[]{ new Grant(4037000, 3) });          // Ellin coin x3
        DAYS[11] = new Reward(5130000, 0, new Grant[]{ new Grant(5130000, 2) });          // Safety charm x2
        DAYS[12] = new Reward(2022306, 0, new Grant[]{ new Grant(2022306, 2) });          // Primal Brew x2
        DAYS[13] = new Reward(2022307, 0, new Grant[]{ new Grant(2022307, 2) });          // Spirit Herbs x2

        // Bloque 3 (Dias 15 - 21)
        DAYS[14] = new Reward(2000005, 0, new Grant[]{ new Grant(2000005, 100) });        // Power Elixir x100
        DAYS[15] = new Reward(2000000, 0, NONE, 3, 4);                                    // 4 espacios setup
        DAYS[16] = new Reward(5041000, 0, new Grant[]{ new Grant(5041000, 1, 3) });       // Vip teleport rock x1 temporal por 3 dias
        DAYS[17] = new Reward(4037000, 0, new Grant[]{ new Grant(4037000, 4) });          // Ellin coin x4
        DAYS[18] = new Reward(5130000, 0, new Grant[]{ new Grant(5130000, 2) });          // Safety charm x2
        DAYS[19] = new Reward(2022179, 0, new Grant[]{ new Grant(2022179, 2) });          // Onyx apple x2
        DAYS[20] = new Reward(2049099, 0, new Grant[]{ new Grant(2049099, 1) });            // Ellin scroll x1

        // Bloque 4 (Dias 22 - 28)
        DAYS[21] = new Reward(2000005, 0, new Grant[]{ new Grant(2000005, 200) });        // Power Elixir x200
        DAYS[22] = new Reward(2000000, 0, NONE, 4, 4);                                    // 4 espacios etc
        DAYS[23] = new Reward(5041000, 0, new Grant[]{ new Grant(5041000, 1, 3) });       // Vip teleport rock x1 temporal por 3 dias
        DAYS[24] = new Reward(5510000, 0, new Grant[]{ new Grant(5510000, 1) });          // Wheel of Destiny x1
        DAYS[25] = new Reward(4037000, 0, new Grant[]{ new Grant(4037000, 4) });          // Ellin Coin x4
        DAYS[26] = new Reward(2022179, 0, new Grant[]{ new Grant(2022179, 5) });          // Onyx apple x5
        DAYS[27] = new Reward(2049099, 0, new Grant[]{ new Grant(2049099, 2) });        // Ellin Scroll x2
    }

    private DailyCheckinRewards() {
    }

    private static String slotTabName(int t) {
        switch (t) {
            case 1:  return "Equip";
            case 2:  return "Use";
            case 3:  return "Set-up";
            case 4:  return "Etc";
            case 5:  return "Cash";
            default: return "";
        }
    }

    /** Item id whose icon the client draws for {@code day} (1-based), or 0 if out of range. */
    public static int iconItemId(int day) {
        if (day < 1 || day > DAYS.length) {
            return 0;
        }
        return DAYS[day - 1].iconItemId();
    }

    /**
     * Grant the full reward for {@code day} (1-based). Best-effort: each piece is granted
     * independently. Returns false only if the day is out of range.
     */
    public static boolean grantDay(Client c, int day) {
        if (day < 1 || day > DAYS.length) {
            return false;
        }
        Reward r = DAYS[day - 1];
        if (r.mesos() > 0) {
            c.getPlayer().gainMeso(r.mesos(), true);
        }
        for (Grant g : r.grants()) {
            long expiration = g.expireDays() > 0 ? System.currentTimeMillis() + g.expireDays() * ONE_DAY_MS : -1;
            InventoryManipulator.addById(c, g.itemId(), (short) g.qty(), expiration);
        }
        if (r.slotType() > 0 && r.slotCount() > 0) {
            c.getPlayer().gainSlots(r.slotType(), r.slotCount());
        }
        return true;
    }

    /** Human-readable multi-line breakdown for {@code day}, used as the client's hover tooltip. */
    public static String tooltip(int day) {
        if (day < 1 || day > DAYS.length) {
            return "";
        }
        Reward r = DAYS[day - 1];
        ItemInformationProvider ii = ItemInformationProvider.getInstance();
        StringBuilder sb = new StringBuilder();
        sb.append("Day ").append(day).append('\n');
        for (Grant g : r.grants()) {
            String name = ii.getName(g.itemId());
            if (name == null || name.isBlank()) {
                name = "Item " + g.itemId();
            }
            sb.append("- ").append(name);
            if (g.qty() > 1) {
                sb.append(" x").append(g.qty());
            }
            if (g.expireDays() > 0) {
                sb.append(" (").append(g.expireDays()).append(g.expireDays() == 1 ? " day)" : " days)");
            }
            sb.append('\n');
        }
        if (r.mesos() > 0) {
            sb.append("- ").append(String.format("%,d", r.mesos())).append(" mesos\n");
        }
        if (r.slotType() > 0 && r.slotCount() > 0) {
            sb.append("- +").append(r.slotCount()).append(' ').append(slotTabName(r.slotType())).append(" slots\n");
        }
        int len = sb.length();
        if (len > 0 && sb.charAt(len - 1) == '\n') {
            sb.setLength(len - 1);
        }
        return sb.toString();
    }
}
