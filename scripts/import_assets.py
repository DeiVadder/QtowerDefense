"""Turns CC0 model packs into the meshes of the game, see docs/ASSETS.md.

Run from an environment with Qt on the PATH, see scripts/env.cmd:

    python scripts/import_assets.py <folder with the zip files of the packs>

Each model of the game is one mesh, so that it can be drawn with instancing: the parts of a model
are merged into one, stacked on top of each other, in the pose they have in their file. A model
stands on y = 0 around the origin and is one unit wide, or one unit high with "height". Nothing of
a pack is unpacked, the script reads what it needs out of the zip files. Only the meshes and the
textures of the game go into the repository.

Enemies walk (E-41): their walk animation is baked into WALK_FRAMES meshes `<name>_<frame>`, all in
the scale of the first frame. The client shows each enemy in the frame of its way. The monsters
share one texture, `monsters.png`.

The zip files are named as in PACKS. Ultimate Monsters comes as a folder from the page of
Quaternius, put its glTF folders into `ultimate-monsters.zip`. The Medieval Hexagon Pack is the
zip of its GitHub repository.
"""

import json
import pathlib
import shutil
import struct
import subprocess
import sys
import tempfile
import zipfile
import zlib

from gltf_mesh import IDENTITY, apply, matrix_of, multiply, read_accessor, read_glb, read_gltf
from gltf_mesh import unit, walk_cycle
from qtd_common import REPO

TARGET = REPO / "src" / "client" / "assets"
TOWERS = "tower-defense-kit"
NATURE = "nature-kit"
GRAVES = "graveyard-kit"
HEXAGON = "kaykit-medieval-hexagon"
MONSTERS = "ultimate-monsters"
WALK_FRAMES = 8  # kWalkFrames in src/client/game_session.h

# name: pack, the parts from the bottom up, what fits into one unit, the texture of the pack
MODELS = {
    "ballista": (HEXAGON, ["building_tower_A_blue"], "width", "hexagons_medieval"),
    "twin_ballista": (HEXAGON, ["building_tower_B_blue"], "width", "hexagons_medieval"),
    "bastion": (HEXAGON, ["building_castle_blue"], "width", "hexagons_medieval"),
    "horseshoe": (HEXAGON, ["building_barracks_yellow"], "width", "hexagons_medieval"),
    "frost_spire": (HEXAGON, ["building_church_blue"], "width", "hexagons_medieval"),
    "venom_spire": (HEXAGON, ["building_tower_A_green"], "width", "hexagons_medieval"),
    "catapult": (HEXAGON, ["building_tower_catapult_red"], "width", "hexagons_medieval"),
    "rock": (TOWERS, ["detail-rocks"], "width", "colormap"),
    "tree": (TOWERS, ["detail-tree-large"], "width", "colormap"),
}

# name: the model in Ultimate Monsters, the animation that is its walk
ENEMIES = {
    "mire_goblin": ("Big/glTF/Orc", "Walk"),
    "stone_troll": ("Big/glTF/Yeti", "Walk"),
    "swamp_rat": ("Big/glTF/Frog", "Walk"),
    "iron_beetle": ("Blob/glTF/GreenSpikyBlob", "Walk"),
    "ember_imp": ("Big/glTF/Demon", "Walk"),
    "frost_wraith": ("Flying/glTF/Ghost", "Fast_Flying"),
    "moss_shaman": ("Big/glTF/Tribal", "Walk"),
    "hex_warden": ("Big/glTF/Orc_Skull", "Walk"),
    "bog_titan": ("Big/glTF/MushroomKing", "Walk"),
    "cinder_drake": ("Flying/glTF/Dragon_Evolved", "Fast_Flying"),
}

