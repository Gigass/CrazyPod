#include "config.h"

#if defined(IPOD_6G) && \
    (defined(SIMULATOR) || \
     defined(CRAZYPOD_REPRO_DIAGNOSTICS))

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "button.h"
#include "core_alloc.h"
#include "dir.h"
#include "file.h"
#include "kernel.h"
#include "system.h"

#include "../../crazypod_frameclock.h"
#include "../../crazypod_miniapps.h"
#include "../../miniapps/runtime/crazypod_miniapp_host_system.h"
#include "../features/miniapps/crazypod_miniapps_feature.h"
#include "../navigation/crazypod_ui_routes.h"
#include "crazypod_app_launcher.h"
#include "crazypod_miniapp_repro.h"

#define REPRO_CYCLES 5
#define REPRO_MOVES 32
#define REPRO_TRACE_CAPACITY 4096
#define REPRO_LATENCY_CAPACITY (REPRO_CYCLES * REPRO_MOVES)
#define REPRO_MOVE_INTERVAL ((HZ * 160 / 1000) > 0 \
    ? (HZ * 160 / 1000) : 1)
#define REPRO_CLICK_RELEASE ((HZ * 20 / 1000) > 0 \
    ? (HZ * 20 / 1000) : 1)
#define REPRO_RESPONSE_LIMIT ((HZ * 300 / 1000) > 0 \
    ? (HZ * 300 / 1000) : 1)
#define REPRO_P95_LIMIT ((HZ * 180 / 1000) > 0 \
    ? (HZ * 180 / 1000) : 1)
#define REPRO_HEARTBEAT_LIMIT ((HZ * 250 / 1000) > 0 \
    ? (HZ * 250 / 1000) : 1)
#define REPRO_DIRECTORY "/.crazypod/repro"
#define REPRO_TRIGGER_PATH REPRO_DIRECTORY "/run.flag"
#define REPRO_GAME_TRIGGER_PATH \
    REPRO_DIRECTORY "/run-game.flag"
#define REPRO_LAB_TRIGGER_PATH \
    REPRO_DIRECTORY "/run-lab.flag"
#define REPRO_GAME_BOOST_TRIGGER_PATH \
    REPRO_DIRECTORY "/run-game-boost.flag"
#define REPRO_LAB_BOOST_TRIGGER_PATH \
    REPRO_DIRECTORY "/run-lab-boost.flag"
#define REPRO_RUNNING_PATH REPRO_DIRECTORY "/running.flag"
#ifndef SIMULATOR
#define REPRO_WATCHDOG_LIMIT (2 * HZ)
#define REPRO_WATCHDOG_STACK_SIZE \
    (DEFAULT_STACK_SIZE + 0x1000)
#endif

enum repro_phase {
    REPRO_WAIT_LIST = 0,
    REPRO_MOVE_TO_GAME,
    REPRO_OPEN_GAME,
    REPRO_ENTER_GAME,
    REPRO_OPEN_PAUSE,
    REPRO_SELECT_RESTART,
    REPRO_OPEN_RESTART_CONFIRM,
    REPRO_SELECT_RESTART_YES,
    REPRO_CONFIRM_RESTART,
    REPRO_MOVES_BEGIN,
    REPRO_MOVES_ACTIVE,
    REPRO_GAME_EXIT_HOLD,
    REPRO_GAME_EXIT_RIGHT,
    REPRO_GAME_EXIT_SELECT,
    REPRO_MOVE_TO_LAB,
    REPRO_OPEN_LAB,
    REPRO_LAB_WHEEL,
    REPRO_LAB_SELECT,
    REPRO_LAB_BACK,
    REPRO_LAB_EXIT_HOLD,
    REPRO_LAB_EXIT_RIGHT,
    REPRO_LAB_EXIT_SELECT,
};

enum repro_scenario {
    REPRO_SCENARIO_FULL = 0,
    REPRO_SCENARIO_GAME,
    REPRO_SCENARIO_LAB,
    REPRO_SCENARIO_NONE,
};

struct repro_trace {
    long tick;
    const char *phase;
    long value;
    int route;
    int app;
    int button_queue;
    unsigned miniapp_queue;
    uint32_t present_sequence;
    uint32_t framebuffer_crc;
    size_t host_used;
    size_t core_free;
    long cpu_frequency_hz;
};

struct repro_action {
    bool active;
    bool release;
    bool release_sent;
    bool require_frame;
    long button;
    long started;
    long release_due;
    long deadline;
    uint32_t start_sequence;
    uint32_t start_crc;
    const char *label;
};

static const char *const phase_names[] = {
    "wait-list",
    "move-to-game",
    "open-game",
    "enter-game",
    "open-pause",
    "select-restart",
    "open-restart-confirm",
    "select-restart-yes",
    "confirm-restart",
    "moves-begin",
    "moves-active",
    "game-exit-hold",
    "game-exit-right",
    "game-exit-select",
    "move-to-lab",
    "open-lab",
    "lab-wheel",
    "lab-select",
    "lab-back",
    "lab-exit-hold",
    "lab-exit-right",
    "lab-exit-select",
};

