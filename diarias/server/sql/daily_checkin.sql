-- =====================================================================
-- Daily Check-In: per-character streak state. Run ONCE against your game schema
--   (e.g.  mysql -u root your_schema < daily_checkin.sql).
-- Idempotent: each ADD COLUMN is guarded, so re-running is a no-op.
--
--   checkinDay        INT     - last day claimed this 28-day cycle (0..28)
--   checkinClaimed    INT     - 28-bit mask of days claimed this cycle
--   checkinLastClaim  BIGINT  - epoch millis of the last claim (0 = never); the 24h gate
-- =====================================================================

DELIMITER $$
DROP PROCEDURE IF EXISTS _add_checkin_columns $$
CREATE PROCEDURE _add_checkin_columns()
BEGIN
    IF NOT EXISTS (SELECT 1 FROM information_schema.COLUMNS
                   WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'characters'
                     AND COLUMN_NAME = 'checkinDay') THEN
        ALTER TABLE `characters` ADD COLUMN `checkinDay` int NOT NULL DEFAULT 0;
    END IF;
    IF NOT EXISTS (SELECT 1 FROM information_schema.COLUMNS
                   WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'characters'
                     AND COLUMN_NAME = 'checkinClaimed') THEN
        ALTER TABLE `characters` ADD COLUMN `checkinClaimed` int NOT NULL DEFAULT 0;
    END IF;
    IF NOT EXISTS (SELECT 1 FROM information_schema.COLUMNS
                   WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'characters'
                     AND COLUMN_NAME = 'checkinLastClaim') THEN
        ALTER TABLE `characters` ADD COLUMN `checkinLastClaim` bigint NOT NULL DEFAULT 0;
    END IF;
END $$
CALL _add_checkin_columns() $$
DROP PROCEDURE IF EXISTS _add_checkin_columns $$
DELIMITER ;
