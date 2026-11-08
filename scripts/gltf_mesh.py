"""Reads the geometry of glTF files, and poses skinned models, for scripts/import_assets.py.

A skinned model is posed by linear blend skinning: each vertex follows the joints it is bound to,
weighted. The pose of a joint comes from the channels of an animation, linear between two keys,
rotations normalized after the blend.
"""

import base64
import json
import math
import struct

COMPONENTS = {5121: ("B", 1), 5123: ("H", 2), 5125: ("I", 4), 5126: ("f", 4)}
SIZES = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}
IDENTITY = [[float(row == col) for col in range(4)] for row in range(4)]


def read_glb(data):
    size = struct.unpack_from("<I", data, 12)[0]
    document = json.loads(data[20 : 20 + size])
    return document, data[20 + size + 8 :]


def read_gltf(text, read_file):
    """A .gltf file with its buffer, embedded or in a file that read_file reads by its name."""
    document = json.loads(text)
    uri = document["buffers"][0]["uri"]
    if uri.startswith("data:"):
        return document, base64.b64decode(uri.split(",", 1)[1])
    return document, read_file(uri)


def read_accessor(document, binary, index):
    accessor = document["accessors"][index]
    view = document["bufferViews"][accessor["bufferView"]]
    code, width = COMPONENTS[accessor["componentType"]]
    count = SIZES[accessor["type"]]
    stride = view.get("byteStride", width * count)
    start = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
    return [
        struct.unpack_from(f"<{count}{code}", binary, start + item * stride)
        for item in range(accessor["count"])
    ]


def multiply(left, right):
    return [
        [sum(left[row][k] * right[k][col] for k in range(4)) for col in range(4)]
        for row in range(4)
    ]


def column_major(values):
    return [[values[col * 4 + row] for col in range(4)] for row in range(4)]


def matrix_of(node):
    if "matrix" in node:
        return column_major(node["matrix"])
    x, y, z, w = node.get("rotation", [0, 0, 0, 1])
    sx, sy, sz = node.get("scale", [1, 1, 1])
    tx, ty, tz = node.get("translation", [0, 0, 0])
    return [
        [(1 - 2 * (y * y + z * z)) * sx, 2 * (x * y - z * w) * sy, 2 * (x * z + y * w) * sz, tx],
        [2 * (x * y + z * w) * sx, (1 - 2 * (x * x + z * z)) * sy, 2 * (y * z - x * w) * sz, ty],
        [2 * (x * z - y * w) * sx, 2 * (y * z + x * w) * sy, (1 - 2 * (x * x + y * y)) * sz, tz],
        [0, 0, 0, 1],
    ]


def apply(matrix, vector, moves):
    w = 1 if moves else 0
    return [sum(matrix[row][k] * ([*vector, w][k]) for k in range(4)) for row in range(3)]


def unit(vector):
    length = math.sqrt(sum(value * value for value in vector)) or 1
    return [value / length for value in vector]


def sample(times, values, time, rotation):
    """The value of a channel at a time, linear between its keys, held before and after them."""
    if time <= times[0]:
        return list(values[0])
    for key in range(1, len(times)):
        if time <= times[key] and times[key] > times[key - 1]:
            share = (time - times[key - 1]) / (times[key] - times[key - 1])
            before, after = values[key - 1], values[key]
            if rotation and sum(a * b for a, b in zip(before, after)) < 0:
                after = [-value for value in after]  # the shorter way round
            blend = [a + (b - a) * share for a, b in zip(before, after)]
            return unit(blend) if rotation else blend
    return list(values[-1])


def animation_named(document, ending):
    """The animation whose name ends with the given text, Quaternius writes `Armature|Walk`. Only
    what this script can pose: linear keys from time 0."""
    animation = next(item for item in document["animations"] if item["name"].endswith(ending))
    for sampler in animation["samplers"]:
        assert sampler.get("interpolation", "LINEAR") == "LINEAR", "only linear keys"
        assert document["accessors"][sampler["input"]]["min"][0] == 0, "keys from time 0"
    return animation


def animation_length(document, animation):
    return max(document["accessors"][item["input"]]["max"][0] for item in animation["samplers"])


def posed_nodes(document, binary, animation, time):
    nodes = [dict(node) for node in document["nodes"]]
    for channel in animation["channels"]:
        sampler = animation["samplers"][channel["sampler"]]
        times = [key[0] for key in read_accessor(document, binary, sampler["input"])]
        values = read_accessor(document, binary, sampler["output"])
        path = channel["target"]["path"]
        nodes[channel["target"]["node"]][path] = sample(times, values, time, path == "rotation")
    return nodes


def world_matrices(document, nodes):
    world = [IDENTITY] * len(nodes)

    def visit(index, parent):
        world[index] = multiply(parent, matrix_of(nodes[index]))
        for child in nodes[index].get("children", []):
            visit(child, world[index])

    for root in document["scenes"][document.get("scene", 0)]["nodes"]:
        visit(root, IDENTITY)
    return world


def skinned(place, normal, joints, weights, matrices):
    """A vertex moved by its joints. The skin ignores the place of its own node."""
    moved, turned = [0.0] * 3, [0.0] * 3
    for joint, weight in zip(joints, weights):
        if weight == 0:
            continue
        for axis, value in enumerate(apply(matrices[joint], place, True)):
            moved[axis] += weight * value
        for axis, value in enumerate(apply(matrices[joint], normal, False)):
            turned[axis] += weight * value
    return moved, unit(turned)


def joint_matrices(document, binary, skin, world):
    inverse = read_accessor(document, binary, skin["inverseBindMatrices"])
    return [multiply(world[joint], column_major(bind)) for joint, bind in zip(skin["joints"], inverse)]


def posed_mesh(document, binary, animation, time):
    """All skinned parts of a model in the pose of the animation at the time, as one mesh."""
    world = world_matrices(document, posed_nodes(document, binary, animation, time))
    mesh = {"positions": [], "normals": [], "uvs": [], "indices": [], "palette": []}
    for node in document["nodes"]:
        if "mesh" not in node:
            continue
        assert "skin" in node, "a part without skin would be lost"
        matrices = joint_matrices(document, binary, document["skins"][node["skin"]], world)
        for primitive in document["meshes"][node["mesh"]]["primitives"]:
            attributes = primitive["attributes"]
            for name in ("WEIGHTS_0", "TEXCOORD_0"):
                assert document["accessors"][attributes[name]]["componentType"] == 5126, "floats"
            base = len(mesh["positions"])
            for vertex in zip(
                *(read_accessor(document, binary, attributes[name])
                  for name in ("POSITION", "NORMAL", "JOINTS_0", "WEIGHTS_0"))
            ):  # fmt: skip
                moved, turned = skinned(*vertex, matrices)
                mesh["positions"].append(moved)
                mesh["normals"].append(turned)
            mesh["uvs"].extend(read_accessor(document, binary, attributes["TEXCOORD_0"]))
            indices = read_accessor(document, binary, primitive["indices"])
            mesh["indices"].extend(base + item[0] for item in indices)
    return mesh


def walk_cycle(document, binary, ending, frames):
    """The frames of one cycle of an animation, evenly spaced, the last one before the first again."""
    animation = animation_named(document, ending)
    length = animation_length(document, animation)
    return [posed_mesh(document, binary, animation, length * frame / frames) for frame in range(frames)]
