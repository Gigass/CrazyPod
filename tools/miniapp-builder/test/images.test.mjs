import assert from "node:assert/strict";
import test from "node:test";

import { PNG } from "pngjs";
import { convertPngBuffer } from "../src/images.mjs";

test("PNG assets are resized, alpha-composited and converted to RGB565", () => {
  const png = new PNG({ width: 2, height: 1 });
  png.data.set([
    255, 0, 0, 255,
    0, 255, 0, 128,
  ]);
  const encoded = PNG.sync.write(png);
  const result = convertPngBuffer(encoded, {
    width: 4,
    height: 1,
    background: "#000000",
  });

  assert.equal(result.width, 4);
  assert.equal(result.height, 1);
  assert.equal(result.data.readUInt16LE(0), 0xf800);
  assert.equal(result.data.readUInt16LE(2), 0xf800);
  assert.equal(result.data.readUInt16LE(4), 0x0400);
  assert.equal(result.data.readUInt16LE(6), 0x0400);
});
