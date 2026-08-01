import assert from "node:assert/strict";
import test from "node:test";

import { GifWriter } from "omggif";

import {
  convertGifBuffer,
  convertLottieBuffer,
} from "../src/animations.mjs";

test("animated GIF is resampled and converted to a vertical RGB565 sheet", () => {
  const storage = Buffer.alloc(256);
  const writer = new GifWriter(storage, 2, 1, {
    palette: [0xff0000, 0x00ff00],
    loop: 0,
  });
  writer.addFrame(
    0, 0, 2, 1, Uint8Array.from([0, 0]), { delay: 10 },
  );
  writer.addFrame(
    0, 0, 2, 1, Uint8Array.from([1, 1]), { delay: 10 },
  );
  const source = storage.subarray(0, writer.end());
  const result = convertGifBuffer(source, {
    fps: 10,
    background: "#000000",
  });

  assert.equal(result.width, 2);
  assert.equal(result.height, 1);
  assert.equal(result.frames, 2);
  assert.equal(result.frameDuration, 100);
  assert.equal(result.data.length, 8);
  assert.equal(result.data.readUInt16LE(0), 0xf800);
  assert.equal(result.data.readUInt16LE(4), 0x07e0);
});

test("Lottie JSON is rendered offscreen and converted to RGB565", async () => {
  const source = {
    v: "5.7.4",
    fr: 10,
    ip: 0,
    op: 2,
    w: 4,
    h: 4,
    nm: "test",
    ddd: 0,
    assets: [],
    layers: [{
      ddd: 0,
      ind: 1,
      ty: 4,
      nm: "shape",
      sr: 1,
      ks: {
        o: { a: 0, k: 100 },
        r: { a: 0, k: 0 },
        p: { a: 0, k: [2, 2, 0] },
        a: { a: 0, k: [0, 0, 0] },
        s: { a: 0, k: [100, 100, 100] },
      },
      ao: 0,
      shapes: [{
        ty: "rc",
        d: 1,
        s: { a: 0, k: [4, 4] },
        p: { a: 0, k: [0, 0] },
        r: { a: 0, k: 0 },
        nm: "Rectangle Path",
      }, {
        ty: "fl",
        c: { a: 0, k: [1, 0, 0, 1] },
        o: { a: 0, k: 100 },
        r: 1,
        nm: "Fill",
      }, {
        ty: "tr",
        p: { a: 0, k: [0, 0] },
        a: { a: 0, k: [0, 0] },
        s: { a: 0, k: [100, 100] },
        r: { a: 0, k: 0 },
        o: { a: 0, k: 100 },
        sk: { a: 0, k: 0 },
        sa: { a: 0, k: 0 },
        nm: "Transform",
      }],
      ip: 0,
      op: 2,
      st: 0,
      bm: 0,
    }],
  };
  const result = await convertLottieBuffer(
    Buffer.from(JSON.stringify(source)),
    { fps: 10, background: "#000000" },
  );

  assert.equal(result.width, 4);
  assert.equal(result.height, 4);
  assert.equal(result.frames, 2);
  assert.equal(result.frameDuration, 100);
  assert.equal(result.data.length, 64);
  assert(result.data.some((value) => value !== 0));
});

test("animation conversion rejects sprite sheets beyond device limits", () => {
  const storage = Buffer.alloc(256);
  const writer = new GifWriter(storage, 1, 240, {
    palette: [0x000000, 0xffffff],
  });
  writer.addFrame(
    0, 0, 1, 240, new Uint8Array(240), { delay: 1000 },
  );
  const source = storage.subarray(0, writer.end());

  assert.throws(
    () => convertGifBuffer(source, { fps: 30 }),
    /4096-pixel sprite-sheet height/,
  );
});
