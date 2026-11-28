// Types for the Cosby script API (editor autocomplete only; scripts are plain JavaScript).
// Keep in sync with js_bridge, js_sprite_proxy and the bootstrap in script_host.cpp.

/**
 * Easing curve: one of osu!'s 35, by name (any case) or by osu!'s number
 * (0-34). "In"/"Out" are osu!'s old names for QuadIn/QuadOut. Unknown names
 * stop the script with an error.
 */
type CosbyEasing =
    | "Linear" | "Out" | "In"
    | "QuadIn" | "QuadOut" | "QuadInOut"
    | "CubicIn" | "CubicOut" | "CubicInOut"
    | "QuartIn" | "QuartOut" | "QuartInOut"
    | "QuintIn" | "QuintOut" | "QuintInOut"
    | "SineIn" | "SineOut" | "SineInOut"
    | "ExpoIn" | "ExpoOut" | "ExpoInOut"
    | "CircIn" | "CircOut" | "CircInOut"
    | "ElasticIn" | "ElasticOut" | "ElasticHalfOut" | "ElasticQuarterOut" | "ElasticInOut"
    | "BackIn" | "BackOut" | "BackInOut"
    | "BounceIn" | "BounceOut" | "BounceInOut"
    | number;

/**
 * When a trigger fires: "Passing", "Failing", or "HitSound" plus optional
 * sample set, additions sample set, addition and custom sample set number,
 * e.g. "HitSoundClap", "HitSoundSoftWhistle", "HitSoundDrumNormalFinish1".
 */
type CosbyTrigger =
    | "Passing" | "Failing" | "HitSound"
    | "HitSoundWhistle" | "HitSoundFinish" | "HitSoundClap"
    | "HitSoundNormal" | "HitSoundSoft" | "HitSoundDrum"
    | (string & {});

/** Storyboard layer (case-insensitive). Unknown names fall back to "Foreground". */
type CosbyLayer = "Background" | "Foreground" | "Pass" | "Fail" | "Overlay" | (string & {});

/** Sprite origin (case-insensitive). Unknown names fall back to "Center". */
type CosbyOrigin =
    | "TopLeft" | "TopCenter" | "TopRight"
    | "CenterLeft" | "Center" | "CenterRight"
    | "BottomLeft" | "BottomCenter" | "BottomRight"
    | "Centre" | "Middle" | "CenterCenter"
    | (string & {});

type CosbyVec2 = [number, number];
type CosbyRgb = [number, number, number];

interface CosbySpriteOptions {
    /** Default "Foreground". */
    layer?: CosbyLayer;
    /** Default "Center". */
    origin?: CosbyOrigin;
    /** Default [320, 240]. */
    position?: CosbyVec2;
    /** Overrides position[0]. */
    x?: number;
    /** Overrides position[1]. */
    y?: number;
    /** Label shown in the timeline instead of the file name. */
    name?: string;
}

interface CosbyAnimationOptions extends CosbySpriteOptions {
    /** Number of frames, at least 1. */
    frameCount: number;
    /** Milliseconds per frame, above 0. */
    frameDelay: number;
    /** Default "LoopForever". "LoopOnce" stops on the last frame. */
    loopType?: "LoopForever" | "LoopOnce";
}

interface CosbySampleOptions {
    /** Default "Foreground". Not "Overlay". Fail/Pass play only in that state. */
    layer?: "Background" | "Foreground" | "Pass" | "Fail";
    /** 0-100. Default 100. */
    volume?: number;
}

interface CosbyTimed {
    /** Milliseconds. Default 0. */
    startTime?: number;
    /** Milliseconds. Default 0. */
    endTime?: number;
}

interface CosbyEased extends CosbyTimed {
    /** Default "Linear". */
    easing?: CosbyEasing;
}

interface CosbyValueCommand extends CosbyEased {
    startValue?: number;
    endValue?: number;
}

interface CosbyMoveCommand extends CosbyEased {
    startPosition?: CosbyVec2;
    endPosition?: CosbyVec2;
    /** Used when startPosition is missing (falls back to x, then 0). */
    startX?: number;
    startY?: number;
    /** Used when endPosition is missing (falls back to the start). */
    endX?: number;
    endY?: number;
    x?: number;
    y?: number;
}

interface CosbyVectorCommand extends CosbyEased {
    /** Default [1, 1]. */
    startValue?: CosbyVec2;
    /** Default [1, 1]. */
    endValue?: CosbyVec2;
}

