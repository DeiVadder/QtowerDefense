"""Turns CC0 model packs into the meshes of the game, see docs/ASSETS.md.

Run from an environment with Qt on the PATH, see scripts/env.cmd:

    python scripts/import_assets.py <folder with the zip files of the packs>

Each model of the game is one mesh, so that it can be drawn with instancing: the parts of a model
are merged into one, stacked on top of each other, in the pose they have in their file. A model
stands on y = 0 around the origin and is one unit wide, or one unit high with "height". Nothing of
a pack is unpacked, the script reads what it needs out of the zip files. Only the meshes and the
textures of the game go into the repository.
"""

import json
import math
import pathlib
import shutil
import struct
import subprocess
import sys
import tempfile
import zipfile

from qtd_common import REPO

TARGET = REPO / "src" / "client" / "assets"
TOWERS = "tower-defense-kit"
BLOCKY = "blocky-characters"

# name: pack, the parts from the bottom up, what fits into one unit, the texture of the pack
MODELS = {
    "ballista": (TOWERS, ["tower-square-bottom-a", "weapon-ballista"], "width", "colormap"),
    "twin_ballista": (
        TOWERS,
        ["tower-square-bottom-b", "tower-square-middle-a", "weapon-ballista"],
        "width",
        "colormap",
    ),
    "bastion": (
        TOWERS,
        ["tower-round-bottom-a", "tower-round-middle-a", "tower-round-top-a"],
        "width",
        "colormap",
    ),
    "horseshoe": (TOWERS, ["tower-round-bottom-b", "weapon-turret"], "width", "colormap"),
    "frost_spire": (
        TOWERS,
        ["tower-round-bottom-c", "tower-round-crystals"],
        "width",
        "colormap",
    ),
    "venom_spire": (
        TOWERS,
        ["tower-round-bottom-c", "tower-round-middle-c", "tower-round-roof-c"],
        "width",
        "colormap",
    ),
    "catapult": (TOWERS, ["tower-square-bottom-c", "weapon-catapult"], "width", "colormap"),
    "rock": (TOWERS, ["detail-rocks"], "width", "colormap"),
    "tree": (TOWERS, ["detail-tree-large"], "width", "colormap"),
}
for letter, enemy in zip(
    "bcdefghijk",
    [
        "mire_goblin",
        "stone_troll",
        "swamp_rat",
        "iron_beetle",
        "ember_imp",
        "frost_wraith",
        "moss_shaman",
        "hex_warden",
        "bog_titan",
        "cinder_drake",
    ],
):
    MODELS[enemy] = (BLOCKY, [f"character-{letter}"], "height", f"texture-{letter}")

# name: pack, the start of a file name in it
SOUNDS = {
    "build": ("impact-sounds", "impactWood_heavy_000"),
    "upgrade": ("impact-sounds", "impactMetal_heavy_000"),
    "sell": ("rpg-audio", "handleCoins"),
    "spell": ("impact-sounds", "impactBell_heavy_000"),
    "wave": ("interface-sounds", "bong_001"),
    "won": ("music-jingles", "jingles_STEEL00"),
    "lost": ("music-jingles", "jingles_PIZZI00"),
}

PACK_NAMES = {
    TOWERS: "Tower Defense Kit",
    BLOCKY: "Blocky Characters",
    "impact-sounds": "Impact Sounds",
    "interface-sounds": "Interface Sounds",
    "rpg-audio": "RPG Audio",
    "music-jingles": "Music Jingles",
}
DATE = "2026-11-06"

COMPONENTS = {5121: ("B", 1), 5123: ("H", 2), 5125: ("I", 4), 5126: ("f", 4)}
SIZES = {"SCALAR": 1, "VEC2": 2, "VEC3": 3}


def read_glb(data):
    size = struct.unpack_from("<I", data, 12)[0]
    document = json.loads(data[20 : 20 + size])
    return document, data[20 + size + 8 :]


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


