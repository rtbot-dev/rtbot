package dev.rtbot;

import org.junit.Test;
import org.junit.Before;
import org.junit.After;
import static org.junit.Assert.*;

import com.google.gson.Gson;
import com.google.gson.JsonObject;
import java.util.UUID;

/**
 * Integration tests for {@link RtBotEngine}.
 *
 * <p>Requires the native library on {@code java.library.path}.
 * In Bazel, provided via the {@code data} attribute and {@code jvm_flags}
 * on the {@code java_test} target.
 *
 * <p>Program JSON definitions are taken from the RTBot C++ test suite
 * ({@code rtbot/libs/api/test/test_bindings.cpp}).
 */
public class RtBotEngineTest {

    // Pass-through program: Input -> Output
    private static final String PASSTHROUGH_PROGRAM = "{"
        + "\"operators\": ["
        + "  {\"type\": \"Input\",  \"id\": \"in1\",  \"portTypes\": [\"number\"]},"
        + "  {\"type\": \"Output\", \"id\": \"out1\", \"portTypes\": [\"number\"]}"
        + "],"
        + "\"connections\": [{\"from\": \"in1\", \"to\": \"out1\"}],"
        + "\"entryOperator\": \"in1\","
        + "\"output\": {\"out1\": [\"o1\"]}"
        + "}";

    // Moving average (window=3): outputs begin only after 3 samples
    private static final String MOVING_AVERAGE_PROGRAM = "{"
        + "\"operators\": ["
        + "  {\"type\": \"Input\",         \"id\": \"in1\",  \"portTypes\": [\"number\"]},"
        + "  {\"type\": \"MovingAverage\",  \"id\": \"ma1\",  \"window_size\": 3},"
        + "  {\"type\": \"Output\",         \"id\": \"out1\", \"portTypes\": [\"number\"]}"
        + "],"
        + "\"connections\": ["
        + "  {\"from\": \"in1\",  \"to\": \"ma1\",  \"fromPort\": \"o1\", \"toPort\": \"i1\"},"
        + "  {\"from\": \"ma1\",  \"to\": \"out1\", \"fromPort\": \"o1\", \"toPort\": \"i1\"}"
        + "],"
        + "\"entryOperator\": \"in1\","
        + "\"output\": {\"out1\": [\"o1\"]}"
        + "}";

    private String progId;
    private final Gson gson = new Gson();

    @Before
    public void setUp() {
        progId = "test-" + UUID.randomUUID();
    }

    @After
    public void tearDown() {
        try { RtBotEngine.deleteProgram(progId); } catch (RuntimeException ignored) {}
    }

    // -----------------------------------------------------------------
    // createProgram
    // -----------------------------------------------------------------

    @Test
    public void createProgram_validJson_returnsEmpty() {
        String result = RtBotEngine.createProgram(progId, PASSTHROUGH_PROGRAM);
        assertTrue("createProgram should return empty on success, got: " + result,
                   result == null || result.isEmpty());
    }

    @Test
    public void createProgram_invalidJson_returnsError() {
        String result = RtBotEngine.createProgram(progId, "{ not valid }");
        assertNotNull(result);
        assertFalse("should return non-empty error", result.isEmpty());
    }

    // -----------------------------------------------------------------
    // validateProgram
    // -----------------------------------------------------------------

    @Test
    public void validateProgram_validProgram_returnsValid() {
        String result = RtBotEngine.validateProgram(PASSTHROUGH_PROGRAM);
        JsonObject json = gson.fromJson(result, JsonObject.class);
        assertTrue("valid program should validate", json.get("valid").getAsBoolean());
    }

    @Test
    public void validateProgram_invalidProgram_returnsInvalid() {
        String result = RtBotEngine.validateProgram("{\"operators\": []}");
        JsonObject json = gson.fromJson(result, JsonObject.class);
        assertFalse("empty program should not validate", json.get("valid").getAsBoolean());
    }

    // -----------------------------------------------------------------
    // processMessageBuffer — pass-through
    // -----------------------------------------------------------------