static struct {
    bool active;
    bool interaction_started;
    enum repro_phase phase;
    long phase_started;
    long last_service;
    long last_heartbeat_trace;
    long max_heartbeat;
    long max_interactive_heartbeat;
    unsigned cycle;
    unsigned target_cycles;
    enum repro_scenario scenario;
    bool cpu_boost_requested;
    long min_cpu_frequency_hz;
    long max_cpu_frequency_hz;
    unsigned move_count;
    unsigned move_visible;
    unsigned move_misses;
    unsigned max_button_queue;
    unsigned max_miniapp_queue;
    unsigned violation_count;
    const char *first_violation;
    long next_move;
    long release_due;
    long release_button;
    bool release_pending;
    long pending_moves[REPRO_MOVES];
    unsigned pending_count;
    long latencies[REPRO_LATENCY_CAPACITY];
    unsigned latency_count;
    unsigned cycle_latency_start;
    long max_cycle_p95;
    long max_action_latency;
    long max_move_latency;
    long hold_started;
    bool hold_violation_reported;
    uint32_t hold_sequence;
    uint32_t hold_crc;
    uint32_t observed_sequence;
    uint32_t observed_crc;
    struct repro_action action;
    struct repro_trace trace[REPRO_TRACE_CAPACITY];
    unsigned trace_count;
} repro;

#ifndef SIMULATOR
static long repro_watchdog_stack[
    REPRO_WATCHDOG_STACK_SIZE / sizeof(long)];
static volatile bool repro_watchdog_active;
static volatile long repro_watchdog_heartbeat;
#endif

static int current_route_id(void)
{
    const struct route_state *route =
        crazypod_ui_routes_current();

    return route != NULL ? (int)route->route : -1;
}

static const char *current_app_id(void)
{
    int index = crazypod_miniapps_current();
    const struct crazypod_miniapp_metadata *metadata =
        index >= 0 ? crazypod_miniapps_metadata(index) : NULL;

    return metadata != NULL ? metadata->id : NULL;
}

static bool app_is(const char *id)
{
    const char *current = current_app_id();

    return current != NULL && strcmp(current, id) == 0;
}

static const char *scenario_name(void)
{
    switch(repro.scenario) {
    case REPRO_SCENARIO_GAME:
        return "game2048";
    case REPRO_SCENARIO_LAB:
        return "capability-lab";
    case REPRO_SCENARIO_FULL:
    default:
        return "full";
    }
}

static uint32_t framebuffer_crc(void)
{
#ifdef SIMULATOR
    return crazypod_present_framebuffer_crc();
#else
    return 0;
#endif
}

static long current_cpu_frequency_hz(void)
{
#ifdef SIMULATOR
    return 0;
#else
    return cpu_frequency;
#endif
}

static void observe_cpu_frequency(long frequency)
{
    if(frequency <= 0)
        return;
    if(repro.min_cpu_frequency_hz == 0 ||
       frequency < repro.min_cpu_frequency_hz)
        repro.min_cpu_frequency_hz = frequency;
    if(frequency > repro.max_cpu_frequency_hz)
        repro.max_cpu_frequency_hz = frequency;
}

static void trace_event(
    const char *phase, long value)
{
    struct repro_trace *entry;
    int app = crazypod_miniapps_current();
    int buttons = button_queue_count();
    unsigned miniapps =
        crazypod_miniapps_feature_input_count();

    if((unsigned)buttons > repro.max_button_queue)
        repro.max_button_queue = (unsigned)buttons;
    if(miniapps > repro.max_miniapp_queue)
        repro.max_miniapp_queue = miniapps;
    if(repro.trace_count >= REPRO_TRACE_CAPACITY)
        return;
    entry = &repro.trace[repro.trace_count++];
    entry->tick = current_tick;
    entry->phase = phase;
    entry->value = value;
    entry->route = current_route_id();
    entry->app = app;
    entry->button_queue = buttons;
    entry->miniapp_queue = miniapps;
    entry->present_sequence =
        crazypod_present_sequence();
    entry->framebuffer_crc =
        framebuffer_crc();
    entry->host_used =
        crazypod_miniapp_host_memory_used();
    entry->core_free = core_available();
    entry->cpu_frequency_hz =
        current_cpu_frequency_hz();
    observe_cpu_frequency(entry->cpu_frequency_hz);
}

void crazypod_miniapp_repro_trace_marker(
    const char *phase, long value)
{
    struct repro_trace *entry;

    if(!repro.active || phase == NULL ||
       repro.trace_count >= REPRO_TRACE_CAPACITY)
        return;
    entry = &repro.trace[repro.trace_count++];
    memset(entry, 0, sizeof(*entry));
    entry->tick = current_tick;
    entry->phase = phase;
    entry->value = value;
    entry->route = -2;
    entry->app = -2;
    entry->button_queue = -1;
    entry->present_sequence =
        crazypod_present_sequence();
    entry->cpu_frequency_hz =
        current_cpu_frequency_hz();
    observe_cpu_frequency(entry->cpu_frequency_hz);
}

static void write_all(int file, const char *text, size_t size)
{
    while(size > 0) {
        ssize_t count = write(file, text, size);

        if(count <= 0)
            return;
        text += count;
        size -= (size_t)count;
    }
}