def matrix_of(node):
    if "matrix" in node:
        m = node["matrix"]
        return [[m[col * 4 + row] for col in range(4)] for row in range(4)]
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


def collect(document, binary, index, parent, mesh):
    node = document["nodes"][index]
    matrix = multiply(parent, matrix_of(node))
    if "mesh" in node:
        for primitive in document["meshes"][node["mesh"]]["primitives"]:
            base = len(mesh["positions"])
            attributes = primitive["attributes"]
            for place in read_accessor(document, binary, attributes["POSITION"]):
                mesh["positions"].append(apply(matrix, place, True))
            for normal in read_accessor(document, binary, attributes["NORMAL"]):
                turned = apply(matrix, normal, False)
                length = math.sqrt(sum(value * value for value in turned)) or 1
                mesh["normals"].append([value / length for value in turned])
            mesh["uvs"].extend(read_accessor(document, binary, attributes["TEXCOORD_0"]))
            indices = read_accessor(document, binary, primitive["indices"])
            mesh["indices"].extend(base + item[0] for item in indices)
    for child in node.get("children", []):
        collect(document, binary, child, matrix, mesh)


def part_of(pack, name):
    path = next(item for item in pack.namelist() if item.endswith(f"GLB format/{name}.glb"))
    document, binary = read_glb(pack.read(path))
    mesh = {"positions": [], "normals": [], "uvs": [], "indices": []}
    identity = [[float(row == col) for col in range(4)] for row in range(4)]
    for root in document["scenes"][document.get("scene", 0)]["nodes"]:
        collect(document, binary, root, identity, mesh)
    return mesh


def merged(pack, names):
    whole = {"positions": [], "normals": [], "uvs": [], "indices": []}
    height = 0.0
    for name in names:
        part = part_of(pack, name)
        lowest = min(place[1] for place in part["positions"])
        base = len(whole["positions"])
        lift = height - lowest
        whole["positions"].extend([x, y + lift, z] for x, y, z in part["positions"])
        whole["normals"].extend(part["normals"])
        whole["uvs"].extend(part["uvs"])
        whole["indices"].extend(base + index for index in part["indices"])
        height = max(place[1] for place in whole["positions"])
    return whole


def normalized(mesh, fit):
    places = mesh["positions"]
    lows = [min(place[axis] for place in places) for axis in range(3)]
    highs = [max(place[axis] for place in places) for axis in range(3)]
    extent = highs[1] - lows[1] if fit == "height" else max(highs[0] - lows[0], highs[2] - lows[2])
    middle_x, middle_z = (lows[0] + highs[0]) / 2, (lows[2] + highs[2]) / 2
    mesh["positions"] = [
        [(x - middle_x) / extent, (y - lows[1]) / extent, (z - middle_z) / extent]
        for x, y, z in places
    ]


def glb_of(mesh):
    """A file with nothing but the merged geometry."""
    chunks = bytearray()
    views = []
    accessors = []

    def add(data, kind, code, count, target):
        views.append(
            {"buffer": 0, "byteOffset": len(chunks), "byteLength": len(data), "target": target}
        )
        accessor = {
            "bufferView": len(views) - 1,
            "componentType": code,
            "count": count,
            "type": kind,
        }
        accessors.append(accessor)
        chunks.extend(data)
        chunks.extend(b"\0" * (-len(chunks) % 4))
        return accessor

    flat = [value for place in mesh["positions"] for value in place]
    count = len(mesh["positions"])
    positions = add(struct.pack(f"<{len(flat)}f", *flat), "VEC3", 5126, count, 34962)
    positions["min"] = [min(place[axis] for place in mesh["positions"]) for axis in range(3)]
    positions["max"] = [max(place[axis] for place in mesh["positions"]) for axis in range(3)]
    flat = [value for normal in mesh["normals"] for value in normal]
    add(struct.pack(f"<{len(flat)}f", *flat), "VEC3", 5126, count, 34962)
    flat = [value for uv in mesh["uvs"] for value in uv]
    add(struct.pack(f"<{len(flat)}f", *flat), "VEC2", 5126, count, 34962)
    indices = mesh["indices"]
    add(struct.pack(f"<{len(indices)}I", *indices), "SCALAR", 5125, len(indices), 34963)
    document = {
        "asset": {"version": "2.0"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0, "name": "model"}],
        "meshes": [
            {
                "primitives": [
                    {"attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2}, "indices": 3}
                ]
            }
        ],
        "buffers": [{"byteLength": len(chunks)}],
        "bufferViews": views,
        "accessors": accessors,
    }
    text = json.dumps(document).encode()
    text += b" " * (-len(text) % 4)
    total = 12 + 8 + len(text) + 8 + len(chunks)
    return (
        struct.pack("<4sII", b"glTF", 2, total)
        + struct.pack("<I4s", len(text), b"JSON")
        + text
        + struct.pack("<I4s", len(chunks), b"BIN\0")
        + bytes(chunks)
    )