# name: pack, the start of a file name in it
# The scenery of the landscapes: name, pack, part. One unit high, the landscape sets the size. The
# models of the Nature Kit have colors instead of a texture, a palette takes their place.
SCENERY = {
    "oak": (NATURE, "tree_oak"),
    "round_tree": (NATURE, "tree_default"),
    "tall_tree": (NATURE, "tree_tall"),
    "pine": (NATURE, "tree_pineRoundA"),
    "bush": (NATURE, "plant_bush"),
    "big_bush": (NATURE, "plant_bushLarge"),
    "boulder": (NATURE, "rock_largeA"),
    "stone": (NATURE, "rock_smallA"),
    "grass_tuft": (NATURE, "grass"),
    "tall_grass": (NATURE, "grass_large"),
    "yellow_flower": (NATURE, "flower_yellowA"),
    "red_flower": (NATURE, "flower_redA"),
    "purple_flower": (NATURE, "flower_purpleA"),
    "stump": (NATURE, "stump_round"),
    "fallen_log": (NATURE, "log"),
    "red_mushroom": (NATURE, "mushroom_redGroup"),
    "dark_tree": (NATURE, "tree_thin_dark"),
    "dark_oak": (NATURE, "tree_default_dark"),
    "old_stump": (NATURE, "stump_old"),
    "big_log": (NATURE, "log_large"),
    "lily": (NATURE, "lily_large"),
    "small_lily": (NATURE, "lily_small"),
    "reed": (NATURE, "plant_flatTall"),
    "tan_mushroom": (NATURE, "mushroom_tanGroup"),
    "broken_grave": (GRAVES, "gravestone-broken"),
}

# The Nature Kit is teal and peach. These earthy colors replace its own, by the name of a material.
NATURAL_COLORS = {
    "leafsGreen": (92, 138, 58),
    "leafsDark": (58, 96, 52),
    "leafsFall": (186, 120, 50),
    "grass": (110, 150, 66),
    "wood": (120, 84, 56),
    "woodBark": (104, 74, 50),
    "woodBarkDark": (78, 58, 44),
    "woodDark": (84, 60, 42),
    "woodInner": (196, 164, 120),
    "woodBirch": (222, 214, 196),
    "dirt": (112, 86, 58),
    "dirtDark": (84, 66, 46),
    "stone": (150, 150, 142),
    "stoneDark": (112, 114, 110),
    "colorTan": (200, 170, 130),
}

SOUNDS = {
    "build": ("impact-sounds", "impactWood_heavy_000"),
    "upgrade": ("impact-sounds", "impactMetal_heavy_000"),
    "sell": ("rpg-audio", "handleCoins"),
    "spell": ("impact-sounds", "impactBell_heavy_000"),
    "wave": ("interface-sounds", "bong_001"),
    "won": ("music-jingles", "jingles_STEEL00"),
    "lost": ("music-jingles", "jingles_PIZZI00"),
}

# pack: name, page, author
PACKS = {
    NATURE: ("Nature Kit", "https://kenney.nl/assets/nature-kit", "Kenney"),
    GRAVES: ("Graveyard Kit", "https://kenney.nl/assets/graveyard-kit", "Kenney"),
    TOWERS: ("Tower Defense Kit", "https://kenney.nl/assets/tower-defense-kit", "Kenney"),
    HEXAGON: (
        "KayKit Medieval Hexagon Pack 1.0",
        "https://github.com/KayKit-Game-Assets/KayKit-Medieval-Hexagon-Pack-1.0",
        "Kay Lousberg",
    ),
    MONSTERS: (
        "Ultimate Monsters",
        "https://quaternius.com/packs/ultimatemonsters.html",
        "Quaternius",
    ),
    "impact-sounds": ("Impact Sounds", "https://kenney.nl/assets/impact-sounds", "Kenney"),
    "interface-sounds": ("Interface Sounds", "https://kenney.nl/assets/interface-sounds", "Kenney"),
    "rpg-audio": ("RPG Audio", "https://kenney.nl/assets/rpg-audio", "Kenney"),
    "music-jingles": ("Music Jingles", "https://kenney.nl/assets/music-jingles", "Kenney"),
}
DATES = {HEXAGON: "2026-11-08", MONSTERS: "2026-11-08"}  # the others came on 2026-11-06

PALETTE_WIDTH = 16


