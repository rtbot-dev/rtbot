#include <jni.h>
#include <string>
#include <vector>

#include "rtbot/bindings.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void throw_runtime_exception(JNIEnv* env, const std::string& msg) {
    jclass cls = env->FindClass("java/lang/RuntimeException");
    if (cls) env->ThrowNew(cls, msg.c_str());
}

static std::string jstring_to_std(JNIEnv* env, jstring s) {
    if (!s) return "";
    const char* chars = env->GetStringUTFChars(s, nullptr);
    std::string result(chars);
    env->ReleaseStringUTFChars(s, chars);
    return result;
}

static jstring std_to_jstring(JNIEnv* env, const std::string& s) {
    return env->NewStringUTF(s.c_str());
}

// ---------------------------------------------------------------------------
// JNI implementations — Java class: dev.rtbot.RtBotEngine
//
// Every function maps 1:1 to a function in rtbot/bindings.h.
// All complex data crosses the boundary as JSON strings.
// C++ exceptions are caught and translated to Java RuntimeExceptions.
// ---------------------------------------------------------------------------

extern "C" {

JNIEXPORT jstring JNICALL
Java_dev_rtbot_RtBotEngine_createProgram(JNIEnv* env, jclass, jstring id, jstring programJson) {
    try {
        return std_to_jstring(env, rtbot::create_program(
            jstring_to_std(env, id), jstring_to_std(env, programJson)));
    } catch (const std::exception& e) {
        throw_runtime_exception(env, std::string("createProgram: ") + e.what());
        return nullptr;
    }
}

JNIEXPORT jstring JNICALL
Java_dev_rtbot_RtBotEngine_deleteProgram(JNIEnv* env, jclass, jstring id) {
    try {
        return std_to_jstring(env, rtbot::delete_program(jstring_to_std(env, id)));
    } catch (const std::exception& e) {
        throw_runtime_exception(env, std::string("deleteProgram: ") + e.what());
        return nullptr;
    }
}

JNIEXPORT jstring JNICALL
Java_dev_rtbot_RtBotEngine_validateProgram(JNIEnv* env, jclass, jstring programJson) {
    try {
        return std_to_jstring(env, rtbot::validate_program(jstring_to_std(env, programJson)));
    } catch (const std::exception& e) {
        throw_runtime_exception(env, std::string("validateProgram: ") + e.what());
        return nullptr;
    }
}

JNIEXPORT jstring JNICALL
Java_dev_rtbot_RtBotEngine_validateOperator(JNIEnv* env, jclass, jstring type, jstring jsonOp) {
    try {
        return std_to_jstring(env, rtbot::validate_operator(
            jstring_to_std(env, type), jstring_to_std(env, jsonOp)));
    } catch (const std::exception& e) {
        throw_runtime_exception(env, std::string("validateOperator: ") + e.what());
        return nullptr;
    }
}

JNIEXPORT jstring JNICALL
Java_dev_rtbot_RtBotEngine_addToMessageBuffer(
        JNIEnv* env, jclass, jstring id, jstring portId, jlong time, jdouble value) {
    try {
        return std_to_jstring(env, rtbot::add_to_message_buffer(
            jstring_to_std(env, id), jstring_to_std(env, portId),
            static_cast<uint64_t>(time), value));
    } catch (const std::exception& e) {
        throw_runtime_exception(env, std::string("addToMessageBuffer: ") + e.what());
        return nullptr;
    }
}

JNIEXPORT jstring JNICALL
Java_dev_rtbot_RtBotEngine_processMessageBuffer(JNIEnv* env, jclass, jstring id) {
    try {
        return std_to_jstring(env, rtbot::process_message_buffer(jstring_to_std(env, id)));
    } catch (const std::exception& e) {
        throw_runtime_exception(env, std::string("processMessageBuffer: ") + e.what());
        return nullptr;
    }
}

JNIEXPORT jstring JNICALL
Java_dev_rtbot_RtBotEngine_processMessageBufferDebug(JNIEnv* env, jclass, jstring id) {
    try {
        return std_to_jstring(env, rtbot::process_message_buffer_debug(jstring_to_std(env, id)));
    } catch (const std::exception& e) {
        throw_runtime_exception(env, std::string("processMessageBufferDebug: ") + e.what());
        return nullptr;
    }
}

JNIEXPORT jstring JNICALL
Java_dev_rtbot_RtBotEngine_processBatch(
        JNIEnv* env, jclass, jstring id, jlongArray times, jdoubleArray values, jobjectArray ports) {
    try {
        std::string prog_id = jstring_to_std(env, id);

        // Convert times
        jsize len = env->GetArrayLength(times);
        jlong* time_elems = env->GetLongArrayElements(times, nullptr);
        std::vector<uint64_t> cpp_times(time_elems, time_elems + len);
        env->ReleaseLongArrayElements(times, time_elems, JNI_ABORT);

        // Convert values
        jdouble* val_elems = env->GetDoubleArrayElements(values, nullptr);
        std::vector<double> cpp_values(val_elems, val_elems + len);
        env->ReleaseDoubleArrayElements(values, val_elems, JNI_ABORT);

        // Convert ports
        std::vector<std::string> cpp_ports;
        cpp_ports.reserve(len);
        for (jsize i = 0; i < len; i++) {
            jstring port = (jstring)env->GetObjectArrayElement(ports, i);
            cpp_ports.push_back(jstring_to_std(env, port));
            env->DeleteLocalRef(port);
        }

        return std_to_jstring(env, rtbot::process_batch(prog_id, cpp_times, cpp_values, cpp_ports));
    } catch (const std::exception& e) {
        throw_runtime_exception(env, std::string("processBatch: ") + e.what());
        return nullptr;
    }
}

JNIEXPORT jstring JNICALL
Java_dev_rtbot_RtBotEngine_processBatchDebug(
        JNIEnv* env, jclass, jstring id, jlongArray times, jdoubleArray values, jobjectArray ports) {
    try {
        std::string prog_id = jstring_to_std(env, id);

        jsize len = env->GetArrayLength(times);
        jlong* time_elems = env->GetLongArrayElements(times, nullptr);
        std::vector<uint64_t> cpp_times(time_elems, time_elems + len);
        env->ReleaseLongArrayElements(times, time_elems, JNI_ABORT);

        jdouble* val_elems = env->GetDoubleArrayElements(values, nullptr);
        std::vector<double> cpp_values(val_elems, val_elems + len);
        env->ReleaseDoubleArrayElements(values, val_elems, JNI_ABORT);

        std::vector<std::string> cpp_ports;
        cpp_ports.reserve(len);
        for (jsize i = 0; i < len; i++) {
            jstring port = (jstring)env->GetObjectArrayElement(ports, i);
            cpp_ports.push_back(jstring_to_std(env, port));
            env->DeleteLocalRef(port);
        }

        return std_to_jstring(env, rtbot::process_batch_debug(prog_id, cpp_times, cpp_values, cpp_ports));
    } catch (const std::exception& e) {
        throw_runtime_exception(env, std::string("processBatchDebug: ") + e.what());
        return nullptr;
    }
}

JNIEXPORT jstring JNICALL
Java_dev_rtbot_RtBotEngine_serializeProgramData(JNIEnv* env, jclass, jstring id) {
    try {
        return std_to_jstring(env, rtbot::serialize_program_data(jstring_to_std(env, id)));
    } catch (const std::exception& e) {
        throw_runtime_exception(env, std::string("serializeProgramData: ") + e.what());
        return nullptr;
    }
}

JNIEXPORT void JNICALL
Java_dev_rtbot_RtBotEngine_restoreProgramDataFromJson(JNIEnv* env, jclass, jstring id, jstring jsonState) {
    try {
        rtbot::restore_program_data_from_json(
            jstring_to_std(env, id), jstring_to_std(env, jsonState));
    } catch (const std::exception& e) {
        throw_runtime_exception(env, std::string("restoreProgramDataFromJson: ") + e.what());
    }
}

JNIEXPORT jstring JNICALL
Java_dev_rtbot_RtBotEngine_getProgramEntryOperatorId(JNIEnv* env, jclass, jstring id) {
    try {
        return std_to_jstring(env, rtbot::get_program_entry_operator_id(jstring_to_std(env, id)));
    } catch (const std::exception& e) {
        throw_runtime_exception(env, std::string("getProgramEntryOperatorId: ") + e.what());
        return nullptr;
    }
}

JNIEXPORT jstring JNICALL
Java_dev_rtbot_RtBotEngine_diagnoseProgram(JNIEnv* env, jclass, jstring programJson) {
    try {
        return std_to_jstring(env, rtbot::diagnose_program(jstring_to_std(env, programJson)));
    } catch (const std::exception& e) {
        throw_runtime_exception(env, std::string("diagnoseProgram: ") + e.what());
        return nullptr;
    }
}

} // extern "C"
