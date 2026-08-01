import { readFile } from "node:fs/promises";
import { fileURLToPath } from "node:url";

import { GifReader } from "omggif";

const DISPLAY_WIDTH = 320;
const DISPLAY_HEIGHT = 240;
const FRAME_COUNT_MAX = 32;
const SHEET_HEIGHT_MAX = 4096;
const DEFAULT_FPS = 10;
const LOAD_TIMEOUT_MS = 10000;

function integer(value, fallback, minimum, maximum, name) {
  const result = value === undefined ? fallback : Number(value);
  if (!Number.isInteger(result) ||
      result < minimum || result > maximum) {
    throw new Error(
      `${name} must be an integer between ${minimum} and ${maximum}`,
    );
  }
  return result;
}

function parseBackground(value) {
  const color = value ?? "#000000";
  if (!/^#[0-9a-f]{6}$/i.test(color)) {
    throw new Error("animation background must be #RRGGBB");
  }
  const rgb = Number.parseInt(color.slice(1), 16);
  return [
    (rgb >>> 16) & 0xff,
    (rgb >>> 8) & 0xff,
    rgb & 0xff,
  ];
}

function outputSize(options, sourceWidth, sourceHeight) {
  const width = integer(
    options.width, sourceWidth, 1, DISPLAY_WIDTH, "animation width",
  );
  const height = integer(
    options.height, sourceHeight, 1, DISPLAY_HEIGHT, "animation height",
  );
  return { width, height };
}

function sourceRect(
  sourceWidth, sourceHeight, outputWidth, outputHeight, fit,
) {
  if (fit === "fill") {
    return {
      x: 0,
      y: 0,
      width: outputWidth,
      height: outputHeight,
    };
  }
  if (fit !== undefined && fit !== "contain") {
    throw new Error('animation fit must be "contain" or "fill"');
  }
  const scale = Math.min(
    outputWidth / sourceWidth,
    outputHeight / sourceHeight,
  );
  const width = Math.max(1, Math.round(sourceWidth * scale));
  const height = Math.max(1, Math.round(sourceHeight * scale));
  return {
    x: Math.floor((outputWidth - width) / 2),
    y: Math.floor((outputHeight - height) / 2),
    width,
    height,
  };
}

function resizeRgba(
  source, sourceWidth, sourceHeight,
  outputWidth, outputHeight, fit,
) {
  if (sourceWidth === outputWidth &&
      sourceHeight === outputHeight &&
      (fit === undefined || fit === "contain" || fit === "fill")) {
    return Uint8Array.from(source);
  }
  const destination = new Uint8Array(outputWidth * outputHeight * 4);
  const rectangle = sourceRect(
    sourceWidth, sourceHeight, outputWidth, outputHeight, fit,
  );
  for (let y = 0; y < rectangle.height; y++) {
    const sourceY = Math.min(
      sourceHeight - 1,
      Math.floor(y * sourceHeight / rectangle.height),
    );
    for (let x = 0; x < rectangle.width; x++) {
      const sourceX = Math.min(
        sourceWidth - 1,
        Math.floor(x * sourceWidth / rectangle.width),
      );
      const from = (sourceY * sourceWidth + sourceX) * 4;
      const to = (
        (rectangle.y + y) * outputWidth + rectangle.x + x
      ) * 4;
      destination[to] = source[from];
      destination[to + 1] = source[from + 1];
      destination[to + 2] = source[from + 2];
      destination[to + 3] = source[from + 3];
    }
  }
  return destination;
}

function rgbaToRgb565(frame, background) {
  const result = Buffer.alloc(frame.length / 2);
  for (let source = 0, target = 0;
       source < frame.length;
       source += 4, target += 2) {
    const alpha = frame[source + 3];
    const inverse = 255 - alpha;
    const red = Math.round(
      (frame[source] * alpha + background[0] * inverse) / 255,
    );
    const green = Math.round(
      (frame[source + 1] * alpha + background[1] * inverse) / 255,
    );
    const blue = Math.round(
      (frame[source + 2] * alpha + background[2] * inverse) / 255,
    );
    result.writeUInt16LE(
      ((red >> 3) << 11) |
      ((green >> 2) << 5) |
      (blue >> 3),
      target,
    );
  }
  return result;
}

