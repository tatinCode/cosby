print("Smoke test: scrub 0-13000 ms and compare with the printed notes");

const IMG = "sb/dummy.png";   // <project>/assets/sb/dummy.png
const S = 0.1;                // 2000 px * 0.1 = 200 px

// ---------------------------------------------------------------
// 1) Main sprite: one feature per second
// ---------------------------------------------------------------
const s = SceneApi.sprite(IMG, { layer: "Foreground", origin: "Center", position: [320, 240] });

s.scale({ easing: "Linear", startTime: 0, endTime: 0, startValue: S, endValue: S });

print("0-1000     fade in at the centre, 200 px");
s.fade({ easing: "Linear", startTime: 0, endTime: 1000, startValue: 0, endValue: 1 });

print("1000-2000  move left -> right, speeding up (In)");
s.move({ easing: "In", startTime: 1000, endTime: 2000, startPosition: [120, 240], endPosition: [520, 240] });

print("2000-3000  move back to the centre, slowing down (Out)");
s.move({ easing: "Out", startTime: 2000, endTime: 3000, startPosition: [520, 240], endPosition: [320, 240] });

print("3000-4000  grow 200 -> 300 px");
s.scale({ easing: "Out", startTime: 3000, endTime: 4000, startValue: S, endValue: S * 1.5 });

print("4000-5000  one full turn");
s.rotate({ easing: "Linear", startTime: 4000, endTime: 5000, startValue: 0, endValue: 6.28318 });

print("5000-6000  tint red -> blue");
s.color({ easing: "Linear", startTime: 5000, endTime: 6000, startValue: [255, 0, 0], endValue: [0, 128, 255] });

print("6000-7000  moveX right + moveY up at the same time");
s.moveX({ easing: "Linear", startTime: 6000, endTime: 7000, startValue: 320, endValue: 450 });
s.moveY({ easing: "Linear", startTime: 6000, endTime: 7000, startValue: 240, endValue: 150 });

print("7000-8000  scaleVec stretches it wide and flat (x2 wide, x0.5 tall, on top of scale)");
s.scaleVec({ easing: "Linear", startTime: 7000, endTime: 8000, startValue: [1, 1], endValue: [2, 0.5] });

// Tilt it so flips are visible even on a symmetric image.
    s.rotate({ easing: "Linear", startTime: 8000, endTime: 8000, startValue: 0.5, endValue: 0.5 });
print("8000       tilts to ~29 degrees");
print("8500-9000  flipH: the tilt mirrors the other way, then back");
s.flipH({ startTime: 8500, endTime: 9000 });
print("9500-10000 flipV: mirrors again, then back");
s.flipV({ startTime: 9500, endTime: 10000 });

print("11000+     flipH stays on for good (endTime 0 = permanent)");
s.flipH({ startTime: 11000, endTime: 0 });

print("12000-13000 fade out; after 13000 it's gone");
s.fade({ easing: "Linear", startTime: 12000, endTime: 13000, startValue: 1, endValue: 0 });

// ---------------------------------------------------------------
// 2) Additive: two half-transparent sprites overlap at 10000-11000
//    and the overlap glows brighter than either one
// ---------------------------------------------------------------
print("10000-11000 two sprites at the bottom; their overlap glows (additive)");
for (const x of [260, 380]) {
    const a = SceneApi.sprite(IMG, { layer: "Foreground", origin: "Center", position: [x, 380] });
    a.scale({ easing: "Linear", startTime: 10000, endTime: 10000, startValue: S, endValue: S });
    a.fade({ easing: "Linear", startTime: 10000, endTime: 11000, startValue: 0.6, endValue: 0.6 });
    a.additive({ startTime: 10000, endTime: 11000 });
}

// ---------------------------------------------------------------
// 3) Layers: created foreground first, but the background still
//    draws behind it (0-13000, top left corner)
// ---------------------------------------------------------------
const front = SceneApi.sprite(IMG, { layer: "Foreground", origin: "Center", position: [80, 80] });
front.scale({ easing: "Linear", startTime: 0, endTime: 13000, startValue: S * 0.5, endValue: S * 0.5 });
front.color({ easing: "Linear", startTime: 0, endTime: 0, startValue: [80, 255, 80], endValue: [80, 255, 80] });

const back = SceneApi.sprite(IMG, { layer: "Background", origin: "Center", position: [80, 80] });
back.scale({ easing: "Linear", startTime: 0, endTime: 13000, startValue: S, endValue: S });
back.color({ easing: "Linear", startTime: 0, endTime: 0, startValue: [255, 80, 80], endValue: [255, 80, 80] });
print("top left: small green square on top of a bigger red one");

// ---------------------------------------------------------------
// 4) Origins: three sprites on the same point (560, 80)
// ---------------------------------------------------------------
for (const origin of ["TopLeft", "Center", "BottomRight"]) {
    const o = SceneApi.sprite(IMG, { layer: "Overlay", origin, position: [560, 80] });
    o.scale({ easing: "Linear", startTime: 0, endTime: 13000, startValue: S * 0.3, endValue: S * 0.3 });
    o.fade({ easing: "Linear", startTime: 0, endTime: 0, startValue: 0.5, endValue: 0.5 });
}
print("top right: TopLeft hangs down-right of (560,80), BottomRight up-left, Center on it");

// ---------------------------------------------------------------
// 5) Widescreen: x = -60 is outside 4:3, left of the dashed line
// ---------------------------------------------------------------
const wide = SceneApi.sprite(IMG, { layer: "Foreground", origin: "Center", position: [-60, 240] });
wide.scale({ easing: "Linear", startTime: 0, endTime: 13000, startValue: S * 0.4, endValue: S * 0.4 });
print("far left, outside the dashed line: only widescreen players see this");

// ---------------------------------------------------------------
// 6) Lifetime: only exists 3000-4000, invisible before and after
// ---------------------------------------------------------------
const blink = SceneApi.sprite(IMG, { layer: "Foreground", origin: "Center", position: [320, 60] });
blink.scale({ easing: "Linear", startTime: 3000, endTime: 4000, startValue: S * 0.3, endValue: S * 0.3 });
print("3000-4000  a small square at the top centre, gone outside that range");

// ---------------------------------------------------------------
// 7) Not shown: no commands (osu! skips it), and a missing image
// ---------------------------------------------------------------
SceneApi.sprite(IMG, { position: [320, 440] });   // never visible
const missing = SceneApi.sprite("sb/missing.png", { position: [600, 440] });
missing.fade({ easing: "Linear", startTime: 0, endTime: 13000, startValue: 1, endValue: 1 });
print("bottom right: 32 px placeholder outline for sb/missing.png");

print("Smoke test end");
