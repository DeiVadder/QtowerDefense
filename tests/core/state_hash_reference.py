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
    return [
        100, 3,                                     # start gold, start lives
        2, 0, 0, 100000, 0,                         # path
        3, 50000, 10000, 20000, 10000, 1000000, 1000000,  # build slots
        1, 40, 4, 30000, 20000, 20000,              # towers
        1, 40000, 5000, 7, 1,                       # enemies
        1, 0, 2, 0, 4,                              # wave
    ]  # fmt: skip


def handmade_state():
    """The state of handmadeState() in tst_state_hash.cpp."""
    random_state, increment = pcg32_state(7, 54)
    return [
        0x0123456789ABCDEF, 42, 2, -5, 3,           # content, tick, status, gold, lives
        random_state, increment,
        9, 1, 2, 4,                                 # next id, spawn group, spawned, countdown
        1, 3, 0, 15000, 25000,                      # enemies
        1, 5, 1, 2, 6,                              # towers
    ]  # fmt: skip


print(f"content 0x{fnv1a(scenario_content()):016x}")
print(f"state   0x{fnv1a(handmade_state()):016x}")