    @Test
    public void processMessage_passthrough_returnsOutput() {
        RtBotEngine.createProgram(progId, PASSTHROUGH_PROGRAM);

        String addResult = RtBotEngine.addToMessageBuffer(progId, "i1", 1000L, 42.0);
        assertEquals("1", addResult);

        String output = RtBotEngine.processMessageBuffer(progId);
        assertNotNull(output);

        // Parse: {"out1": {"o1": [{"time": 1000, "value": 42.0}]}}
        JsonObject batch = gson.fromJson(output, JsonObject.class);
        assertTrue("output should contain out1", batch.has("out1"));

        JsonObject out1 = batch.getAsJsonObject("out1");
        assertTrue("out1 should contain o1", out1.has("o1"));

        double value = out1.getAsJsonArray("o1").get(0)
                .getAsJsonObject().get("value").getAsDouble();
        assertEquals(42.0, value, 1e-9);
    }

    // -----------------------------------------------------------------
    // processMessageBuffer — moving average
    // -----------------------------------------------------------------

    @Test
    public void processMessage_movingAverage_noOutputBeforeWindowFills() {
        RtBotEngine.createProgram(progId, MOVING_AVERAGE_PROGRAM);

        RtBotEngine.addToMessageBuffer(progId, "i1", 1000L, 3.0);
        String out1 = RtBotEngine.processMessageBuffer(progId);

        RtBotEngine.addToMessageBuffer(progId, "i1", 2000L, 6.0);
        String out2 = RtBotEngine.processMessageBuffer(progId);

        assertNoOutputValues(out1, "out1", "o1");
        assertNoOutputValues(out2, "out1", "o1");
    }

    @Test
    public void processMessage_movingAverage_correctValue() {
        RtBotEngine.createProgram(progId, MOVING_AVERAGE_PROGRAM);

        RtBotEngine.addToMessageBuffer(progId, "i1", 1000L, 3.0);
        RtBotEngine.processMessageBuffer(progId);
        RtBotEngine.addToMessageBuffer(progId, "i1", 2000L, 6.0);
        RtBotEngine.processMessageBuffer(progId);
        RtBotEngine.addToMessageBuffer(progId, "i1", 3000L, 9.0);
        String out3 = RtBotEngine.processMessageBuffer(progId);

        double avg = extractFirstValue(out3, "out1", "o1");
        assertEquals("moving average of [3, 6, 9] = 6.0", 6.0, avg, 1e-9);
    }

    // -----------------------------------------------------------------
    // Batch processing
    // -----------------------------------------------------------------

    @Test
    public void processBatch_movingAverage_correctValue() {
        RtBotEngine.createProgram(progId, MOVING_AVERAGE_PROGRAM);

        long[] times = {1000L, 2000L, 3000L};
        double[] values = {3.0, 6.0, 9.0};
        String[] ports = {"i1", "i1", "i1"};

        String output = RtBotEngine.processBatch(progId, times, values, ports);

        double avg = extractFirstValue(output, "out1", "o1");
        assertEquals("batch moving average of [3, 6, 9] = 6.0", 6.0, avg, 1e-9);
    }

    // -----------------------------------------------------------------
    // Serialize / Restore
    // -----------------------------------------------------------------

    @Test
    public void serializeAndRestore_stateIsPreserved() {
        RtBotEngine.createProgram(progId, MOVING_AVERAGE_PROGRAM);

        // Feed two samples (window not yet filled)
        RtBotEngine.addToMessageBuffer(progId, "i1", 1000L, 3.0);
        RtBotEngine.processMessageBuffer(progId);
        RtBotEngine.addToMessageBuffer(progId, "i1", 2000L, 6.0);
        RtBotEngine.processMessageBuffer(progId);

        // Checkpoint
        String checkpoint = RtBotEngine.serializeProgramData(progId);
        assertNotNull(checkpoint);
        assertFalse(checkpoint.isEmpty());

        // Destroy and restore — must recreate program before restoring state
        RtBotEngine.deleteProgram(progId);

        String restoredId = progId + "-restored";
        RtBotEngine.createProgram(restoredId, MOVING_AVERAGE_PROGRAM);
        RtBotEngine.restoreProgramDataFromJson(restoredId, checkpoint);

        // Third sample: moving average of [3, 6, 9] = 6.0
        RtBotEngine.addToMessageBuffer(restoredId, "i1", 3000L, 9.0);
        String output = RtBotEngine.processMessageBuffer(restoredId);
        double avg = extractFirstValue(output, "out1", "o1");
        assertEquals(6.0, avg, 1e-9);

        RtBotEngine.deleteProgram(restoredId);
    }

