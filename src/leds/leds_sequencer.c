#include <stddef.h>
#include <stdint.h>

#include "app_error.h"
#include "app_timer.h"
#include "app_util_platform.h"
#include "nrf_assert.h"

#include "internal/sequencer_executor.h"
#include "leds_sequencer.h"

#define SEQUENCER_TIMER_INTERVAL_MS 10

typedef enum {
    SEQUENCER_PHASE_IDLE,
    SEQUENCER_PHASE_SMOOTH_START,
    SEQUENCER_PHASE_CONTINUATION,
    SEQUENCER_PHASE_NORMAL
} sequencer_phase_t;

typedef struct {
    executor_t executor;
    sequencer_phase_t phase;

    sequencer_config_t original_config;
    sequencer_step_t smooth_step;

    sequencer_callback_t callback;
    void *context;
} sequencer_t;

static sequencer_t m_yellow_sequencer;
static sequencer_t m_rgb_sequencer;

static bool m_initialized;

APP_TIMER_DEF(m_timer);

static sequencer_t *sequencer_get(sequencer_led_id_t id);
static void timer_callback(void *p_context);
static void sequencer_process(sequencer_t *sequencer);
static bool config_is_valid(sequencer_config_t const *config);
static bool config_has_timed_step(sequencer_config_t const *config);
static uint32_t smooth_duration_get(sequencer_step_t const *step, uint16_t current_value);

bool sequencer_init(void) {
    ASSERT(!m_initialized);

    if (m_initialized) return false;

    if (app_timer_create(
            &m_timer,
            APP_TIMER_MODE_REPEATED,
            timer_callback) != NRF_SUCCESS)
        return false;

    if (app_timer_start(
            m_timer,
            APP_TIMER_TICKS(SEQUENCER_TIMER_INTERVAL_MS),
            NULL) != NRF_SUCCESS)
        return false;

    m_initialized = true;

    return true;
}

bool sequencer_start(
    sequencer_led_id_t id,
    sequencer_config_t const *config,
    sequencer_start_mode_t start_mode,
    uint16_t current_value,
    sequencer_callback_t callback,
    void *context) {
    sequencer_t *sequencer = sequencer_get(id);

    if (!m_initialized || sequencer == NULL || callback == NULL ||
        !config_is_valid(config))
        return false;

    if (start_mode != SEQUENCER_START_FROM_STEP &&
        start_mode != SEQUENCER_START_CURRENT)
        return false;

    if (config->repeat && !config_has_timed_step(config))
        return false;

    CRITICAL_REGION_ENTER();

    sequencer->phase = SEQUENCER_PHASE_NORMAL;
    sequencer->callback = callback;
    sequencer->context = context;

    if (start_mode == SEQUENCER_START_FROM_STEP) {
        executor_start(
            &sequencer->executor,
            config,
            callback,
            context);
    } else {
        sequencer_config_t smooth_config;

        sequencer->original_config = *config;

        sequencer->smooth_step.from = current_value;
        sequencer->smooth_step.to = config->steps[0].to;
        sequencer->smooth_step.duration_ms = smooth_duration_get(
            &config->steps[0],
            current_value);

        smooth_config.steps = &sequencer->smooth_step;
        smooth_config.step_count = 1;
        smooth_config.repeat = false;

        sequencer->phase = SEQUENCER_PHASE_SMOOTH_START;

        executor_start(
            &sequencer->executor,
            &smooth_config,
            callback,
            context);
    }

    sequencer_process(sequencer);

    CRITICAL_REGION_EXIT();

    return true;
}

void sequencer_pause(sequencer_led_id_t id) {
    sequencer_t *sequencer = sequencer_get(id);

    if (sequencer == NULL) return;

    CRITICAL_REGION_ENTER();
    executor_pause(&sequencer->executor);
    CRITICAL_REGION_EXIT();
}

void sequencer_resume(sequencer_led_id_t id) {
    sequencer_t *sequencer = sequencer_get(id);

    if (sequencer == NULL) return;

    CRITICAL_REGION_ENTER();
    executor_resume(&sequencer->executor);
    CRITICAL_REGION_EXIT();
}

sequencer_state_t sequencer_get_state(sequencer_led_id_t id) {
    sequencer_t *sequencer = sequencer_get(id);
    sequencer_state_t state;

    if (sequencer == NULL) return SEQUENCER_STATE_IDLE;

    CRITICAL_REGION_ENTER();
    state = executor_get_state(&sequencer->executor);
    if (state == SEQUENCER_STATE_IDLE &&
        sequencer->phase != SEQUENCER_PHASE_IDLE &&
        sequencer->phase != SEQUENCER_PHASE_NORMAL)
        state = SEQUENCER_STATE_RUNNING;
    CRITICAL_REGION_EXIT();

    return state;
}

static sequencer_t *sequencer_get(sequencer_led_id_t id) {
    switch (id) {
        case SEQUENCER_YELLOW_LED:
            return &m_yellow_sequencer;
        case SEQUENCER_RGB_LED:
            return &m_rgb_sequencer;
        default:
            return NULL;
    }
}

static void timer_callback(void *p_context) {
    (void)p_context;

    sequencer_process(&m_yellow_sequencer);
    sequencer_process(&m_rgb_sequencer);
}

static void sequencer_process(sequencer_t *sequencer) {
    while (sequencer->phase != SEQUENCER_PHASE_IDLE) {
        sequencer_config_t config;

        executor_process(&sequencer->executor);

        if (executor_get_state(&sequencer->executor) != SEQUENCER_STATE_IDLE)
            return;

        if (sequencer->phase == SEQUENCER_PHASE_SMOOTH_START &&
            sequencer->original_config.step_count > 1) {
            config.steps = &sequencer->original_config.steps[1];
            config.step_count = sequencer->original_config.step_count - 1;
            config.repeat = false;
            sequencer->phase = SEQUENCER_PHASE_CONTINUATION;
        } else if (sequencer->phase != SEQUENCER_PHASE_NORMAL &&
                   sequencer->original_config.repeat) {
            config = sequencer->original_config;
            sequencer->phase = SEQUENCER_PHASE_NORMAL;
        } else {
            sequencer->phase = SEQUENCER_PHASE_IDLE;
            return;
        }

        executor_start(
            &sequencer->executor,
            &config,
            sequencer->callback,
            sequencer->context);
    }
}

static bool config_is_valid(sequencer_config_t const *config) {
    if (config == NULL) return false;
    if (config->steps == NULL) return false;
    if (config->step_count == 0) return false;

    return true;
}

static bool config_has_timed_step(sequencer_config_t const *config) {
    for (uint16_t i = 0; i < config->step_count; i++)
        if (config->steps[i].duration_ms > 0) return true;

    return false;
}

static uint32_t smooth_duration_get(
    sequencer_step_t const *step,
    uint16_t current_value) {
    uint32_t original_distance = step->from > step->to ?
        step->from - step->to : step->to - step->from;
    uint32_t remaining_distance = current_value > step->to ?
        current_value - step->to : step->to - current_value;

    if (original_distance == 0)
        return step->duration_ms;

    return (uint32_t)(
        (uint64_t)step->duration_ms * remaining_distance /
        original_distance);
}
