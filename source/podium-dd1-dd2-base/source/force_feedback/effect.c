#include "force_feedback/effect.h"

#include <stdint.h>

static uint16_t clamp_gain(uint16_t gain) {
    return gain > FORCE_EFFECT_MAXIMUM_GAIN ? FORCE_EFFECT_MAXIMUM_GAIN : gain;
}

static int32_t scale_force(int64_t force, uint16_t gain) {
    return (int32_t)(force * clamp_gain(gain) / FORCE_EFFECT_MAXIMUM_GAIN);
}

static int32_t opposing_force(int64_t motion, uint32_t saturation, uint16_t maximum_force,
                              uint16_t gain) {
    if (motion == 0 || maximum_force == 0 || gain == 0) {
        return 0;
    }

    int32_t direction = motion > 0 ? -1 : 1;
    uint64_t magnitude = motion > 0 ? (uint64_t)motion : (uint64_t)-motion;
    if (saturation != 0 && magnitude < saturation) {
        magnitude = (uint64_t)maximum_force * magnitude / saturation;
    } else {
        magnitude = maximum_force;
    }

    return direction * scale_force((int64_t)magnitude, gain);
}

int32_t force_effect_constant(int32_t magnitude, uint16_t gain) {
    return scale_force(magnitude, gain);
}

int32_t force_effect_spring(const ForceSpringEffect *effect, int32_t position) {
    int64_t displacement = (int64_t)position - effect->center;
    if (displacement > 0) {
        displacement -= effect->deadband;
        if (displacement < 0) {
            displacement = 0;
        }
    } else {
        displacement += effect->deadband;
        if (displacement > 0) {
            displacement = 0;
        }
    }

    return opposing_force(displacement, effect->saturation_distance, effect->maximum_force,
                          effect->gain);
}

int32_t force_effect_damper(const ForceDamperEffect *effect, int32_t velocity) {
    return opposing_force(velocity, effect->saturation_velocity, effect->maximum_force,
                          effect->gain);
}

int32_t force_effect_mix(int32_t accumulated, int32_t contribution, uint16_t maximum_force) {
    int64_t mixed = (int64_t)accumulated + contribution;
    if (mixed < -(int32_t)maximum_force) {
        return -(int32_t)maximum_force;
    }
    if (mixed > maximum_force) {
        return maximum_force;
    }
    return (int32_t)mixed;
}