    @Test
    public void serializeAndRestore_deterministicOutput() {
        // Run A: uninterrupted
        String idA = progId + "-a";
        RtBotEngine.createProgram(idA, MOVING_AVERAGE_PROGRAM);
        RtBotEngine.addToMessageBuffer(idA, "i1", 1000L, 3.0);
        RtBotEngine.processMessageBuffer(idA);
        RtBotEngine.addToMessageBuffer(idA, "i1", 2000L, 6.0);
        RtBotEngine.processMessageBuffer(idA);
        RtBotEngine.addToMessageBuffer(idA, "i1", 3000L, 9.0);
        String outA = RtBotEngine.processMessageBuffer(idA);
        RtBotEngine.deleteProgram(idA);

        // Run B: serialize/restore after sample 2
        String idB = progId + "-b";
        RtBotEngine.createProgram(idB, MOVING_AVERAGE_PROGRAM);
        RtBotEngine.addToMessageBuffer(idB, "i1", 1000L, 3.0);
        RtBotEngine.processMessageBuffer(idB);
        RtBotEngine.addToMessageBuffer(idB, "i1", 2000L, 6.0);
        RtBotEngine.processMessageBuffer(idB);
        String snap = RtBotEngine.serializeProgramData(idB);
        RtBotEngine.deleteProgram(idB);
        // Must recreate program before restoring state
        RtBotEngine.createProgram(idB, MOVING_AVERAGE_PROGRAM);
        RtBotEngine.restoreProgramDataFromJson(idB, snap);
        RtBotEngine.addToMessageBuffer(idB, "i1", 3000L, 9.0);
        String outB = RtBotEngine.processMessageBuffer(idB);
        RtBotEngine.deleteProgram(idB);

        double valA = extractFirstValue(outA, "out1", "o1");
        double valB = extractFirstValue(outB, "out1", "o1");
        assertEquals("serialize-restore must be deterministic", valA, valB, 1e-12);
    }

    // -----------------------------------------------------------------
    // deleteProgram
    // -----------------------------------------------------------------

    @Test
    public void processMessage_afterDelete_returnsFalse() {
        RtBotEngine.createProgram(progId, PASSTHROUGH_PROGRAM);
        RtBotEngine.deleteProgram(progId);
        // addToMessageBuffer returns "0" (false) for missing program, not an exception
        String result = RtBotEngine.addToMessageBuffer(progId, "i1", 1000L, 1.0);
        assertEquals("should return 0 (false) for deleted program", "0", result);
    }

    // -----------------------------------------------------------------
    // getProgramEntryOperatorId
    // -----------------------------------------------------------------

    @Test
    public void getProgramEntryOperatorId_returnsCorrectId() {
        RtBotEngine.createProgram(progId, PASSTHROUGH_PROGRAM);
        String entryId = RtBotEngine.getProgramEntryOperatorId(progId);
        assertEquals("in1", entryId);
    }

    // -----------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------

    private void assertNoOutputValues(String outputJson, String operatorId, String portId) {
        JsonObject batch = gson.fromJson(outputJson, JsonObject.class);
        if (!batch.has(operatorId)) return; // no output at all
        JsonObject op = batch.getAsJsonObject(operatorId);
        if (!op.has(portId)) return;
        assertEquals("expected empty output array", 0, op.getAsJsonArray(portId).size());
    }

    private double extractFirstValue(String outputJson, String operatorId, String portId) {
        JsonObject batch = gson.fromJson(outputJson, JsonObject.class);
        assertTrue("output should contain " + operatorId, batch.has(operatorId));
        JsonObject op = batch.getAsJsonObject(operatorId);
        assertTrue(operatorId + " should contain " + portId, op.has(portId));
        assertTrue(portId + " should have at least one message",
                   op.getAsJsonArray(portId).size() > 0);
        return op.getAsJsonArray(portId).get(0)
                .getAsJsonObject().get("value").getAsDouble();
    }
}
