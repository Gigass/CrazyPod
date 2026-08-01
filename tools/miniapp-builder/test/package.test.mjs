import assert from "node:assert/strict";
import {
  mkdtemp,
  mkdir,
  readFile,
  rm,
  writeFile,
} from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import test from "node:test";

import {
  buildAssets,
  deterministicZip,
  emptyAssets,
  NATIVE_ABI_MINOR,
  rgb565Icon,
  nativeProfile,
  solidIcon,
  writeNativePackage,
} from "../src/package.mjs";

test("CPK5 ZIP STORE output is byte-for-byte deterministic", () => {
  const entries = [
    ["manifest.json", Buffer.from("{}")],
    ["app.dylib", Buffer.from("native")],
    ["profile.bin", nativeProfile()],
    ["assets.bin", emptyAssets()],
    ["icon.bin", solidIcon("#123456")],
  ];
  const first = deterministicZip(entries);
  const second = deterministicZip(entries);
  assert.deepEqual(first, second);
  assert.equal(first.readUInt32LE(0), 0x04034b50);
  assert.equal(first.readUInt16LE(8), 0);
  assert.equal(first.readUInt32LE(first.length - 22), 0x06054b50);
  assert.equal(first.readUInt16LE(first.length - 12), 5);
});

test("CPK5 carries a native binary and explicit ABI profile", async () => {
  const directory = await mkdtemp(
    path.join(os.tmpdir(), "crazypod-cpk5-"),
  );
  const output = path.join(directory, "native.cpk");
  await writeNativePackage(output, {
    manifest: Buffer.from('{"format":5}'),
    binaryName: "app.dylib",
    binary: Buffer.from("native"),
    profile: nativeProfile(),
    assets: emptyAssets(),
    icon: solidIcon("#ff9f43"),
  });
  const archive = await readFile(output);
  assert.equal(archive.readUInt32LE(0), 0x04034b50);
  assert.equal(archive.readUInt16LE(archive.length - 12), 5);
  assert.ok(archive.includes(Buffer.from("app.dylib")));
  assert.ok(archive.includes(Buffer.from("profile.bin")));
  assert.ok(archive.includes(Buffer.from([0x43, 0x50, 0x41, 0x35])));
  assert.equal(nativeProfile().readUInt16LE(10), NATIVE_ABI_MINOR);
});

test("icon and empty assets use native resource container headers", () => {
  const icon = solidIcon("#ffffff");
  const assets = emptyAssets();
  assert.equal(icon.length, 51216);
  assert.equal(icon.readUInt32LE(0), 0x35495043);
  assert.equal(icon.readUInt16LE(6), 160);
  assert.equal(assets.length, 16);
  assert.equal(assets.readUInt32LE(0), 0x53525043);
  const pixels = Buffer.alloc(160 * 160 * 2, 0xa5);
  const custom = rgb565Icon(pixels);
  assert.deepEqual(custom.subarray(16), pixels);
});

test("vertical sprite sheets preserve animation metadata", async () => {
  const directory = await mkdtemp(
    path.join(os.tmpdir(), "crazypod-assets-"),
  );
  try {
    await mkdir(path.join(directory, "assets"));
    await writeFile(
      path.join(directory, "assets/assets.json"),
      JSON.stringify([{
        id: "spinner",
        type: "sprite",
        file: "spinner.rgb565",
        width: 2,
        height: 4,
        frames: 2,
        frameDuration: 75,
      }]),
    );
    await writeFile(
      path.join(directory, "assets/spinner.rgb565"),
      Buffer.alloc(2 * 4 * 2),
    );
    const assets = await buildAssets(path.join(directory, "assets"));
    assert.equal(assets.readUInt8(16 + 33), 2);
    assert.equal(assets.readUInt16LE(16 + 38), 75);
    assert.equal(assets.readUInt16LE(16 + 34), 2);
    assert.equal(assets.readUInt16LE(16 + 36), 4);
  } finally {
    await rm(directory, { recursive: true, force: true });
  }
});

test("animation descriptors are converted before asset packing", async () => {
  const directory = await mkdtemp(
    path.join(os.tmpdir(), "crazypod-animation-assets-"),
  );
  try {
    const assetsDirectory = path.join(directory, "assets");
    await mkdir(assetsDirectory);
    const { GifWriter } = await import("omggif");
    const storage = Buffer.alloc(256);
    const writer = new GifWriter(storage, 2, 1, {
      palette: [0xff0000, 0x00ff00],
    });
    writer.addFrame(
      0, 0, 2, 1, Uint8Array.from([0, 1]), { delay: 10 },
    );
    const size = writer.end();
    await writeFile(
      path.join(assetsDirectory, "spinner.gif"),
      storage.subarray(0, size),
    );
    await writeFile(
      path.join(assetsDirectory, "assets.json"),
      JSON.stringify([{
        id: "spinner",
        type: "animation",
        file: "spinner.gif",
        fps: 10,
        background: "#000000",
      }]),
    );

    const assets = await buildAssets(assetsDirectory);
    assert.equal(assets.readUInt8(16 + 32), 3);
    assert.equal(assets.readUInt8(16 + 33), 1);
    assert.equal(assets.readUInt16LE(16 + 34), 2);
    assert.equal(assets.readUInt16LE(16 + 36), 1);
    assert.equal(assets.readUInt16LE(16 + 38), 100);
  } finally {
    await rm(directory, { recursive: true, force: true });
  }
});

test("PNG tilesets and generated tones need no target-side conversion", async () => {
  const directory = await mkdtemp(
    path.join(os.tmpdir(), "crazypod-generated-assets-"),
  );
  try {
    const assetsDirectory = path.join(directory, "assets");
    await mkdir(assetsDirectory);
    const { PNG } = await import("pngjs");
    const png = new PNG({ width: 8, height: 8 });
    for (let offset = 0; offset < png.data.length; offset += 4) {
      png.data.set([255, 0, 255, 255], offset);
    }
    await writeFile(
      path.join(assetsDirectory, "tiles.png"),
      PNG.sync.write(png),
    );
    await writeFile(
      path.join(assetsDirectory, "assets.json"),
      JSON.stringify([
        {
          id: "tiles",
          type: "tileset",
          file: "tiles.png",
        },
        {
          id: "beep",
          type: "tone",
          sampleRate: 8000,
          frequency: 440,
          duration: 20,
          volume: 0.25,
        },
      ]),
    );
    const assets = await buildAssets(assetsDirectory);
    assert.equal(assets.readUInt16LE(6), 2);
    const firstType = assets.readUInt8(16 + 32);
    const secondType = assets.readUInt8(16 + 52 + 32);
    assert.deepEqual([firstType, secondType].sort(), [4, 5]);
  } finally {
    await rm(directory, { recursive: true, force: true });
  }
});
