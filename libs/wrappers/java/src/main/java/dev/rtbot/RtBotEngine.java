package dev.rtbot;

import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;

/**
 * Low-level JNI bridge to the RTBot C++ engine.
 *
 * <p>Every method maps 1:1 to a function in {@code rtbot/bindings.h}.
 * All complex data crosses the boundary as JSON strings — identical to
 * the Python and WASM wrappers.
 *
 * <p><b>Thread safety:</b> The underlying RTBot engine is NOT thread-safe
 * for the same program ID. Callers must ensure calls for the same
 * {@code programId} are never concurrent.
 */
public final class RtBotEngine {

    private static final String LIB_NAME = "rtbot_jni";

    static {
        loadNativeLibrary();
    }

    private RtBotEngine() {}

    // -----------------------------------------------------------------
    // Program lifecycle
    // -----------------------------------------------------------------

    /**
     * Create a program from a JSON definition.
     *
     * @param programId   unique program identifier
     * @param programJson RTBot JSON program definition
     * @return empty string on success, or a JSON error string
     */
    public static native String createProgram(String programId, String programJson);

    /**
     * Delete a program and free all associated resources.
     *
     * @param programId program identifier
     * @return result string
     */
    public static native String deleteProgram(String programId);

    // -----------------------------------------------------------------
    // Validation
    // -----------------------------------------------------------------

    /**
     * Validate a program JSON definition.
     *
     * @param programJson RTBot JSON program definition
     * @return JSON string: {@code {"valid": true}} or
     *         {@code {"valid": false, "error": "..."}}
     */
    public static native String validateProgram(String programJson);

    /**
     * Validate a single operator definition.
     *
     * @param type   operator type (e.g. "MovingAverage")
     * @param jsonOp operator JSON definition
     * @return JSON string: {@code {"valid": true}} or
     *         {@code {"valid": false, "error": "..."}}
     */
    public static native String validateOperator(String type, String jsonOp);

    // -----------------------------------------------------------------
    // Message handling (per-sample hot path)
    // -----------------------------------------------------------------

    /**
     * Add a single message to the program's input buffer.
     *
     * @param programId program identifier
     * @param portId    input port (e.g. "i1")
     * @param time      timestamp (monotonically increasing per port)
     * @param value     sample value
     * @return "1" on success
     */
    public static native String addToMessageBuffer(
            String programId, String portId, long time, double value);

    /**
     * Process all buffered messages and return output.
     *
     * @param programId program identifier
     * @return JSON string of output batch:
     *         {@code {"operatorId": {"portId": [{"time": N, "value": V}, ...]}}}
     */
    public static native String processMessageBuffer(String programId);

    /**
     * Process all buffered messages in debug mode
     * (includes all intermediate operator states).
     *
     * @param programId program identifier
     * @return JSON string of debug output batch
     */
    public static native String processMessageBufferDebug(String programId);

    // -----------------------------------------------------------------
    // Batch operations
    // -----------------------------------------------------------------

    /**
     * Process a batch of messages in one call.
     *
     * @param programId program identifier
     * @param times     timestamps array
     * @param values    values array (same length as times)
     * @param ports     port IDs array (same length as times)
     * @return JSON string of output batch
     */
    public static native String processBatch(
            String programId, long[] times, double[] values, String[] ports);

    /**
     * Process a batch of messages in debug mode.
     *
     * @param programId program identifier
     * @param times     timestamps array
     * @param values    values array
     * @param ports     port IDs array
     * @return JSON string of debug output batch
     */
    public static native String processBatchDebug(
            String programId, long[] times, double[] values, String[] ports);

    // -----------------------------------------------------------------
    // State serialization
    // -----------------------------------------------------------------

    /**
     * Serialize the full program state to JSON.
     *
     * @param programId program identifier
     * @return JSON string of serialized program state
     */
    public static native String serializeProgramData(String programId);

    /**
     * Restore a program from a previously serialized JSON state.
     *
     * @param programId program identifier
     * @param jsonState JSON state from {@link #serializeProgramData}
     */
    public static native void restoreProgramDataFromJson(
            String programId, String jsonState);

    // -----------------------------------------------------------------
    // Introspection
    // -----------------------------------------------------------------

    /**
     * Get the entry operator ID for a program.
     *
     * @param programId program identifier
     * @return entry operator ID string
     */
    public static native String getProgramEntryOperatorId(String programId);

    /**
     * Diagnose a program definition.
     *
     * @param programJson RTBot JSON program definition
     * @return JSON string with diagnosis results
     */
    public static native String diagnoseProgram(String programJson);

    // -----------------------------------------------------------------
    // Library loading
    // -----------------------------------------------------------------

    /**
     * Loads the platform-specific native library.
     *
     * <p>Strategy:
     * <ol>
     *   <li>Look for the library as a classpath resource under
     *       {@code /native/<platform>/}. If found, extract to a temp file
     *       and load via {@link System#load(String)}.
     *   <li>Fall back to {@link System#loadLibrary(String)} when the
     *       resource is absent. This covers Bazel test environments where
     *       the library is placed on {@code java.library.path} via the
     *       {@code data} attribute.
     * </ol>
     */
    private static void loadNativeLibrary() {
        String resourcePath = nativeResourcePath();
        try (InputStream in = RtBotEngine.class.getResourceAsStream(resourcePath)) {
            if (in != null) {
                Path tmp = Files.createTempFile("rtbot_jni_", platformSuffix());
                tmp.toFile().deleteOnExit();
                Files.copy(in, tmp, StandardCopyOption.REPLACE_EXISTING);
                System.load(tmp.toAbsolutePath().toString());
            } else {
                System.loadLibrary(LIB_NAME);
            }
        } catch (IOException e) {
            throw new RuntimeException(
                "Failed to load RTBot native library from " + resourcePath, e);
        }
    }

    private static String nativeResourcePath() {
        String os = System.getProperty("os.name", "").toLowerCase();
        String arch = System.getProperty("os.arch", "").toLowerCase();
        String platform;
        if (os.contains("linux")) {
            platform = "linux-" + (arch.contains("aarch64") ? "aarch64" : "x86_64");
        } else if (os.contains("mac")) {
            platform = "mac-" + (arch.contains("aarch64") ? "aarch64" : "x86_64");
        } else if (os.contains("win")) {
            platform = "win-x86_64";
        } else {
            platform = "unknown";
        }
        return "/native/" + platform + "/" + System.mapLibraryName(LIB_NAME);
    }

    private static String platformSuffix() {
        String os = System.getProperty("os.name", "").toLowerCase();
        if (os.contains("win")) return ".dll";
        if (os.contains("mac")) return ".dylib";
        return ".so";
    }
}