interface CosbyColorCommand extends CosbyEased {
    /** 0-255 per channel. Default white. */
    startValue?: CosbyRgb;
    /** Defaults to startValue. */
    endValue?: CosbyRgb;
    /** Constant color, used when endValue is missing. */
    r?: number;
    g?: number;
    b?: number;
}

/** Commands of a sprite, loop or trigger. Each returns its target, so calls can be chained. */
interface CosbyCommands {
    /** Opacity, 0-1. Default 0 → 0. */
    fade(command: CosbyValueCommand): this;
    fade(easing: CosbyEasing, startTime: number, endTime: number, startValue: number, endValue: number): this;

    move(command: CosbyMoveCommand): this;
    move(easing: CosbyEasing, startTime: number, endTime: number,
        startX: number, startY: number, endX: number, endY: number): this;

    /** Horizontal position. Defaults to the sprite's x. */
    moveX(command: CosbyValueCommand): this;
    /** Vertical position. Defaults to the sprite's y. */
    moveY(command: CosbyValueCommand): this;

    /** Uniform scale. Default 1 → 1. */
    scale(command: CosbyValueCommand): this;
    scale(easing: CosbyEasing, startTime: number, endTime: number, startValue: number, endValue: number): this;

    /** Separate x/y scale. */
    scaleVec(command: CosbyVectorCommand): this;

    /** Rotation in radians. */
    rotate(command: CosbyValueCommand): this;
    rotate(easing: CosbyEasing, startTime: number, endTime: number, startValue: number, endValue: number): this;

    color(command: CosbyColorCommand): this;
    /** Constant color. */
    color(easing: CosbyEasing, startTime: number, endTime: number, r: number, g: number, b: number): this;

    /** Mirror horizontally. endTime <= startTime makes it permanent from startTime. */
    flipH(command: CosbyTimed): this;
    /** Mirror vertically. endTime <= startTime makes it permanent from startTime. */
    flipV(command: CosbyTimed): this;
    /** Additive blending. endTime <= startTime makes it permanent from startTime. */
    additive(command: CosbyTimed): this;
}

/** A storyboard sprite. */
interface CosbySprite extends CosbyCommands {
    /**
     * Plays the commands added by `body` `count` times in a row (osu!'s L).
     * Their times are relative to `start`; one pass lasts from the earliest
     * command start to the latest end. Loops can't contain loops or triggers.
     * @example s.loop(1000, 8, l => l.fade({ startTime: 0, endTime: 250, startValue: 1, endValue: 0 }))
     */
    loop(start: number, count: number, body: (loop: CosbyCommands) => void): this;

    /**
     * Plays the commands added by `body` each time `trigger` fires between
     * `start` and `end` (osu!'s T); their times are relative to that moment.
     * Triggers need gameplay, so the preview doesn't play them. Triggers with
     * the same `group` (above 0) cancel each other's running commands.
     * @example s.trigger("HitSoundClap", 0, 60000, t => t.scale({ startTime: 0, endTime: 100, startValue: 1.2, endValue: 1 }))
     */
    trigger(trigger: CosbyTrigger, start: number, end: number,
        body: (trigger: CosbyCommands) => void, group?: number): this;
}

interface CosbySceneApi {
    /**
     * Adds a sprite. `path` is relative to the beatmap folder, e.g. "sb/star.png".
     * Cosby looks for it in the project's assets/ folder first, then in the
     * beatmap copy; export copies images from assets/ into the beatmap folder.
     * @example SceneApi.sprite("sb/star.png", { layer: "Foreground", position: [320, 240] })
     */
    sprite(path: string, options?: CosbySpriteOptions): CosbySprite;

    add_sprite(path: string, layer: CosbyLayer, origin: CosbyOrigin, x: number, y: number): CosbySprite;

    /**
     * Adds an animation. Frame files have the frame number before the
     * extension: "sb/flame.png" with 3 frames uses sb/flame0.png,
     * sb/flame1.png and sb/flame2.png. Frames count from the first command.
     * @example SceneApi.animation("sb/flame.png", { frameCount: 8, frameDelay: 50 })
     */
    animation(path: string, options: CosbyAnimationOptions): CosbySprite;

    /**
     * Plays a sound once at `time` (ms). `path` is relative to the beatmap
     * folder, like images. Export copies it; the preview doesn't play it yet.
     * @example SceneApi.sample(1500, "sb/hit.wav", { volume: 70 })
     */
    sample(time: number, path: string, options?: CosbySampleOptions): void;
}

declare const SceneApi: CosbySceneApi;

/** Prints its arguments, separated by spaces. */
declare function print(...values: any[]): void;

declare const console: {
    log(...values: any[]): void;
    warn(...values: any[]): void;
    error(...values: any[]): void;
};
