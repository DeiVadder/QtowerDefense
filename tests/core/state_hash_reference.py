"""Works out the pinned hashes of tst_state_hash.cpp without any code of the project.

FNV-1a and PCG32 are written down here a second time on purpose. If a test fails after an intended
change of the hash layout, change this file first and take the new values from its output.
"""

MASK = (1 << 64) - 1
PCG_MULTIPLIER = 6364136223846793005


def fnv1a(numbers):
    value = 14695981039346656037
    for number in numbers:
        for byte in (number & MASK).to_bytes(8, "little"):
            value = ((value ^ byte) * 1099511628211) & MASK
    return value


def pcg32_state(seed, stream):
    increment = ((stream << 1) | 1) & MASK
    state = (0 * PCG_MULTIPLIER + increment) & MASK
    state = (state + seed) & MASK
    state = (state * PCG_MULTIPLIER + increment) & MASK
    return state, increment


def scenario_content():
    """The content of tests/core/scenario.h, decimal values in ten-thousandths."""
    terrain = [0] * 121
    terrain[:11] = [1] * 11                         # the path in the first row
    terrain[22] = 2                                 # a rock at (0, 2)
    return [
        100, 3, 70,                                 # start gold, start lives, sell percent
        0,                                          # no bonus for an early wave
        2, 0, 0, 100000, 0,                         # path
        11, 11, *terrain,                           # grid
        1, 0,                                       # towers: damage type physical
        1, 0, 0,                                    # footprint
        1, 40, 4, 30000, 20000, 20000, 0,           # one level, without a splash
        0,                                          # no status effects
        1, 40000, 5000, 7, 1, 0, 0,                 # enemies: no regeneration, no boss
        0, 0, 0, 0,                                 # four resistances
        1, 0, 2, 0, 4,                              # wave
        0,                                          # no spells
    ]  # fmt: skip


def handmade_state():
    """The state of handmadeState() in tst_state_hash.cpp."""
    random_state, increment = pcg32_state(7, 54)
    return [
        0x0123456789ABCDEF, 42, 2, -5, 3,           # content, tick, status, gold, lives
        random_state, increment,
        9, 1, 2, 4,                                 # next id, spawn group, spawned, countdown
        1, 3, 0, 15000, 25000, 4, 5000, 6, 2500,    # enemies with slow and poison
        1, 5, 1, 2, 3, 6, 3, 2, 1, 70,              # towers: turned, aiming at the last, upgraded
        1, 11,                                      # one spell cooling down
    ]  # fmt: skip


print(f"content 0x{fnv1a(scenario_content()):016x}")
print(f"state   0x{fnv1a(handmade_state()):016x}")
