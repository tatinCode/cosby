// Types for the Cosby script API (editor autocomplete only; scripts are plain JavaScript).
// Keep in sync with js_bridge, js_sprite_proxy and the bootstrap in script_host.cpp.

/** Easing curve. Unknown names fall back to "Linear". */
type CosbyEasing = "Linear" | "In" | "Out" | (string & {});

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

/** A storyboard sprite. Every command returns the sprite, so calls can be chained. */
interface CosbySprite {
    /** Opacity, 0-1. Default 0 → 0. */
    fade(command: CosbyValueCommand): CosbySprite;
    fade(easing: CosbyEasing, startTime: number, endTime: number, startValue: number, endValue: number): CosbySprite;

    move(command: CosbyMoveCommand): CosbySprite;
    move(easing: CosbyEasing, startTime: number, endTime: number,
        startX: number, startY: number, endX: number, endY: number): CosbySprite;

    /** Horizontal position. Defaults to the sprite's x. */
    moveX(command: CosbyValueCommand): CosbySprite;
    /** Vertical position. Defaults to the sprite's y. */
    moveY(command: CosbyValueCommand): CosbySprite;

    /** Uniform scale. Default 1 → 1. */
    scale(command: CosbyValueCommand): CosbySprite;
    scale(easing: CosbyEasing, startTime: number, endTime: number, startValue: number, endValue: number): CosbySprite;

    /** Separate x/y scale. */
    scaleVec(command: CosbyVectorCommand): CosbySprite;

    /** Rotation in radians. */
    rotate(command: CosbyValueCommand): CosbySprite;
    rotate(easing: CosbyEasing, startTime: number, endTime: number, startValue: number, endValue: number): CosbySprite;

    color(command: CosbyColorCommand): CosbySprite;
    /** Constant color. */
    color(easing: CosbyEasing, startTime: number, endTime: number, r: number, g: number, b: number): CosbySprite;

    /** Mirror horizontally. endTime <= startTime makes it permanent from startTime. */
    flipH(command: CosbyTimed): CosbySprite;
    /** Mirror vertically. endTime <= startTime makes it permanent from startTime. */
    flipV(command: CosbyTimed): CosbySprite;
    /** Additive blending. endTime <= startTime makes it permanent from startTime. */
    additive(command: CosbyTimed): CosbySprite;
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
}

declare const SceneApi: CosbySceneApi;

/** Prints its arguments, separated by spaces. */
declare function print(...values: any[]): void;

declare const console: {
    log(...values: any[]): void;
    warn(...values: any[]): void;
    error(...values: any[]): void;
};
