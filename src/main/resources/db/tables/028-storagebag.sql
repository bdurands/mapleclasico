-- ============================================================================
-- Storage Bag  ->  per-CHARACTER + rename the CASH bag to the MOUNT bag.
-- PRODUCTION-SAFE / IDEMPOTENT: run the whole file once (it can also be re-run).
-- Works on a brand-new DB and on one that already ran the old (per-account) version.
--
-- After this, the 4 bags (ore / scroll / chair / mount) behave exactly like the normal
-- inventory: their items live in `inventoryitems` keyed by `characterid` + `type`
--   type 10 = ore bag, 11 = scroll bag, 12 = chair bag, 13 = mount bag
-- (no metadata table; capacity is a fixed 200 slots).
--
-- The 4 auto-collect toggles stay ON `characters` (per character), just renamed cash -> mount:
--   autoOreStorage, autoScrollStorage, autoChairStorage, autoMountStorage.
-- NOTE: run this file as ONE script (it uses DELIMITER for the migration procedure).
-- ============================================================================

DELIMITER $$

DROP PROCEDURE IF EXISTS bagstorage_migrate $$
CREATE PROCEDURE bagstorage_migrate()
BEGIN
    DECLARE has_ore, has_scroll, has_chair, has_cash, has_mount INT DEFAULT 0;

    SELECT COUNT(*) INTO has_ore    FROM information_schema.COLUMNS
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'characters' AND COLUMN_NAME = 'autoOreStorage';
    SELECT COUNT(*) INTO has_scroll FROM information_schema.COLUMNS
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'characters' AND COLUMN_NAME = 'autoScrollStorage';
    SELECT COUNT(*) INTO has_chair  FROM information_schema.COLUMNS
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'characters' AND COLUMN_NAME = 'autoChairStorage';
    SELECT COUNT(*) INTO has_cash   FROM information_schema.COLUMNS
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'characters' AND COLUMN_NAME = 'autoCashStorage';
    SELECT COUNT(*) INTO has_mount  FROM information_schema.COLUMNS
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'characters' AND COLUMN_NAME = 'autoMountStorage';

    -- ore / scroll / chair: create the toggle column if it's missing (fresh DB).
    IF has_ore = 0 THEN
        ALTER TABLE `characters` ADD COLUMN `autoOreStorage`    TINYINT(1) NOT NULL DEFAULT 0;
    END IF;
    IF has_scroll = 0 THEN
        ALTER TABLE `characters` ADD COLUMN `autoScrollStorage` TINYINT(1) NOT NULL DEFAULT 0;
    END IF;
    IF has_chair = 0 THEN
        ALTER TABLE `characters` ADD COLUMN `autoChairStorage`  TINYINT(1) NOT NULL DEFAULT 0;
    END IF;

    -- 4th toggle: rename autoCashStorage -> autoMountStorage, or create it fresh,
    -- or (if both somehow exist) drop the stale cash one.
    IF has_mount = 0 AND has_cash = 1 THEN
        ALTER TABLE `characters` CHANGE COLUMN `autoCashStorage` `autoMountStorage` TINYINT(1) NOT NULL DEFAULT 0;
    ELSEIF has_mount = 0 THEN
        ALTER TABLE `characters` ADD COLUMN `autoMountStorage` TINYINT(1) NOT NULL DEFAULT 0;
    ELSEIF has_cash = 1 THEN
        ALTER TABLE `characters` DROP COLUMN `autoCashStorage`;
    END IF;
END $$

DELIMITER ;

CALL bagstorage_migrate();
DROP PROCEDURE IF EXISTS bagstorage_migrate;

-- Old per-ACCOUNT bag items only: they were stored in `inventoryitems` with characterid = NULL
-- (keyed by accountid = the old orestorages storageid). Clearing just those NULL rows migrates
-- cleanly WITHOUT touching any per-character bag items (characterid IS NOT NULL) -> safe post-deploy.
DELETE FROM `inventoryitems` WHERE `type` IN (10, 11, 12, 13) AND `characterid` IS NULL;

-- Old per-account metadata table is no longer used (slots are fixed at 200, keyed by character).
DROP TABLE IF EXISTS `orestorages`;