def color_of(document, primitive):
    """The plain color of a part, nothing if it has a texture."""
    material = document["materials"][primitive["material"]]
    surface = material["pbrMetallicRoughness"]
    if "baseColorTexture" in surface:
        return None
    natural = NATURAL_COLORS.get(material.get("name"))
    return tuple(value / 255 for value in natural) if natural else tuple(surface["baseColorFactor"][:3])


def palette_place(mesh, color):
    """Where a color lies in the palette of the model, the texture is one pixel high."""
    palette = mesh["palette"]
    if color not in palette:
        palette.append(color)
    return [(palette.index(color) + 0.5) / PALETTE_WIDTH, 0.5]


def png_of(colors):
    """A palette as a picture. The last color fills the rest."""
    pixels = [colors[min(item, len(colors) - 1)] for item in range(PALETTE_WIDTH)]
    # The Nature Kit writes the values of its colors as they are shown, not as linear light.
    line = b"\0" + bytes(round(255 * value) for pixel in pixels for value in pixel)

    def chunk(kind, data):
        checksum = zlib.crc32(kind + data)
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", checksum)

    header = struct.pack(">IIBBBBB", PALETTE_WIDTH, 1, 8, 2, 0, 0, 0)
    return (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", header)
        + chunk(b"IDAT", zlib.compress(line))
        + chunk(b"IEND", b"")
    )


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
                mesh["normals"].append(unit(apply(matrix, normal, False)))
            uvs = read_accessor(document, binary, attributes["TEXCOORD_0"])
            color = color_of(document, primitive)
            if color is not None:
                uvs = [palette_place(mesh, color)] * len(uvs)
            mesh["uvs"].extend(uvs)
            indices = read_accessor(document, binary, primitive["indices"])
            mesh["indices"].extend(base + item[0] for item in indices)
    for child in node.get("children", []):
        collect(document, binary, child, matrix, mesh)


def document_of(pack, name):
    """A model of a pack by the end of its path: a .glb of Kenney or a .gltf with its buffer."""
    endings = (f"GLB format/{name}.glb", f"GLTF format/{name}.glb", f"/{name}.gltf")
    path = next(item for item in pack.namelist() if item.endswith(endings))
    if path.endswith(".glb"):
        return read_glb(pack.read(path))
    folder = path.rsplit("/", 1)[0]
    return read_gltf(pack.read(path), lambda uri: pack.read(f"{folder}/{uri}"))


def part_of(pack, name):
    document, binary = document_of(pack, name)
    mesh = {"positions": [], "normals": [], "uvs": [], "indices": [], "palette": []}
    for root in document["scenes"][document.get("scene", 0)]["nodes"]:
        collect(document, binary, root, IDENTITY, mesh)
    return mesh


def merged(pack, names):
    whole = {"positions": [], "normals": [], "uvs": [], "indices": [], "palette": []}
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
        whole["palette"] = part["palette"]
        height = max(place[1] for place in whole["positions"])
    return whole


