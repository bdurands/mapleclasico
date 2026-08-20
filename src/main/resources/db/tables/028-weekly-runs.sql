CREATE TABLE character_weekly_runs (
    id INT NOT NULL AUTO_INCREMENT,
    characterid INT NOT NULL,
    run_name VARCHAR(50) NOT NULL,
    run_count INT NOT NULL DEFAULT 1,
    last_update TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    UNIQUE KEY uq_char_run (characterid, run_name)
);
