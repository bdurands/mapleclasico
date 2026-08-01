/*
    Slotted bag windows - server side of the CP_BagWindow / LP_BagWindow protocol.
    Each bag (server.OreStorage: ore bag, scroll bag, chair bag, cash bag) is shown
    as a custom scrollable client window instead of the vanilla storage UI. The
    client only REQUESTS; the server validates and is the source of truth, replying
    with a fresh RESP_SNAPSHOT.

    Wire (client -> server, RecvOpcode.BAG_WINDOW):
      byte action  0=OPEN 1=WITHDRAW 2=DEPOSIT 3=MERGE 4=SET_AUTO
      byte bagKind 0=ore 1=scroll 2=chair 3=cash
      OPEN     : (nothing)
      WITHDRAW : short srcSlot                       (bag slot index -> player inventory, auto)
      DEPOSIT  : short srcInvType, short srcInvPos   (player inventory -> bag, merge)
      MERGE    : (nothing)                           (consolidate identical stacks + compact)
      SET_AUTO : byte on                             (Auto button: 1=on 0=off for this bag kind)
    Reply (PacketCreator.bagWindowSnapshot) via SendOpcode.BAG_WINDOW after every action.
*/
package net.server.channel.handlers;

import client.Character;
import client.Client;
import client.inventory.Inventory;
import client.inventory.InventoryType;
import client.inventory.Item;
import client.inventory.ModifyInventory;
import client.inventory.manipulator.InventoryManipulator;
import client.inventory.manipulator.KarmaManipulator;
import net.AbstractPacketHandler;
import net.packet.InPacket;
import server.ItemInformationProvider;
import server.OreStorage;
import tools.PacketCreator;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.List;

public final class BagWindowHandler extends AbstractPacketHandler {

    private static final Logger log = LoggerFactory.getLogger(BagWindowHandler.class);
    private static final int REQ_OPEN = 0, REQ_WITHDRAW = 1, REQ_DEPOSIT = 2, REQ_MERGE = 3, REQ_SET_AUTO = 4, REQ_MOVE = 5;
    private static final String[] BAG_NAME = {"bolsa de minérios", "bolsa de pergaminhos", "bolsa de cadeiras", "bolsa de montarias"};

    @Override
    public void handlePacket(InPacket p, Client c) {
        Character player = c.getPlayer();
        if (player == null) {
            return;
        }
        int action = p.readByte();
        int bagKind = p.readByte();                       // 0=ore 1=scroll 2=chair 3=cash
        if (bagKind < 0 || bagKind > 3) {
            return;
        }
        OreStorage storage = storageFor(player, bagKind);
        if (storage == null) {
            player.dropMessage(1, "A bolsa não está carregada - relogue e tente novamente.");
            return;
        }

        // Check if the player has purchased this bag kind (Quest IDs 30000 to 30003)
        if (player.getQuestStatus(30000 + bagKind) != 2) {
            player.dropMessage(1, "Debes comprar esta bolsa con el admin.");
            c.sendPacket(PacketCreator.bagWindowError());
            return;
        }

        try {
            switch (action) {
                case REQ_OPEN:
                    break;                                // just snapshot, below
                case REQ_WITHDRAW: {
                    int srcSlot = p.readShort();
                    int targetInvSlot = p.readShort();     // inventory slot dropped on (-1 = first free)
                    withdraw(c, player, storage, bagKind, srcSlot, targetInvSlot);
                    break;
                }
                case REQ_DEPOSIT: {
                    int srcInvType = p.readShort();
                    int srcInvPos = p.readShort();
                    int targetBagSlot = p.readShort();     // bag slot dropped on (-1 = first free)
                    deposit(c, player, storage, bagKind, srcInvType, srcInvPos, targetBagSlot);
                    break;
                }
                case REQ_MOVE: {                            // rearrange within the bag (drag slot->slot)
                    int srcSlot = p.readShort();
                    int dstSlot = p.readShort();
                    storage.move(srcSlot, dstSlot, c);
                    break;
                }
                case REQ_MERGE:
                    storage.mergeStacks(c);               // consolidate identical stacks + compact
                    break;                                 // snapshot sent below reflects the merged bag
                case REQ_SET_AUTO: {
                    int on = p.readByte();                // Auto button toggle for this bag kind
                    setAuto(player, bagKind, on != 0);
                    break;                                 // snapshot below echoes the new auto flag
                }
                default:
                    return;
            }
        } catch (Exception e) {
            // never let a malformed request crash the channel packet thread - but log it.
            log.warn("[BagWindow] action={} bagKind={} threw", action, bagKind, e);
        }

        c.sendPacket(PacketCreator.bagWindowSnapshot(bagKind, storage, isAuto(player, bagKind)));
    }

    private static OreStorage storageFor(Character player, int kind) {
        switch (kind) {
            case 1:  return player.getScrollStorage();
            case 2:  return player.getChairStorage();
            case 3:  return player.getMountStorage();
            default: return player.getOreStorage();
        }
    }