def normalized(mesh, fit, others=()):
    """Scaled and moved by the bounds of the mesh. The other meshes, frames of the same model, move
    the same way, and none of them reaches below the ground."""
    places = mesh["positions"]
    lows = [min(place[axis] for place in places) for axis in range(3)]
    highs = [max(place[axis] for place in places) for axis in range(3)]
    extent = highs[1] - lows[1] if fit == "height" else max(highs[0] - lows[0], highs[2] - lows[2])
    middle_x, middle_z = (lows[0] + highs[0]) / 2, (lows[2] + highs[2]) / 2
    ground = min(place[1] for each in (mesh, *others) for place in each["positions"])
    for each in (mesh, *others):
        each["positions"] = [
            [(x - middle_x) / extent, (y - ground) / extent, (z - middle_z) / extent]
            for x, y, z in each["positions"]
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
    path = next(item for item in sorted(pack.namelist()) if item.endswith(f"/{name}.png"))
    return pack.read(path)


def import_model(folder, name, work):
    pack_name, parts, fit, texture = MODELS[name]
    with zipfile.ZipFile(folder / f"{pack_name}.zip") as pack:
        mesh = merged(pack, parts)
        normalized(mesh, fit)
        (TARGET / f"{name}.png").write_bytes(texture_of(pack, texture))
    compile_mesh(mesh, name, work)


def import_scenery(folder, name, work):
    pack_name, part = SCENERY[name]
    with zipfile.ZipFile(folder / f"{pack_name}.zip") as pack:
        mesh = merged(pack, [part])
        normalized(mesh, "height")
        picture = png_of(mesh["palette"]) if mesh["palette"] else texture_of(pack, "colormap")
    (TARGET / f"{name}.png").write_bytes(picture)
    compile_mesh(mesh, name, work)


def import_enemy(folder, name, work):
    part, walk = ENEMIES[name]
    with zipfile.ZipFile(folder / f"{MONSTERS}.zip") as pack:
        document, binary = document_of(pack, part)
    frames = walk_cycle(document, binary, walk, WALK_FRAMES)
    normalized(frames[0], "height", frames[1:])
    for frame, mesh in enumerate(frames):
        compile_mesh(mesh, f"{name}_{frame}", work)


def compile_mesh(mesh, name, work):
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
    title, url, author = PACKS[pack]
    date = DATES.get(pack, "2026-11-06")
    return f"| `{file}` | {title}, {url} | {author} | CC0 | {date} | {change} |"


def asset_list(sounds):
    lines = [
        "# Assets",
        "",
        "Alle Dateien liegen in `src/client/assets`. Erzeugt von `scripts/import_assets.py`, nicht",
        "von Hand ändern. Jedes Modell ist ein Mesh mit der Textur des Pakets, Gegner haben",
        f"{WALK_FRAMES} Meshes für einen Schritt-Zyklus (E-41). Alle Pakete stehen unter CC0.",
        "",
        "| Datei | Quelle | Urheber | Lizenz | Datum | Änderungen |",
        "|---|---|---|---|---|---|",
    ]
    for name, (pack, parts, fit, texture) in MODELS.items():
        change = f"Aus {', '.join(parts)} zu einem Mesh verbunden, auf eine Einheit skaliert"
        lines.append(row(f"{name}.mesh", pack, change))
        lines.append(row(f"{name}.png", pack, f"{texture}.png, unverändert"))
    for name, (part, walk) in ENEMIES.items():
        change = f"{part}.gltf, Animation {walk} in {WALK_FRAMES} Posen, auf eine Einheit Höhe skaliert"
        lines.append(row(f"{name}_0.mesh … {name}_{WALK_FRAMES - 1}.mesh", MONSTERS, change))
    lines.append(row("monsters.png", MONSTERS, "Atlas_Monsters.png, unverändert"))
    for name, (pack, part) in SCENERY.items():
        lines.append(row(f"{name}.mesh", pack, f"Aus {part}, auf eine Einheit Höhe skaliert"))
        lines.append(row(f"{name}.png", pack, f"Farben von {part} geändert, als Palette, oder colormap.png"))
    for name, (pack, _) in SOUNDS.items():
        lines.append(row(f"{name}.ogg", pack, f"{sounds[name]}, unverändert"))
    lines.append(
        "| `icon.png`, `src/app/qtowerdefense.ico` | eigene Zeichnung, `scripts/make_icon.py` "
        "| QtowerDefense | wie das Projekt | 2026-11-08 | – |"
    )
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
        for name in ENEMIES:
            import_enemy(folder, name, pathlib.Path(work))
            print(name)
        for name in SCENERY:
            import_scenery(folder, name, pathlib.Path(work))
            print(name)
    with zipfile.ZipFile(folder / f"{MONSTERS}.zip") as pack:
        (TARGET / "monsters.png").write_bytes(texture_of(pack, "Atlas_Monsters"))
    sounds = {name: import_sound(folder, name) for name in SOUNDS}
    (REPO / "docs" / "ASSETS.md").write_text(asset_list(sounds), encoding="utf-8", newline="\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
