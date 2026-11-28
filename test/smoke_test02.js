// Cosby PR 2 test: animations, loops, triggers, easings, samples, warnings.
// Timeline: 0-10 s.

const smoke = "Black smoke/blackSmoke.png";
const smokeStill = "Black smoke/blackSmoke12.png";

// 1. Background (1920x1080 scaled to the 854x480 view), tinted blue 6-8 s
SceneApi.sprite("minji-newjeans-omg-hd-wallpaper-uhdpaper.com-237@0@i.jpg",
        { layer: "Background", name: "background" })
    .scale({ startTime: 0, endTime: 10000, startValue: 480 / 1080, endValue: 480 / 1080 })
    .fade({ startTime: 0, endTime: 10000, startValue: 1, endValue: 1 })
    .color({ startTime: 6000, endTime: 7000, startValue: [255, 255, 255], endValue: [120, 140, 255], easing: "SineInOut" })
    .color({ startTime: 7000, endTime: 8000, startValue: [120, 140, 255], endValue: [255, 255, 255], easing: "SineInOut" });

// 2. Looping animation: 25 frames x 40 ms = 1 s per cycle, rising for 6 s
SceneApi.animation(smoke, { frameCount: 25, frameDelay: 40, position: [140, 400], name: "rising smoke" })
    .fade({ startTime: 0, endTime: 500, startValue: 0, endValue: 0.9 })
    .moveY({ startTime: 0, endTime: 6000, startValue: 400, endValue: 180, easing: "SineOut" })
    .fade({ startTime: 5000, endTime: 6000, startValue: 0.9, endValue: 0 });

// 3. Play-once animation: 25 x 60 ms = 1.5 s from 1 s, then holds the last frame
SceneApi.animation(smoke, { frameCount: 25, frameDelay: 60, loopType: "LoopOnce", position: [520, 320], name: "puff once" })
    .scale({ startTime: 1000, endTime: 2500, startValue: 0.5, endValue: 1.2, easing: "QuadOut" })
    .fade({ startTime: 1000, endTime: 3500, startValue: 1, endValue: 0, easing: "QuadIn" });

// 4. Easing race 2-4 s: same move, different curves (one by osu! number)
[["Linear", 60], ["QuadOut", 110], ["BackOut", 160], ["ElasticOut", 210], [33, 260]]
    .forEach(([easing, y]) => {
        SceneApi.sprite(smokeStill, { position: [60, y], name: `race ${easing}` })
            .scale({ startTime: 2000, endTime: 4500, startValue: 0.15, endValue: 0.15 })
            .fade({ startTime: 2000, endTime: 4500, startValue: 1, endValue: 1 })
            .moveX({ startTime: 2000, endTime: 4000, startValue: 60, endValue: 580, easing });
    });

// 5. Loop: 6 pulses of 500 ms from 4.5 s, plus a trigger (timeline/export only)
SceneApi.sprite(smokeStill, { position: [320, 240], name: "pulse" })
    .fade({ startTime: 4500, endTime: 4500, startValue: 1, endValue: 1 })
    .loop(4500, 6, l => l
        .scale({ startTime: 0, endTime: 250, startValue: 0.3, endValue: 0.45, easing: "SineOut" })
        .scale({ startTime: 250, endTime: 500, startValue: 0.45, endValue: 0.3, easing: "SineIn" }))
    .fade({ startTime: 7500, endTime: 8000, startValue: 1, endValue: 0 })
    .trigger("HitSoundClap", 4500, 8000, t => t
        .color({ startTime: 0, endTime: 150, startValue: [255, 80, 80], endValue: [255, 255, 255] }));

// 6. Sample: there's no hit.wav, so Output should warn about this line
SceneApi.sample(1000, "hit.wav", { volume: 70 });

// 7. Missing image: magenta box 8-10 s, plus a warning about this line
SceneApi.sprite("missing.png", { position: [520, 120], name: "missing" })
    .fade({ startTime: 8000, endTime: 10000, startValue: 1, endValue: 1 });