static long latency_percentile95_range(unsigned start, unsigned count)
{
    long values[REPRO_LATENCY_CAPACITY];
    unsigned i;
    unsigned j;

    if(start > repro.latency_count ||
       count > repro.latency_count - start || count == 0)
        return 0;
    memcpy(
        values, repro.latencies + start,
        count * sizeof(values[0]));
    for(i = 1; i < count; ++i) {
        long value = values[i];

        j = i;
        while(j > 0 && values[j - 1] > value) {
            values[j] = values[j - 1];
            --j;
        }
        values[j] = value;
    }
    i = (count * 95u + 99u) / 100u;
    if(i == 0)
        i = 1;
    return values[i - 1u];
}

static long latency_percentile95(void)
{
    return latency_percentile95_range(0, repro.latency_count);
}

static void write_results(bool passed, const char *reason)
{
    static const char summary_format[] =
        "{\n"
        "  \"status\":\"%s\",\n"
        "  \"reason\":\"%s\",\n"
        "  \"scenario\":\"%s\",\n"
        "  \"phase\":\"%s\",\n"
        "  \"cycles\":%u,\n"
        "  \"moveSamples\":%u,\n"
        "  \"moveP95Ms\":%ld,\n"
        "  \"worstCycleP95Ms\":%ld,\n"
        "  \"moveMaxMs\":%ld,\n"
        "  \"actionMaxMs\":%ld,\n"
        "  \"heartbeatMaxMs\":%ld,\n"
        "  \"allPhaseHeartbeatMaxMs\":%ld,\n"
        "  \"buttonQueueMax\":%u,\n"
        "  \"miniappQueueMax\":%u,\n"
        "  \"cpuBoostRequested\":%s,\n"
        "  \"cpuFrequencyMinHz\":%ld,\n"
        "  \"cpuFrequencyMaxHz\":%ld,\n"
        "  \"violationCount\":%u,\n"
        "  \"traceRecords\":%u\n"
        "}\n";
    static const char environment_format[] =
#ifdef SIMULATOR
        "target=ipod6g-simulator\n"
#else
        "target=ipod6g-hardware\n"
#endif
        "hz=%d\n"
        "cycles=%u\n"
        "scenario=%s\n"
        "cpuBoostRequested=%s\n"
#ifndef SIMULATOR
        "cpuFrequencyNormalHz=%ld\n"
        "cpuFrequencyMaxHz=%ld\n"
#endif
        "movesPerCycle=%d\n"
        "inputPath=button_queue\n"
        "displayCriterion="
#ifdef SIMULATOR
        "present-sequence-plus-framebuffer-crc\n";
#else
        "hardware-lcd-present-sequence\n";
#endif
    static const char status_pass[] = "PASS";
    static const char status_fail[] = "FAIL";
    static const char boolean_true[] = "true";
    static const char boolean_false[] = "false";
    char line[512];
    int file;
    unsigned i;
    long p95 = latency_percentile95();

    mkdir("/.crazypod");
    mkdir(REPRO_DIRECTORY);
    file = open(
        REPRO_DIRECTORY "/trace.csv",
        O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(file >= 0) {
        static const char header[] =
            "tick,phase,value,route,app,button_queue,miniapp_queue,"
            "present_sequence,framebuffer_crc,host_used,core_free,"
            "cpu_frequency_hz\n";

        write_all(file, header, sizeof(header) - 1u);
        for(i = 0; i < repro.trace_count; ++i) {
            const struct repro_trace *entry = &repro.trace[i];
            int length = snprintf(
                line, sizeof(line),
                "%ld,%s,%ld,%d,%d,%d,%u,%lu,%08lx,%lu,%lu,%ld\n",
                entry->tick, entry->phase, entry->value,
                entry->route, entry->app, entry->button_queue,
                entry->miniapp_queue,
                (unsigned long)entry->present_sequence,
                (unsigned long)entry->framebuffer_crc,
                (unsigned long)entry->host_used,
                (unsigned long)entry->core_free,
                entry->cpu_frequency_hz);

            if(length > 0)
                write_all(
                    file, line,
                    (size_t)length < sizeof(line)
                        ? (size_t)length : sizeof(line) - 1u);
        }
        close(file);
    }

    file = open(
        REPRO_DIRECTORY "/summary.json",
        O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(file >= 0) {
        int length = snprintf(
            line, sizeof(line), summary_format,
            passed ? status_pass : status_fail, reason,
            scenario_name(),
            phase_names[repro.phase],
            repro.cycle, repro.latency_count,
            p95 * 1000L / HZ,
            repro.max_cycle_p95 * 1000L / HZ,
            repro.max_move_latency * 1000L / HZ,
            repro.max_action_latency * 1000L / HZ,
            repro.max_interactive_heartbeat * 1000L / HZ,
            repro.max_heartbeat * 1000L / HZ,
            repro.max_button_queue,
            repro.max_miniapp_queue,
            repro.cpu_boost_requested
                ? boolean_true : boolean_false,
            repro.min_cpu_frequency_hz,
            repro.max_cpu_frequency_hz,
            repro.violation_count,
            repro.trace_count);

        if(length > 0)
            write_all(
                file, line,
                (size_t)length < sizeof(line)
                    ? (size_t)length : sizeof(line) - 1u);
        close(file);
    }

    file = open(
        REPRO_DIRECTORY "/frame-crc.csv",
        O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(file >= 0) {
        static const char header[] =
            "tick,present_sequence,framebuffer_crc\n";

        write_all(file, header, sizeof(header) - 1u);
        for(i = 0; i < repro.trace_count; ++i) {
            const struct repro_trace *entry = &repro.trace[i];
            int length;

            if(i > 0 &&
               entry->present_sequence ==
                   repro.trace[i - 1u].present_sequence)
                continue;
            length = snprintf(
                line, sizeof(line), "%ld,%lu,%08lx\n",
                entry->tick,
                (unsigned long)entry->present_sequence,
                (unsigned long)entry->framebuffer_crc);
            if(length > 0)
                write_all(
                    file, line,
                    (size_t)length < sizeof(line)
                        ? (size_t)length : sizeof(line) - 1u);
        }
        close(file);
    }

    file = open(
        REPRO_DIRECTORY "/environment.txt",
        O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(file >= 0) {
        int length = snprintf(
            line, sizeof(line), environment_format,
            HZ, repro.target_cycles,
            scenario_name(),
            repro.cpu_boost_requested
                ? boolean_true : boolean_false,
#ifndef SIMULATOR
            (long)CPUFREQ_NORMAL,
            (long)CPUFREQ_MAX,
#endif
            REPRO_MOVES);

        if(length > 0)
            write_all(
                file, line,
                (size_t)length < sizeof(line)
                    ? (size_t)length : sizeof(line) - 1u);
        close(file);
    }
}

#ifndef SIMULATOR
static void write_interrupted_result(void)
{
    static const char result[] =
        "{\n"
        "  \"status\":\"RESET\",\n"
        "  \"reason\":\"previous-run-ended-without-result\"\n"
        "}\n";
    int file = open(
        REPRO_DIRECTORY "/summary.json",
        O_WRONLY | O_CREAT | O_TRUNC, 0666);

    if(file >= 0) {
        write_all(file, result, sizeof(result) - 1u);
        close(file);
    }
}

static bool consume_trigger(const char *path)
{
    int file;

    file = open(path, O_RDONLY);
    if(file < 0)
        return false;
    close(file);
    remove(REPRO_RUNNING_PATH);
    if(rename(path, REPRO_RUNNING_PATH) < 0)
        return false;
    return true;
}

static enum repro_scenario consume_hardware_trigger(
    bool *cpu_boost_requested)
{
    int file;

    *cpu_boost_requested = false;
    mkdir("/.crazypod");
    mkdir(REPRO_DIRECTORY);
    file = open(REPRO_RUNNING_PATH, O_RDONLY);
    if(file >= 0) {
        close(file);
        write_interrupted_result();
        remove(REPRO_RUNNING_PATH);
    }
    if(consume_trigger(REPRO_GAME_BOOST_TRIGGER_PATH)) {
        *cpu_boost_requested = true;
        return REPRO_SCENARIO_GAME;
    }
    if(consume_trigger(REPRO_LAB_BOOST_TRIGGER_PATH)) {
        *cpu_boost_requested = true;
        return REPRO_SCENARIO_LAB;
    }
    if(consume_trigger(REPRO_GAME_TRIGGER_PATH))
        return REPRO_SCENARIO_GAME;
    if(consume_trigger(REPRO_LAB_TRIGGER_PATH))
        return REPRO_SCENARIO_LAB;
    if(consume_trigger(REPRO_TRIGGER_PATH))
        return REPRO_SCENARIO_FULL;
    return REPRO_SCENARIO_NONE;
}

static void repro_watchdog_thread(void)
{
    while(repro_watchdog_active) {
        long gap;

        sleep(MAX(1, HZ / 10));
        if(!repro_watchdog_active)
            break;
        gap = current_tick - repro_watchdog_heartbeat;
        if(gap <= REPRO_WATCHDOG_LIMIT)
            continue;
        if(gap > repro.max_heartbeat)
            repro.max_heartbeat = gap;
        if(repro.first_violation == NULL)
            repro.first_violation =
                "hardware-ui-thread-hang";
        ++repro.violation_count;
        repro.active = false;
        repro_watchdog_active = false;
        write_results(false, "hardware-ui-thread-hang");
        remove(REPRO_RUNNING_PATH);
        break;
    }
}
#endif

static void finish(bool passed, const char *reason)
{
#ifdef SIMULATOR
    long p95;
#endif

    trace_event(passed ? "finish-pass" : "finish-fail", 0);
    repro.active = false;
#ifndef SIMULATOR
    repro_watchdog_active = false;
#endif
    write_results(passed, reason);
#ifdef SIMULATOR
    p95 = latency_percentile95();
    fprintf(
        stderr,
        "CrazyPod Mini App E2E reproduction %s: "
        "reason=%s cycles=%u/%u samples=%u p95=%ldms "
        "worst-cycle-p95=%ldms "
        "move-max=%ldms action-max=%ldms "
        "heartbeat=%ldms all-phase-heartbeat=%ldms "
        "buttonq=%u miniappq=%u\n",
        passed ? "passed" : "failed",
        reason, repro.cycle, repro.target_cycles,
        repro.latency_count,
        p95 * 1000L / HZ,
        repro.max_cycle_p95 * 1000L / HZ,
        repro.max_move_latency * 1000L / HZ,
        repro.max_action_latency * 1000L / HZ,
        repro.max_interactive_heartbeat * 1000L / HZ,
        repro.max_heartbeat * 1000L / HZ,
        repro.max_button_queue, repro.max_miniapp_queue);
    exit(passed ? 0 : 2);
#else
    remove(REPRO_RUNNING_PATH);
#endif
}

static void note_violation(const char *reason, long value)
{
    if(repro.first_violation == NULL)
        repro.first_violation = reason;
    ++repro.violation_count;
    trace_event(reason, value);
}

static void finish_completed_scenario(void)
{
    if(repro.violation_count > 0)
        finish(
            false,
            repro.first_violation != NULL
                ? repro.first_violation
                : "performance-violation");
    else
        finish(true, "all-e2e-cycles");
}

static void set_phase(enum repro_phase phase, long now)
{
    repro.phase = phase;
    repro.phase_started = now;
    memset(&repro.action, 0, sizeof(repro.action));
    trace_event(phase_names[phase], (long)repro.cycle);
}

static bool frame_changed(uint32_t sequence, uint32_t crc)
{
    if(crazypod_present_sequence() <= sequence)
        return false;
#ifdef SIMULATOR
    return framebuffer_crc() != crc;
#else
    (void)crc;
    return true;
#endif
}

static int action_service(
    const char *label, long button, bool release,
    int timeout_ms, bool require_frame, long now)
{
    long timeout_ticks =
        MAX(1, (long)timeout_ms * HZ / 1000L);

    if(!repro.action.active) {
        repro.action.active = true;
        repro.action.release = release;
        repro.action.release_sent = !release;
        repro.action.require_frame = require_frame;
        repro.action.button = button;
        repro.action.started = now;
        repro.action.release_due =
            now + REPRO_CLICK_RELEASE;
        repro.action.deadline =
            now + timeout_ticks;
        repro.action.start_sequence =
            crazypod_present_sequence();
        repro.action.start_crc =
            framebuffer_crc();
        repro.action.label = label;
        button_queue_post(button, 0);
        trace_event(label, button);
        return 0;
    }
    if(repro.action.release &&
       !repro.action.release_sent &&
       !TIME_BEFORE(now, repro.action.release_due)) {
        button_queue_post(
            repro.action.button | BUTTON_REL, 0);
        repro.action.release_sent = true;
        trace_event("button-release", repro.action.button);
        return 0;
    }
    if(repro.action.release_sent &&
       frame_changed(
           repro.action.start_sequence,
           repro.action.start_crc)) {
        long latency =
            crazypod_present_last_tick() -
            repro.action.started;

        if(latency < 0)
            latency = now - repro.action.started;
        if(latency > repro.max_action_latency)
            repro.max_action_latency = latency;
        trace_event("action-present", latency);
        repro.action.active = false;
        return 1;
    }
    if(!TIME_BEFORE(now, repro.action.deadline)) {
        trace_event("action-timeout", now - repro.action.started);
        repro.action.active = false;
        if(require_frame) {
#ifdef SIMULATOR
            finish(false, label);
#else
            note_violation(
                label, now - repro.action.started);
#endif
        }
        return 1;
    }
    return 0;
}

static void remove_oldest_pending_move(void)
{
    if(repro.pending_count == 0)
        return;
    --repro.pending_count;
    if(repro.pending_count > 0)
        memmove(
            repro.pending_moves,
            repro.pending_moves + 1,
            repro.pending_count * sizeof(repro.pending_moves[0]));
}

static void observe_move_present(long now)
{
    uint32_t sequence =
        crazypod_present_sequence();
    uint32_t crc =
        framebuffer_crc();
    bool visible = true;

    if(sequence == repro.observed_sequence)
        return;
    repro.observed_sequence = sequence;
#ifdef SIMULATOR
    visible = crc != repro.observed_crc;
#endif
    repro.observed_crc = crc;
    if(repro.pending_count > 0) {
        long latency =
            crazypod_present_last_tick() -
            repro.pending_moves[0];

        if(latency < 0)
            latency = now - repro.pending_moves[0];
        if(repro.latency_count < REPRO_LATENCY_CAPACITY)
            repro.latencies[repro.latency_count++] = latency;
        if(latency > repro.max_action_latency)
            repro.max_action_latency = latency;
        if(latency > repro.max_move_latency)
            repro.max_move_latency = latency;
        if(visible)
            ++repro.move_visible;
        repro.move_misses = 0;
        remove_oldest_pending_move();
        trace_event(
            visible ? "move-present" : "move-noop-present",
            latency);
    }
}

static void service_moves(long now)
{
    static const long moves[] = {
        BUTTON_LEFT,
        BUTTON_PLAY,
        BUTTON_RIGHT,
        BUTTON_MENU,
    };
    long cycle_p95;

    observe_move_present(now);
    if(repro.release_pending &&
       !TIME_BEFORE(now, repro.release_due)) {
        button_queue_post(
            repro.release_button | BUTTON_REL, 0);
        repro.release_pending = false;
        trace_event("move-release", repro.release_button);
    }
    if(repro.move_count < REPRO_MOVES &&
       !repro.release_pending &&
       !TIME_BEFORE(now, repro.next_move)) {
        long button =
            moves[repro.move_count %
                  (sizeof(moves) / sizeof(moves[0]))];

        button_queue_post(button, 0);
        repro.release_button = button;
        repro.release_due = now + REPRO_CLICK_RELEASE;
        repro.release_pending = true;
        if(repro.pending_count < REPRO_MOVES)
            repro.pending_moves[repro.pending_count++] = now;
        ++repro.move_count;
        repro.next_move += REPRO_MOVE_INTERVAL;
        trace_event("move-post", button);
        return;
    }
    if(repro.move_count < REPRO_MOVES ||
       repro.release_pending ||
       TIME_BEFORE(now, repro.next_move + HZ / 2))
        return;
    observe_move_present(now);
    if(repro.move_visible < REPRO_MOVES / 4) {
#ifdef SIMULATOR
        finish(false, "2048-visible-move-count");
#else
        note_violation(
            "2048-visible-move-count",
            repro.move_visible);
#endif
    }
    if(repro.max_move_latency > REPRO_RESPONSE_LIMIT)
        note_violation(
            "2048-input-max-latency",
            repro.max_move_latency);
    cycle_p95 = latency_percentile95_range(
        repro.cycle_latency_start,
        repro.latency_count - repro.cycle_latency_start);
    if(cycle_p95 > repro.max_cycle_p95)
        repro.max_cycle_p95 = cycle_p95;
    trace_event("cycle-move-p95", cycle_p95);
    if(cycle_p95 > REPRO_P95_LIMIT)
        note_violation(
            "2048-input-p95-latency",
            cycle_p95);
    set_phase(REPRO_GAME_EXIT_HOLD, now);
}

static bool route_is(enum crazypod_route route)
{
    const struct route_state *current =
        crazypod_ui_routes_current();

    return current != NULL && current->route == route;
}

static void begin_hold(long now)
{
    repro.hold_started = now;
    repro.hold_violation_reported = false;
    repro.hold_sequence =
        crazypod_present_sequence();
    repro.hold_crc =
        framebuffer_crc();
    button_queue_post(BUTTON_MENU, 0);
    trace_event("menu-hold-post", BUTTON_MENU);
}

static bool service_hold(long now, const char *failure)
{
    if(repro.hold_started == 0)
        begin_hold(now);
    if(crazypod_miniapps_feature_exit_prompt_visible() &&
       frame_changed(repro.hold_sequence, repro.hold_crc)) {
        long deadline_latency =
            crazypod_present_last_tick() -
            (repro.hold_started + HZ / 2);

        if(deadline_latency < 0)
            deadline_latency =
                now - (repro.hold_started + HZ / 2);
        if(deadline_latency > HZ * 150 / 1000)
            note_violation(failure, deadline_latency);
        trace_event("exit-prompt-present", deadline_latency);
        button_queue_post(BUTTON_MENU | BUTTON_REL, 0);
        trace_event("menu-hold-release", BUTTON_MENU);
        repro.hold_started = 0;
        return true;
    }
    if(!TIME_BEFORE(
           now, repro.hold_started +
               MAX(1, HZ * 700 / 1000))) {
#ifdef SIMULATOR
        finish(false, failure);
#else
        if(!repro.hold_violation_reported) {
            note_violation(
                failure, now - repro.hold_started);
            repro.hold_violation_reported = true;
        }
        if(!TIME_BEFORE(
               now, repro.hold_started + 2 * HZ)) {
            button_queue_post(
                BUTTON_MENU | BUTTON_REL, 0);
            repro.hold_started = 0;
            return true;
        }
#endif
    }
    return false;
}

bool crazypod_miniapp_repro_start(long now)
{
#ifdef SIMULATOR
    const char *cycles;
#endif
    enum repro_scenario scenario =
        REPRO_SCENARIO_FULL;
    bool cpu_boost_requested = false;
    long target_cycles = REPRO_CYCLES;

#ifdef SIMULATOR
    if(getenv("CRAZYPOD_SIM_MINIAPP_REPRO") == NULL)
        return false;
    cycles = getenv("CRAZYPOD_SIM_MINIAPP_REPRO_CYCLES");
    if(cycles != NULL) {
        char *end;
        long requested = strtol(cycles, &end, 10);

        if(end != cycles && *end == '\0' &&
           requested >= 1 && requested <= REPRO_CYCLES)
            target_cycles = requested;
    }
#else
    scenario = consume_hardware_trigger(
        &cpu_boost_requested);
    if(scenario == REPRO_SCENARIO_NONE)
        return false;
    if(scenario != REPRO_SCENARIO_FULL)
        target_cycles = 1;
#endif
    memset(&repro, 0, sizeof(repro));
    repro.active = true;
    repro.scenario = scenario;
    repro.cpu_boost_requested =
        cpu_boost_requested;
    repro.target_cycles = (unsigned)target_cycles;
    repro.observed_sequence =
        crazypod_present_sequence();
    repro.observed_crc =
        framebuffer_crc();
    set_phase(REPRO_WAIT_LIST, now);
#ifndef SIMULATOR
    repro_watchdog_heartbeat = now;
    repro_watchdog_active = true;
    if(create_thread(
           repro_watchdog_thread,
           repro_watchdog_stack,
           sizeof(repro_watchdog_stack),
           0, "miniapp repro watchdog"
           IF_PRIO(, PRIORITY_BACKGROUND)
           IF_COP(, CPU)) == 0) {
        finish(false, "hardware-watchdog-thread");
        return false;
    }
#endif
    crazypod_app_launcher_open(CRAZYPOD_APP_MINI_APPS);
    repro.last_service = current_tick;
    repro.phase_started = current_tick;
    trace_event("open-miniapps-list", 0);
    return true;
}

bool crazypod_miniapp_repro_cpu_boost_requested(void)
{
    return repro.active &&
        repro.cpu_boost_requested;
}

int crazypod_miniapp_repro_wait_ticks(void)
{
    return repro.active ? 1 : (HZ > 0 ? HZ : 1);
}

void crazypod_miniapp_repro_service(long now)
{
    struct route_state *route;
    long heartbeat;
    bool interactive_heartbeat;
    int target;

    if(!repro.active)
        return;
    heartbeat = now - repro.last_service;
#ifndef SIMULATOR
    repro_watchdog_heartbeat = now;
#endif
    if(heartbeat > repro.max_heartbeat)
        repro.max_heartbeat = heartbeat;
    repro.last_service = now;
    interactive_heartbeat = repro.interaction_started &&
        repro.phase != REPRO_WAIT_LIST &&
        repro.phase != REPRO_OPEN_GAME &&
        repro.phase != REPRO_OPEN_LAB;
    if(interactive_heartbeat &&
       heartbeat > repro.max_interactive_heartbeat)
        repro.max_interactive_heartbeat = heartbeat;
    if(interactive_heartbeat && heartbeat > REPRO_HEARTBEAT_LIMIT)
        note_violation("ui-heartbeat-gap", heartbeat);
    if(repro.last_heartbeat_trace == 0 ||
       !TIME_BEFORE(
           now,
           repro.last_heartbeat_trace +
               MAX(1, HZ * 50 / 1000))) {
        repro.last_heartbeat_trace = now;
        trace_event("heartbeat", heartbeat);
    }

    route = crazypod_ui_routes_current();
    switch(repro.phase) {
    case REPRO_WAIT_LIST:
        if(route_is(UTILITIES_ROUTE_MENU) &&
           frame_changed(
               repro.observed_sequence,
               repro.observed_crc)) {
            repro.observed_sequence =
                crazypod_present_sequence();
            repro.observed_crc =
                framebuffer_crc();
            repro.max_heartbeat = 0;
            repro.last_service = now;
            set_phase(
                repro.scenario == REPRO_SCENARIO_LAB
                    ? REPRO_MOVE_TO_LAB
                    : REPRO_MOVE_TO_GAME,
                now);
        }
        else if(!TIME_BEFORE(
                    now, repro.phase_started + 2 * HZ))
            finish(false, "miniapps-list-first-frame");
        break;

    case REPRO_MOVE_TO_GAME:
        target = crazypod_miniapps_find("game2048");
        if(target < 0 || route == NULL)
            finish(false, "game2048-catalog");
        else if(route->selected < target) {
            if(action_service(
                   "list-next-game", BUTTON_RIGHT, true,
                   750, true, now))
                memset(&repro.action, 0, sizeof(repro.action));
        }
        else if(route->selected > target) {
            if(action_service(
                   "list-previous-game", BUTTON_LEFT, true,
                   750, true, now))
                memset(&repro.action, 0, sizeof(repro.action));
        }
        else
            set_phase(REPRO_OPEN_GAME, now);
        break;

    case REPRO_OPEN_GAME:
        if(action_service(
               "game2048-first-frame", BUTTON_SELECT, true,
               1500, true, now)) {
            if(!route_is(MINIAPP_ROUTE_VIEW) ||
               !app_is("game2048") ||
               !crazypod_miniapps_feature_has_scene_content())
                finish(false, "game2048-first-frame-state");
            repro.interaction_started = true;
            set_phase(REPRO_ENTER_GAME, now);
        }
        break;

    case REPRO_ENTER_GAME:
        if(action_service(
               "game2048-enter-game", BUTTON_SELECT, true,
               1500, true, now))
            set_phase(REPRO_MOVES_BEGIN, now);
        break;

    case REPRO_OPEN_PAUSE:
        if(action_service(
               "game2048-open-pause", BUTTON_SELECT, true,
               750, true, now))
            set_phase(REPRO_SELECT_RESTART, now);
        break;

    case REPRO_SELECT_RESTART:
        if(action_service(
               "game2048-select-restart",
               BUTTON_SCROLL_FWD, false,
               750, true, now))
            set_phase(REPRO_OPEN_RESTART_CONFIRM, now);
        break;

    case REPRO_OPEN_RESTART_CONFIRM:
        if(action_service(
               "game2048-open-restart-confirm",
               BUTTON_SELECT, true, 750, true, now))
            set_phase(REPRO_SELECT_RESTART_YES, now);
        break;

    case REPRO_SELECT_RESTART_YES:
        if(action_service(
               "game2048-select-restart-yes",
               BUTTON_RIGHT, true, 750, true, now))
            set_phase(REPRO_CONFIRM_RESTART, now);
        break;

    case REPRO_CONFIRM_RESTART:
        if(action_service(
               "game2048-confirm-restart",
               BUTTON_SELECT, true, 1500, true, now))
            set_phase(REPRO_MOVES_BEGIN, now);
        break;

    case REPRO_MOVES_BEGIN:
        repro.move_count = 0;
        repro.move_visible = 0;
        repro.move_misses = 0;
        repro.pending_count = 0;
        repro.cycle_latency_start = repro.latency_count;
        repro.release_pending = false;
        repro.next_move = now;
        repro.observed_sequence =
            crazypod_present_sequence();
        repro.observed_crc =
            framebuffer_crc();
        set_phase(REPRO_MOVES_ACTIVE, now);
        break;

    case REPRO_MOVES_ACTIVE:
        service_moves(now);
        break;

    case REPRO_GAME_EXIT_HOLD:
        if(service_hold(now, "game2048-exit-prompt"))
            set_phase(REPRO_GAME_EXIT_RIGHT, now);
        break;

    case REPRO_GAME_EXIT_RIGHT:
        if(action_service(
               "game2048-exit-select",
               BUTTON_RIGHT, true, 300, true, now))
            set_phase(REPRO_GAME_EXIT_SELECT, now);
        break;

    case REPRO_GAME_EXIT_SELECT:
        if(action_service(
               "game2048-exit-list-frame",
               BUTTON_SELECT, true, 750, true, now)) {
            if(!route_is(UTILITIES_ROUTE_MENU) ||
               crazypod_miniapps_is_open())
                finish(false, "game2048-exit-state");
            if(!repro.active)
                return;
            if(repro.scenario == REPRO_SCENARIO_GAME) {
                ++repro.cycle;
                finish_completed_scenario();
                return;
            }
            set_phase(REPRO_MOVE_TO_LAB, now);
        }
        break;

    case REPRO_MOVE_TO_LAB:
        target = crazypod_miniapps_find("capability-lab");
        if(target < 0 || route == NULL)
            finish(false, "capability-lab-catalog");
        else if(route->selected < target) {
            if(action_service(
                   "list-next-lab", BUTTON_RIGHT, true,
                   750, true, now))
                memset(&repro.action, 0, sizeof(repro.action));
        }
        else if(route->selected > target) {
            if(action_service(
                   "list-previous-lab", BUTTON_LEFT, true,
                   750, true, now))
                memset(&repro.action, 0, sizeof(repro.action));
        }
        else
            set_phase(REPRO_OPEN_LAB, now);
        break;

    case REPRO_OPEN_LAB:
        if(action_service(
               "capability-lab-first-frame",
               BUTTON_SELECT, true, 1500, true, now)) {
            if(!route_is(MINIAPP_ROUTE_VIEW) ||
               !app_is("capability-lab") ||
               !crazypod_miniapps_feature_has_scene_content())
                finish(false, "capability-lab-first-frame-state");
            set_phase(REPRO_LAB_WHEEL, now);
        }
        break;

    case REPRO_LAB_WHEEL:
        if(action_service(
               "capability-lab-wheel-frame",
               BUTTON_SCROLL_FWD, false, 300, false, now))
            set_phase(REPRO_LAB_SELECT, now);
        break;

    case REPRO_LAB_SELECT:
        if(action_service(
               "capability-lab-page-frame",
               BUTTON_SELECT, true, 300, true, now))
            set_phase(REPRO_LAB_BACK, now);
        break;

    case REPRO_LAB_BACK:
        if(action_service(
               "capability-lab-back-frame",
               BUTTON_MENU, true, 300, true, now))
            set_phase(REPRO_LAB_EXIT_HOLD, now);
        break;

    case REPRO_LAB_EXIT_HOLD:
        if(service_hold(now, "capability-lab-exit-prompt"))
            set_phase(REPRO_LAB_EXIT_RIGHT, now);
        break;

    case REPRO_LAB_EXIT_RIGHT:
        if(action_service(
               "capability-lab-exit-select",
               BUTTON_RIGHT, true, 300, true, now))
            set_phase(REPRO_LAB_EXIT_SELECT, now);
        break;

    case REPRO_LAB_EXIT_SELECT:
        if(action_service(
               "capability-lab-exit-list-frame",
               BUTTON_SELECT, true, 750, true, now)) {
            if(!route_is(UTILITIES_ROUTE_MENU) ||
               crazypod_miniapps_is_open())
                finish(false, "capability-lab-exit-state");
            if(!repro.active)
                return;
            ++repro.cycle;
            if(repro.cycle >= repro.target_cycles) {
                finish_completed_scenario();
                return;
            }
            set_phase(REPRO_MOVE_TO_GAME, now);
        }
        break;
    }
}

#endif