function frameSchedule(durationMs, options) {
  const fps = integer(
    options.fps, DEFAULT_FPS, 1, 30, "animation fps",
  );
  const requestedMaximum = integer(
    options.maxFrames,
    FRAME_COUNT_MAX,
    1,
    FRAME_COUNT_MAX,
    "animation maxFrames",
  );
  const frameDuration = Math.max(
    Math.ceil(1000 / fps),
    Math.ceil(durationMs / requestedMaximum),
    10,
  );
  const frameCount = Math.max(
    1,
    Math.min(
      requestedMaximum,
      Math.ceil(durationMs / frameDuration),
    ),
  );
  return {
    frameCount,
    frameDuration: Math.min(frameDuration, 60000),
    times: Array.from(
      { length: frameCount },
      (_unused, index) => Math.min(
        durationMs - 1,
        index * frameDuration,
      ),
    ),
  };
}

function clearGifRectangle(canvas, width, frame) {
  for (let y = frame.y; y < frame.y + frame.height; y++) {
    canvas.fill(
      0,
      (y * width + frame.x) * 4,
      (y * width + frame.x + frame.width) * 4,
    );
  }
}

export function convertGifBuffer(data, options = {}) {
  const reader = new GifReader(data);
  const sourceWidth = reader.width;
  const sourceHeight = reader.height;
  const frameTotal = reader.numFrames();
  if (frameTotal < 1 || frameTotal > 2048) {
    throw new Error("GIF must contain 1-2048 frames");
  }
  const durations = [];
  let durationMs = 0;
  for (let index = 0; index < frameTotal; index++) {
    const delay = Math.max(1, reader.frameInfo(index).delay || 10) * 10;
    durations.push(delay);
    durationMs += delay;
  }
  const schedule = frameSchedule(durationMs, options);
  const size = outputSize(options, sourceWidth, sourceHeight);
  if (size.height * schedule.frameCount > SHEET_HEIGHT_MAX) {
    throw new Error(
      "animation frames exceed the 4096-pixel sprite-sheet height",
    );
  }
  const canvas = new Uint8Array(sourceWidth * sourceHeight * 4);
  const frames = [];
  let previousFrame = null;
  let restoreCanvas = null;
  let timeline = 0;
  let sample = 0;
  for (let index = 0; index < frameTotal; index++) {
    if (previousFrame?.disposal === 2) {
      clearGifRectangle(canvas, sourceWidth, previousFrame);
    } else if (previousFrame?.disposal === 3 && restoreCanvas) {
      canvas.set(restoreCanvas);
    }
    const frame = reader.frameInfo(index);
    restoreCanvas = frame.disposal === 3
      ? Uint8Array.from(canvas)
      : null;
    reader.decodeAndBlitFrameRGBA(index, canvas);
    const frameEnd = timeline + durations[index];
    while (sample < schedule.times.length &&
           schedule.times[sample] < frameEnd) {
      frames.push(resizeRgba(
        canvas,
        sourceWidth,
        sourceHeight,
        size.width,
        size.height,
        options.fit,
      ));
      sample++;
    }
    timeline = frameEnd;
    previousFrame = frame;
  }
  while (frames.length < schedule.frameCount) {
    frames.push(Uint8Array.from(frames.at(-1)));
  }
  const background = parseBackground(options.background);
  return {
    width: size.width,
    height: size.height,
    frames: frames.length,
    frameDuration: schedule.frameDuration,
    data: Buffer.concat(
      frames.map((frame) => rgbaToRgb565(frame, background)),
    ),
  };
}

let dotLottieModulePromise;