def texture_of(pack, name):
    path = next(item for item in pack.namelist() if item.endswith(f"Textures/{name}.png"))
    return pack.read(path)


def import_model(folder, name, work):
    pack_name, parts, fit, texture = MODELS[name]
    with zipfile.ZipFile(folder / f"{pack_name}.zip") as pack:
        mesh = merged(pack, parts)
        normalized(mesh, fit)
        (TARGET / f"{name}.png").write_bytes(texture_of(pack, texture))
    source = work / f"{name}.glb"
    source.write_bytes(glb_of(mesh))
    out = work / name
    subprocess.run(
        ["balsam", "-o", str(out), str(source)],
        check=True,
        stdout=subprocess.DEVNULL,
        timeout=60,
    )
    meshes = list(out.rglob("*.mesh"))
    if len(meshes) != 1:
        raise RuntimeError(f"{name}: balsam gave {len(meshes)} meshes")
    shutil.copyfile(meshes[0], TARGET / f"{name}.mesh")


def import_sound(folder, name):
    pack_name, start = SOUNDS[name]
    with zipfile.ZipFile(folder / f"{pack_name}.zip") as pack:
        path = next(
            item
            for item in sorted(pack.namelist())
            if item.rsplit("/", 1)[-1].startswith(start) and item.endswith(".ogg")
        )
        (TARGET / f"{name}.ogg").write_bytes(pack.read(path))
    return path.rsplit("/", 1)[-1]


def row(file, pack, change):
    url = f"https://kenney.nl/assets/{pack}"
    return f"| `{file}` | {PACK_NAMES[pack]}, {url} | Kenney | CC0 | {DATE} | {change} |"


def asset_list(sounds):
    lines = [
        "# Assets",
        "",
        "Alle Dateien liegen in `src/client/assets`. Erzeugt von `scripts/import_assets.py`, nicht",
        "von Hand ändern. Jedes Modell ist ein Mesh mit der Textur des Pakets.",
        "",
        "| Datei | Quelle | Urheber | Lizenz | Datum | Änderungen |",
        "|---|---|---|---|---|---|",
    ]
    for name, (pack, parts, fit, texture) in MODELS.items():
        change = f"Aus {', '.join(parts)} zu einem Mesh verbunden, auf eine Einheit skaliert"
        lines.append(row(f"{name}.mesh", pack, change))
        lines.append(row(f"{name}.png", pack, f"{texture}.png, unverändert"))
    for name, (pack, _) in SOUNDS.items():
        lines.append(row(f"{name}.ogg", pack, f"{sounds[name]}, unverändert"))
    return "\n".join(lines) + "\n"


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 1
    folder = pathlib.Path(sys.argv[1])
    TARGET.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as work:
        for name in MODELS:
            import_model(folder, name, pathlib.Path(work))
            print(name)
    sounds = {name: import_sound(folder, name) for name in SOUNDS}
    (REPO / "docs" / "ASSETS.md").write_text(asset_list(sounds), encoding="utf-8", newline="\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