    // BAG -> PLAYER. Space check, takeOut of the exact bag slot, then add to inventory honoring the
    // dropped-on inventory slot when possible (else first free + merge).
    private static void withdraw(Client c, Character player, OreStorage storage, int kind, int srcSlot, int targetInvSlot) {
        Item item = storage.getItemAtSlot(srcSlot);
        if (item == null) {
            log.warn("[BagWindow] withdraw slot {} empty", srcSlot);
            return;
        }
        ItemInformationProvider ii = ItemInformationProvider.getInstance();
        if (ii.isPickupRestricted(item.getItemId()) && player.haveItemWithId(item.getItemId(), true)) {
            player.dropMessage(1, "Você já possui esse item único.");
            return;   // unique item the player already owns
        }
        if (!InventoryManipulator.checkSpace(c, item.getItemId(), item.getQuantity(), item.getOwner())) {
            player.dropMessage(1, "Espaço insuficiente no inventário para retirar esse item.");
            log.info("[BagWindow] withdraw blocked: no space for {} x{}", item.getItemId(), item.getQuantity());
            return;   // no inventory space
        }
        if (storage.takeOut(item)) {
            KarmaManipulator.toggleKarmaFlagToUntradeable(item);
            // Roll back into the bag if the inventory rejected it (last free slot taken between the
            // checkSpace pre-check and the add), so a failed withdrawal never destroys the item.
            if (!placeInInventory(c, player, item, targetInvSlot)) {
                storage.store(item);
                player.dropMessage(1, "Não foi possível mover esse item para o inventário.");
                log.warn("[BagWindow] withdraw place rejected {} x{}", item.getItemId(), item.getQuantity());
                return;
            }
            markUsed(player, kind);
            log.info("[BagWindow] withdraw OK: {} x{} (slot {} -> inv {})", item.getItemId(), item.getQuantity(), srcSlot, targetInvSlot);
        } else {
            log.warn("[BagWindow] withdraw takeOut failed for slot {}", srcSlot);
        }
    }

    // Place a withdrawn item into the player inventory. If the player dropped it on a specific, EMPTY,
    // in-range slot of the item's own inventory type, honor that exact slot; otherwise fall back to the
    // normal first-free + merge path. Returns false only if nothing could be placed.
    private static boolean placeInInventory(Client c, Character player, Item item, int targetInvSlot) {
        InventoryType type = item.getInventoryType();
        Inventory inv = player.getInventory(type);
        if (inv != null && targetInvSlot >= 1 && targetInvSlot <= inv.getSlotLimit()) {
            boolean placed = false;
            inv.lockInventory();
            try {
                if (inv.getItem((short) targetInvSlot) == null) {   // exact slot free -> drop it there
                    item.setPosition((short) targetInvSlot);
                    inv.addItemToSlot((short) targetInvSlot, item);
                    placed = true;
                }
            } finally {
                inv.unlockInventory();
            }
            if (placed) {
                c.sendPacket(PacketCreator.modifyInventory(true, java.util.Collections.singletonList(new ModifyInventory(0, item))));
                return true;
            }
        }
        return InventoryManipulator.addFromDrop(c, item, false);   // first free + merge
    }

    // PLAYER -> BAG. Gate by bag-type filter, remove from inventory, store into the bag honoring the
    // dropped-on bag slot (empty slot -> exact placement; same item -> merge; else first free).
    private static void deposit(Client c, Character player, OreStorage storage, int kind,
                               int srcInvType, int srcInvPos, int targetBagSlot) {
        InventoryType type = InventoryType.getByType((byte) srcInvType);
        if (type == null) {
            return;
        }
        Inventory inv = player.getInventory(type);
        if (inv == null || srcInvPos < 1 || srcInvPos > inv.getSlotLimit()) {
            return;
        }
        Item item = inv.getItem((short) srcInvPos);
        if (item == null) {
            return;
        }
        int itemId = item.getItemId();
        if (!Character.bagAccepts(kind, itemId)) {
            player.dropMessage(1, "A " + BAG_NAME[kind] + " não pode guardar esse item!");
            return;
        }
        if (storage.isFull()) {
            return;
        }
        short qty;
        inv.lockInventory();
        try {
            item = inv.getItem((short) srcInvPos);
            if (item == null || item.getItemId() != itemId) {
                return;
            }
            qty = item.getQuantity();        // drag deposits the whole stack
            InventoryManipulator.removeFromSlot(c, type, (short) srcInvPos, qty, false);
            item = item.copy();
        } finally {
            inv.unlockInventory();
        }
        KarmaManipulator.toggleKarmaFlagToUntradeable(item);
        item.setQuantity(qty);
        // Roll back to the inventory if the bag rejected it (filled via a concurrent deposit / bot
        // funnel between the isFull() pre-check and storeAt), so the removed item is never lost.
        if (!storage.storeAt(item, targetBagSlot, c)) {
            InventoryManipulator.addFromDrop(c, item, false);
            player.dropMessage(1, "A " + BAG_NAME[kind] + " está cheia.");
            return;
        }
        markUsed(player, kind);
    }

    private static void markUsed(Character player, int kind) {
        switch (kind) {
            case 1:  player.setUsedScrollStorage(); break;
            case 2:  player.setUsedChairStorage(); break;
            case 3:  player.setUsedMountStorage(); break;
            default: player.setUsedOreStorage(); break;
        }
    }

    // Auto button state for this bag kind (echoed to the client in every snapshot so the
    // button draws lit/greyed correctly).
    private static boolean isAuto(Character player, int kind) {
        switch (kind) {
            case 1:  return player.isAutoScrollStorage();
            case 2:  return player.isAutoChairStorage();
            case 3:  return player.isAutoMountStorage();
            default: return player.isAutoOreStorage();
        }
    }

    private static void setAuto(Character player, int kind, boolean on) {
        switch (kind) {
            case 1:  player.setAutoScrollStorage(on); break;
            case 2:  player.setAutoChairStorage(on); break;
            case 3:  player.setAutoMountStorage(on); break;
            default: player.setAutoOreStorage(on); break;
        }
    }
}