async function dotLottieModule() {
  if (!dotLottieModulePromise) {
    dotLottieModulePromise = (async () => {
      const module = await import("@lottiefiles/dotlottie-web");
      const entry = new URL(
        import.meta.resolve("@lottiefiles/dotlottie-web"),
      );
      const wasmPath = fileURLToPath(
        new URL("./dotlottie-player.wasm", entry),
      );
      const wasm = await readFile(wasmPath);
      module.DotLottie.setWasmUrl(
        `data:application/wasm;base64,${wasm.toString("base64")}`,
      );
      await module.DotLottie.preload();
      return module;
    })();
  }
  return dotLottieModulePromise;
}

async function waitForLottie(player) {
  if (player.isLoaded) return;
  await new Promise((resolve, reject) => {
    const timeout = setTimeout(() => {
      cleanup();
      reject(new Error("Lottie renderer timed out"));
    }, LOAD_TIMEOUT_MS);
    const loaded = () => {
      cleanup();
      resolve();
    };
    const failed = (event) => {
      cleanup();
      reject(new Error(
        `cannot load Lottie animation: ${event?.error ?? "invalid data"}`,
      ));
    };
    const cleanup = () => {
      clearTimeout(timeout);
      player.removeEventListener("load", loaded);
      player.removeEventListener("loadError", failed);
    };
    player.addEventListener("load", loaded);
    player.addEventListener("loadError", failed);
  });
}

export async function convertLottieBuffer(data, options = {}) {
  let source;
  try {
    source = JSON.parse(data.toString("utf8"));
  } catch {
    throw new Error("Lottie source must be UTF-8 JSON");
  }
  const sourceWidth = integer(
    source.w, undefined, 1, 4096, "Lottie width",
  );
  const sourceHeight = integer(
    source.h, undefined, 1, 4096, "Lottie height",
  );
  const size = outputSize(options, sourceWidth, sourceHeight);
  const { DotLottie } = await dotLottieModule();
  const player = new DotLottie({
    canvas: { width: size.width, height: size.height },
    data: source,
    autoplay: false,
    loop: false,
    layout: {
      fit: options.fit ?? "contain",
      align: [0.5, 0.5],
    },
    backgroundColor: "transparent",
    renderConfig: {
      autoResize: false,
      devicePixelRatio: 1,
      freezeOnOffscreen: false,
      quality: 100,
    },
  });
  try {
    await waitForLottie(player);
    const durationMs = Math.max(10, Math.ceil(player.duration * 1000));
    const schedule = frameSchedule(durationMs, options);
    if (size.height * schedule.frameCount > SHEET_HEIGHT_MAX) {
      throw new Error(
        "animation frames exceed the 4096-pixel sprite-sheet height",
      );
    }
    const background = parseBackground(options.background);
    const frames = [];
    for (let index = 0; index < schedule.frameCount; index++) {
      const sourceFrame = Math.min(
        Math.max(0, player.totalFrames - 1),
        Math.floor(
          index * Math.max(1, player.totalFrames) /
          schedule.frameCount,
        ),
      );
      player.setFrame(sourceFrame);
      const buffer = player.buffer;
      if (!buffer ||
          buffer.length !== size.width * size.height * 4) {
        throw new Error("Lottie renderer returned an invalid frame");
      }
      frames.push(rgbaToRgb565(buffer, background));
    }
    return {
      width: size.width,
      height: size.height,
      frames: frames.length,
      frameDuration: schedule.frameDuration,
      data: Buffer.concat(frames),
    };
  } finally {
    player.destroy();
  }
}

export async function convertAnimation(file, data, options = {}) {
  const extension = file.toLowerCase().split(".").at(-1);
  if (extension === "gif") {
    return convertGifBuffer(data, options);
  }
  if (extension === "json") {
    return convertLottieBuffer(data, options);
  }
  throw new Error("animation source must be .gif or Lottie .json");
}
