// The icons of the HUD as SVG paths in a box of 64 by 64. Holes come from the odd-even fill rule.
.pragma library

const flame = "M32 4c4 14 20 20 20 36 0 12-9 20-20 20S12 52 12 40c0-8 5-12 8-18 2 6 4 8 7 9"
            + " 1-9 2-18 5-27Z M32 36c-6 6-8 9-8 12a8 8 0 0 0 16 0c0-3-2-6-8-12Z";
const spire = "M32 4l10 20-6 34h-8l-6-34Z M12 22l8 10 2 26H14L7 34Z M52 22l-8 10-2 26h8l7-24Z";
const ballista = "M6 14c14-8 38-8 52 0l-4 8c-12-6-32-6-44 0Z M28 18h8v38h-8Z"
               + " M12 22l20 14 20-14 3 5-23 16L9 27Z";

const paths = {
    "dice": "M12 8h40a4 4 0 0 1 4 4v40a4 4 0 0 1-4 4H12a4 4 0 0 1-4-4V12a4 4 0 0 1 4-4Z"
            + " M21 15a6 6 0 1 0 .01 0Z M43 15a6 6 0 1 0 .01 0Z M32 26a6 6 0 1 0 .01 0Z"
            + " M21 37a6 6 0 1 0 .01 0Z M43 37a6 6 0 1 0 .01 0Z",
    "bag": "M22 6h20l-5 10h7c10 8 14 20 14 30 0 8-6 12-26 12S6 54 6 46c0-10 4-22 14-30h7Z"
           + " M32 26a10 10 0 1 0 .01 0Z",
    "trash": "M24 5h16v6h14v8H10v-8h14Z M14 23h36l-4 36H18Z M24 30v22h5V30Z M35 30v22h5V30Z",
    "close": "M14 6l18 18L50 6l8 8-18 18 18 18-8 8-18-18-18 18-8-8 18-18L6 14Z",
    "coin": "M32 6a26 26 0 1 0 .01 0Z M32 15a17 17 0 1 1-.01 0Z M26 23h12v18H26Z",
    "heart": "M32 57C8 39 5 27 8 20c4-10 18-12 24 0 6-12 20-10 24 0 3 7 0 19-24 37Z",
    "skull": "M32 6C18 6 10 16 10 28c0 8 4 14 10 18v12h24V46c6-4 10-10 10-18C54 16 46 6 32 6Z"
             + " M22 22a6 6 0 1 0 .01 0Z M42 22a6 6 0 1 0 .01 0Z M29 40h6l-3-8Z",
    "pause": "M14 10h13v44H14Z M37 10h13v44H37Z",
    "play": "M16 8l38 24-38 24Z",
    "fast": "M6 10l26 22L6 54Z M32 10l26 22-26 22Z",
    "flag": "M12 4h7v56h-7Z M19 8c12-6 22 6 36 0v28c-14 6-24-6-36 0Z",
    "trash": "M24 5h16v6h14v8H10v-8h14Z M14 23h36l-4 36H18Z M24 30v22h5V30Z M35 30v22h5V30Z",
    "sword": "M50 6h8v8L30 42l6 6-6 6-7-7-9 9-6-6 9-9-7-7 6-6 6 6Z",
    "bolt": "M38 4L12 36h16l-6 24 30-36H34Z",
    "hourglass": "M14 6h36v7l-13 19 13 19v7H14v-7l13-19-13-19Z M23 13l9 13 9-13Z",
    "target": "M32 4a28 28 0 1 0 .01 0Z M32 12a20 20 0 1 1-.01 0Z M32 22a10 10 0 1 0 .01 0Z",
    "up": "M32 6l24 26H42v26H22V32H8Z",
    "rotate": "M32 8a24 24 0 1 1-22 14l8 4a15 15 0 1 0 14-9v9L18 13 32 0Z",
    "save": "M8 8h40l8 8v40H8Z M18 8v14h24V8Z M18 34v22h28V34Z",
    "load": "M8 12h18l5 7h25v37H8Z",

    "ballista": ballista,
    "twin_ballista": "M6 8c14-6 38-6 52 0l-3 7c-12-5-34-5-46 0Z M6 26c14-6 38-6 52 0l-3 7c-12-5-34-5-46 0Z"
                     + " M28 12h8v10h-8Z M28 30h8v26h-8Z",
    "bastion": "M12 8h9v7h6V8h10v7h6V8h9v17l-4 4v29H16V29l-4-4Z M27 35v14h10V35Z",
    "horseshoe": "M14 58V30a18 18 0 0 1 36 0v28H37V30a5 5 0 0 0-10 0v28Z",
    "frost_spire": spire,
    "venom_spire": "M32 4c10 16 20 26 20 38a20 20 0 0 1-40 0c0-12 10-22 20-38Z"
                   + " M22 40c0 8 4 12 10 12v-5c-3 0-5-3-5-7Z",
    "catapult": "M6 50h52v8H6Z M12 50l8-16h6l-6 16Z M38 50l-6-16h6l8 16Z M12 38L48 12l4 6-36 26Z"
                + " M50 4a7 7 0 1 0 .01 0Z",

    "meteor": flame,
    "deep_freeze": "M30 3L34 3L36 25L56 16L58 19L40 32L58 45L56 48L36 39L34 61L30 61L28 39L8 48L6 45"
                   + "L24 32L6 19L8 16L28 25Z M45 10L45 16L40 18L40 12Z M58 32L53 35L48 32L53 29Z"
                   + " M45 54L40 52L40 46L45 48Z M19 54L19 48L24 46L24 52Z M6 32L11 29L16 32L11 35Z"
                   + " M19 10L24 12L24 18L19 16Z"
};

function known(name) {
    return name in paths;
}

// New content without an icon of its own still gets one.
function pathOf(name) {
    return paths[name] ?? paths.bolt;
}
