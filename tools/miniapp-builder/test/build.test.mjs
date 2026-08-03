import assert from "node:assert/strict";
import {
  mkdtemp,
  readFile,
  rm,
  writeFile,
} from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import test from "node:test";

import { buildProject } from "../src/build.mjs";

const repository = path.resolve(import.meta.dirname, "../../..");

test("Native 2048 builds as a deterministic CPK5", async () => {
  const directory = await mkdtemp(
    path.join(os.tmpdir(), "crazypod-builder-"),
  );
  try {
    const output = path.join(directory, "game2048.cpk");
    const binary = path.join(directory, "app.dylib");
    await writeFile(binary, Buffer.from("native-game2048"));
    const first = await buildProject(
      path.join(repository, "miniapps/apps/game2048"), {
        output,
        binary,
        target: "simulator",
      });
    const firstBytes = await readFile(output);
    const second = await buildProject(
      path.join(repository, "miniapps/apps/game2048"), {
        output,
        binary,
        target: "simulator",
      });
    const secondBytes = await readFile(output);
    assert.deepEqual(firstBytes, secondBytes);
    assert.equal(first.manifest.format, 5);
    assert.equal(first.manifest.runtime, "native-aot");
    assert.equal(first.manifest.entry, "app.dylib");
    assert.equal(first.sizes.binary, 15);
    assert.deepEqual(first.sizes, second.sizes);
  } finally {
    await rm(directory, { recursive: true, force: true });
  }
});

test("Capability Lab packages a deterministic CPK5 native payload", async () => {
  const directory = await mkdtemp(
    path.join(os.tmpdir(), "crazypod-capability-builder-"),
  );
  try {
    const output = path.join(directory, "capability-lab.cpk");
    const binary = path.join(directory, "app.dylib");
    await writeFile(binary, Buffer.from("native-capability-lab"));
    const result = await buildProject(
      path.join(repository, "miniapps/apps/capability-lab"), {
        output,
        binary,
        target: "simulator",
      });
    const bytes = await readFile(output);
    assert.equal(result.manifest.id, "capability-lab");
    assert.equal(result.manifest.format, 5);
    assert.equal(result.manifest.runtime, "native-aot");
    assert.equal(result.manifest.entry, "app.dylib");
    assert.equal(result.sizes.binary, 21);
    assert.equal(result.sizes.profile, 16);
    assert.ok(result.sizes.assets > 50_000);
    assert.equal(bytes.readUInt32LE(0), 0x04034b50);
  } finally {
    await rm(directory, { recursive: true, force: true });
  }
});

test("Now Playing theme packages its source artwork size", async () => {
  const directory = await mkdtemp(
    path.join(os.tmpdir(), "crazypod-theme-builder-"),
  );
  try {
    const output = path.join(directory, "atelier-hifi.cpk");
    const binary = path.join(directory, "app.dylib");
    await writeFile(binary, Buffer.from("native-theme"));
    const result = await buildProject(
      path.join(repository, "miniapps/themes/atelier-hifi"), {
        output,
        binary,
        target: "simulator",
      });
    assert.equal(result.manifest.kind, "now-playing-theme");
    assert.equal(result.manifest.artworkSourceSize, 104);
  } finally {
    await rm(directory, { recursive: true, force: true });
  }
});
