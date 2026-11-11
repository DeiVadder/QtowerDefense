# Changelog

All notable changes to QtowerDefense. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), versions follow
[Semantic Versioning](https://semver.org/).

## [1.0.0] - 2026-11-11

The first release: a complete fantasy tower defense game in 3D for Windows, macOS, Linux and the
browser.

### Game

- **Campaign** on a world map in the fog: nine sections with their own landscapes (meadow, mire,
  forest, heath, badlands, highlands, barrows, frost and ashland), up to three stars per map, and
  new sections that open as you win.
- **Endless mode:** the waves keep coming and grow stronger, with a best score for each section.
- **7 tower types** with two upgrades each, selling for a partial refund, and a target priority
  for each tower (first, last, strongest, nearest).
- **Every tower shoots its own way:** bolts, a volley of arrows, a glob of venom, a boulder that
  lands in dust, fireballs and ice shards, and the tower gives way as it fires.
- **8 enemy types and 2 bosses** with damage types, resistances, slowing, poison and regeneration.
- **Cards and dice:** towers come as cards, roll the dice for new ones, keep up to five in your
  hand and drag one onto a free field of the grid; towers turn in quarter steps.
- **A hero** with moods and remarks, and two spells, Meteor and Deep Freeze, with a cooldown.
- **Flow:** call a wave early for bonus gold, play at 1x/2x/3x speed or pause, save a running game
  and resume it later.
- **A guide in the first game** shows how to roll, choose, turn and build, and waits for you.
- **Controls** by mouse or touch, three control sizes and three levels of picture quality.
- **Languages:** English and German.
- **Sound** for building, upgrades, selling, spells, waves and the end of a game.

### Platforms

- **Windows** (x64), **macOS** (Apple Silicon and Intel, macOS 14.4 or newer) and **Linux** (x64)
  as self-contained packages with Qt.
- **Browser** (WebAssembly) on GitHub Pages; progress and settings stay in the browser.

### Under the hood

- Deterministic simulation with a fixed tick, fixed-point numbers and its own random number
  generator: the same seed and the same commands always give the same game.
- Replays and golden replays check the rules exactly; bots (`naive`, `good`, `fuzz`) and a JSON
  control interface play the same commands as a person.
- `qtd_headless` runs games without a window, for tests, balancing and benchmarks.

### Known limits

- The packages are not signed: Windows SmartScreen and macOS Gatekeeper warn on the first start
  (see the README).
- iOS and Android are prepared in CMake but not built.
- The browser download is about 53 MB.

[1.0.0]: https://github.com/DeiVadder/QtowerDefense/releases/tag/v1.0.0
