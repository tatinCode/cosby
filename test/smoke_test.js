print("Smoke test start");
const s = SceneApi.sprite("test/assets/dummy.png", {
    layer: "Foreground",
    origin: "Center",
    position: [320, 240]
});

// Core commands
s.scale({
    easing: "Linear",
    startTime: 0,
    endTime: 500,
    startValue: 0.02,
    endValue: 0.02
});

s.fade({ 
    easing: "Linear", 
    startTime: 0, 
    endTime: 1000, 
    startValue: 0.0, 
    endValue: 1.0 
});

s.move({ 
    easing: "In", 
    startTime: 0, 
    endTime: 1000, 
    startPosition: [100, 100], 
    endPosition: [540, 380] 
});

s.scale({ 
    easing: "Out", 
    startTime: 500, 
    endTime: 1500, 
    startValue: 0.02, 
    endValue: 0.08 
});

s.rotate({ 
    easing: "Linear", 
    startTime: 0, 
    endTime: 2000, 
    startValue: 0.0, 
    endValue: 6.28318 
}); // 2pi

s.color({
    easing: "Linear",
    startTime: 0,
    endTime: 2000,
    startValue: [255, 0, 0],
    endValue: [0, 128, 255]
});


// Axis-specific + vector scale
s.moveX({ 
    easing: "Linear", 
    startTime: 2000, 
    endTime: 3000, 
    startValue: 320, 
    endValue: 500 
});

s.moveY({ 
    easing: "Linear", 
    startTime: 2000, 
    endTime: 3000, 
    startValue: 240, 
    endValue: 120 
});

s.scaleVec({ 
    easing: "Linear", 
    startTime: 2000, 
    endTime: 3000, 
    startValue: [0.08, 0.04], 
    endValue: [0.16, 0.12] 
});

// Timed flip/additive window
s.flipH({ 
    startTime: 3000, 
    endTime: 3500 
});
s.flipV({ 
    startTime: 3500, 
    endTime: 4000 
});
s.additive({ 
    startTime: 3000, 
    endTime: 4500 
});

// Persistent toggle-style (endTime <= startTime => persistent in your current logic)
s.flipH({ startTime: 5000, endTime: 0 });

s.additive({ startTime: 5500, endTime: 0 });
print("Smoke test end");

//Quick mini-tests you can also run:
//1) Defaults test
//
    SceneApi.sprite("a.png", {});

//2) Guard test (x/y non-number should not override defaults)
SceneApi.sprite("b.png", { 
    x: "oops", 
    y: null, 
    position: [123, 456] 
});

//3) Case-insensitive layer/origin test
SceneApi.sprite("c.png", { 
    layer: "overlay", 
    origin: "bottomright", 
    x: 320, 
    y: 240 
});
