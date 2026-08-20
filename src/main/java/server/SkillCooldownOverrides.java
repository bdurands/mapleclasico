package server;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.io.Reader;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import java.util.Properties;

/**
 * Loads and serves custom skill cooldown overrides from {@code skill-cooldowns.properties}.
 * <p>
 * Any skill ID present in that file will have its WZ-defined cooldown replaced by the value
 * specified here (in seconds). Skills not listed fall back to whatever the WZ data says.
 * <p>
 * The file is read once at startup. Call {@link #reload()} at runtime (e.g. from a GM command)
 * to pick up changes without restarting the server.
 */
public final class SkillCooldownOverrides {
    private static final Logger log = LoggerFactory.getLogger(SkillCooldownOverrides.class);
    private static final String FILE_NAME = "skill-cooldowns.properties";

    /** skillId → cooldown in seconds.  Immutable snapshot, safe for concurrent reads. */
    private static volatile Map<Integer, Integer> overrides = Collections.emptyMap();

    static {
        reload();
    }

    private SkillCooldownOverrides() {}

    /**
     * (Re)loads the override map from disk.  Thread-safe: the swap is atomic because the
     * reference is volatile and the map published is a fresh unmodifiable copy.
     */
    public static void reload() {
        Path path = Path.of(FILE_NAME);
        if (!Files.exists(path)) {
            log.info("No {} found – no skill cooldown overrides active.", FILE_NAME);
            overrides = Collections.emptyMap();
            return;
        }
        Properties props = new Properties();
        try (Reader r = Files.newBufferedReader(path)) {
            props.load(r);
        } catch (IOException e) {
            log.error("Failed to load {}: {}", FILE_NAME, e.getMessage());
            return;     // keep previous overrides rather than wiping them
        }

        Map<Integer, Integer> parsed = new HashMap<>();
        for (String key : props.stringPropertyNames()) {
            try {
                int skillId = Integer.parseInt(key.trim());
                int seconds = Integer.parseInt(props.getProperty(key).trim());
                if (seconds < 0) {
                    log.warn("{}: negative cooldown for skill {} ignored", FILE_NAME, skillId);
                    continue;
                }
                parsed.put(skillId, seconds);
            } catch (NumberFormatException e) {
                log.warn("{}: skipping malformed line: {} = {}", FILE_NAME, key, props.getProperty(key));
            }
        }
        overrides = Collections.unmodifiableMap(parsed);
        log.info("Loaded {} skill cooldown override(s) from {}", parsed.size(), FILE_NAME);
    }

    /**
     * Returns the override cooldown for {@code skillId} in seconds, or {@code -1} if no
     * override is configured and the WZ default should be used.
     */
    public static int getOverride(int skillId) {
        Integer v = overrides.get(skillId);
        return v != null ? v : -1;
    }

    /** True when at least one override is loaded (fast path for the common "no overrides" case). */
    public static boolean hasAny() {
        return !overrides.isEmpty();
    }
}
