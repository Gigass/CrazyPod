import {
  access,
  mkdir,
  mkdtemp,
  readFile,
  readdir,
  rm,
  writeFile,
} from "node:fs/promises";
import { readFileSync } from "node:fs";
import { execFile } from "node:child_process";
import { promisify } from "node:util";
import os from "node:os";
import path from "node:path";
import { convertAnimation } from "./animations.mjs";
import { convertPngBuffer } from "./images.mjs";

const sdkHeader = readFileSync(new URL(
  "../../../miniapps/sdk/crazypod_miniapp_native.h", import.meta.url,
), "utf8");

function sdkConstant(name) {
  const match = sdkHeader.match(new RegExp(`^#define ${name} (\\d+)u$`, "m"));
  if (!match) throw new Error(`SDK header is missing ${name}`);
  return Number(match[1]);
}

export const NATIVE_ABI_MAJOR = sdkConstant("CP_NATIVE_ABI_MAJOR");
export const NATIVE_ABI_MINOR = sdkConstant("CP_NATIVE_ABI_MINOR");
export const REACT_PROFILE = sdkConstant("CP_NATIVE_REACT_PROFILE");

const execFileAsync = promisify(execFile);
const FONT_MAX = 4 * 1024 * 1024;
const FONT_RESOURCE_MAX = 4;
const FONT_HEADER_SIZE = 36;
const FONT_LONG_OFFSET_THRESHOLD = 0xffdb;

function validateRockboxFont(data, id) {
  if (data.length < FONT_HEADER_SIZE ||
      data.subarray(0, 4).toString("ascii") !== "RB12") {
    throw new Error(`${id} did not produce a Rockbox RB12 font`);
  }
  const maxWidth = data.readUInt16LE(4);
  const height = data.readUInt16LE(6);
  const ascent = data.readUInt16LE(8);
  const depth = data.readUInt16LE(10);
  const first = data.readUInt32LE(12);
  const defaultCharacter = data.readUInt32LE(16);
  const size = data.readUInt32LE(20);
  const bitsSize = data.readUInt32LE(24);
  const offsets = data.readUInt32LE(28);
  const widths = data.readUInt32LE(32);
  if (data.length > FONT_MAX || maxWidth < 1 || maxWidth > 128 ||
      height < 1 || height > 64 || ascent > height || depth > 1 ||
      size < 1 || first > 0xffff || first + size > 0x10000 ||
      defaultCharacter < first || defaultCharacter >= first + size ||
      bitsSize < 1 || (offsets !== 0 && offsets !== size) ||
      (widths !== 0 && widths !== size)) {
    throw new Error(`${id} produced an unsupported Rockbox font`);
  }
  const alignment = bitsSize < FONT_LONG_OFFSET_THRESHOLD ? 2 : 4;
  const offsetBytes = bitsSize < FONT_LONG_OFFSET_THRESHOLD ? 2 : 4;
  const tableStart = (FONT_HEADER_SIZE + bitsSize + alignment - 1) &
    ~(alignment - 1);
  const expectedSize = tableStart + offsets * offsetBytes + widths;
  if (expectedSize !== data.length) {
    throw new Error(`${id} produced an invalid Rockbox font layout`);
  }
}

async function ensureTool(tool, makeTarget) {
  try {
    await access(tool);
  } catch {
    const toolsDirectory = path.dirname(tool);
    await execFileAsync("make", ["-C", toolsDirectory, makeTarget], {
      maxBuffer: 1024 * 1024,
    });
  }
}

async function convertFont(source, item) {
  const extension = path.extname(source).toLowerCase();
  const repository = path.resolve(import.meta.dirname, "../../..");
  const toolsDirectory = path.join(repository, "tools");
  const temporary = await mkdtemp(path.join(os.tmpdir(), "crazypod-font-"));
  try {
    let output;
    if ([".ttf", ".otf", ".ttc"].includes(extension)) {
      if (!Number.isInteger(item.size) || item.size < 6 || item.size > 48) {
        throw new Error(`${item.id} outline font requires size from 6 to 48`);
      }
      const converter = process.env.CRAZYPOD_CONVTTF ??
        path.join(toolsDirectory, "convttf");
      await ensureTool(converter, "convttf");
      output = path.join(temporary, `${item.id}.fnt`);
      const args = [
        "-p", String(item.size), "-s", "32", "-l", "65535",
        "-o", output,
      ];
      if (extension === ".ttc") {
        const face = item.face ?? 0;
        if (!Number.isInteger(face) || face < 0 || face > 255) {
          throw new Error(`${item.id} TTC face must be from 0 to 255`);
        }
        args.push("-t", String(face));
      }
      args.push(source);
      await execFileAsync(converter, args, { maxBuffer: 4 * 1024 * 1024 });
    } else if (extension === ".bdf") {
      if (item.size !== undefined || item.face !== undefined) {
        throw new Error(`${item.id} BDF font uses its embedded pixel size`);
      }
      const converter = process.env.CRAZYPOD_CONVBDF ??
        path.join(toolsDirectory, "convbdf");
      await ensureTool(converter, "convbdf");
      await execFileAsync(converter, ["-l", "65535", "-f", source], {
        cwd: temporary,
        maxBuffer: 4 * 1024 * 1024,
      });
      const generated = (await readdir(temporary))
        .filter((name) => name.endsWith(".fnt"));
      if (generated.length !== 1) {
        throw new Error(`${item.id} BDF conversion produced no unique font`);
      }
      output = path.join(temporary, generated[0]);
    } else {
      throw new Error(
        `${item.id} font source must be TTF, OTF, TTC, or BDF`,
      );
    }
    const data = await readFile(output);
    validateRockboxFont(data, item.id);
    return data;
  } finally {
    await rm(temporary, { recursive: true, force: true });
  }
}

const crcTable = new Uint32Array(256);
for (let value = 0; value < 256; value++) {
  let crc = value;
  for (let bit = 0; bit < 8; bit++) {
    crc = (crc & 1) ? (crc >>> 1) ^ 0xedb88320 : crc >>> 1;
  }
  crcTable[value] = crc >>> 0;
}

export function crc32(buffer) {
  let crc = 0xffffffff;
  for (const value of buffer) {
    crc = (crc >>> 8) ^ crcTable[(crc ^ value) & 0xff];
  }
  return (crc ^ 0xffffffff) >>> 0;
}

export function emptyAssets() {
  const header = Buffer.alloc(16);
  header.writeUInt32LE(0x53525043, 0);
  header.writeUInt16LE(1, 4);
  header.writeUInt16LE(0, 6);
  header.writeUInt32LE(16, 8);
  header.writeUInt32LE(0, 12);
  return header;
}

export async function buildAssets(directory) {
  if (!directory) return emptyAssets();
  const descriptorPath = path.join(directory, "assets.json");
  let descriptor;
  try {
    descriptor = JSON.parse(await readFile(descriptorPath, "utf8"));
  } catch (error) {
    if (error.code === "ENOENT") return emptyAssets();
    throw error;
  }
  if (!Array.isArray(descriptor)) {
    throw new Error("assets/assets.json must be an array");
  }
  const types = {
    blob: 0,
    rgb565: 1,
    font: 2,
    sprite: 3,
    tileset: 4,
    pcm: 5,
  };
  const sourceTypes = new Set([
    ...Object.keys(types), "animation", "tone",
  ]);
  const items = [];
  let fontCount = 0;
  for (const item of descriptor) {
    if (!/^[a-z][a-z0-9_.-]{0,30}$/.test(item.id) ||
        !sourceTypes.has(item.type) ||
        (item.type !== "tone" && typeof item.file !== "string")) {
      throw new Error(`invalid asset descriptor ${JSON.stringify(item)}`);
    }
    let data = item.type === "tone"
      ? null
      : await readFile(path.join(directory, item.file));
    if (item.encoding === "base64") {
      data = Buffer.from(
        String(data).replace(/\s+/g, ""), "base64");
    } else if (item.encoding !== undefined) {
      throw new Error(`unsupported encoding for asset ${item.id}`);
    }
    let normalizedType = item.type;
    let converted = null;
    if (item.type === "animation") {
      converted = await convertAnimation(item.file, data, item);
      normalizedType = "sprite";
      data = converted.data;
    } else if (item.type === "font") {
      fontCount += 1;
      if (fontCount > FONT_RESOURCE_MAX) {
        throw new Error(`a package may declare at most ${FONT_RESOURCE_MAX} fonts`);
      }
      data = await convertFont(path.join(directory, item.file), item);
    } else if (
      (item.type === "rgb565" || item.type === "tileset") &&
      (item.format === "png" ||
       item.file.toLowerCase().endsWith(".png"))
    ) {
      converted = convertPngBuffer(data, item);
      data = converted.data;
    } else if (item.type === "tone") {
      const sampleRate = Number(item.sampleRate ?? 44100);
      const frequency = Number(item.frequency ?? 440);
      const duration = Number(item.duration ?? 250);
      const volume = Number(item.volume ?? 0.25);
      if (![44100, 22050, 11025, 8000].includes(sampleRate) ||
          !Number.isFinite(frequency) ||
          frequency < 20 || frequency > sampleRate / 2 ||
          !Number.isFinite(duration) ||
          duration < 10 || duration > 5000 ||
          !Number.isFinite(volume) || volume < 0 || volume > 1) {
        throw new Error(`invalid generated tone ${item.id}`);
      }
      const samples = Math.round(sampleRate * duration / 1000);
      data = Buffer.alloc(samples * 4);
      for (let index = 0; index < samples; index++) {
        const envelope = Math.min(
          1,
          index / Math.max(1, sampleRate * 0.005),
          (samples - index) / Math.max(1, sampleRate * 0.02),
        );
        const sample = Math.round(
          Math.sin(2 * Math.PI * frequency * index / sampleRate) *
          32767 * volume * envelope);
        data.writeInt16LE(sample, index * 4);
        data.writeInt16LE(sample, index * 4 + 2);
      }
      normalizedType = "pcm";
      converted = {
        width: sampleRate,
        height: 2,
      };
    }
    const width = converted?.width ?? (
      item.type === "pcm"
        ? Number(item.sampleRate ?? 0)
        : Number(item.width ?? 0)
    );
    const height = converted?.frames
      ? converted.height * converted.frames
      : (converted?.height ?? (item.type === "pcm"
        ? Number(item.channels ?? 0)
        : Number(item.height ?? 0)));
    const frameCount = converted?.frames ?? (
      normalizedType === "sprite" ? Number(item.frames ?? 1) : 0
    );
    const frameDuration = converted?.frameDuration ?? (
      normalizedType === "sprite"
        ? Number(item.frameDuration ?? 100)
        : 0
    );
    if (!Number.isInteger(width) || !Number.isInteger(height) ||
        width < 0 || width > 65535 || height < 0 || height > 65535) {
      throw new Error(`invalid dimensions for asset ${item.id}`);
    }
    if (!Number.isInteger(frameCount) || frameCount < 0 ||
        frameCount > 32 || !Number.isInteger(frameDuration) ||
        frameDuration < 0 || frameDuration > 60000) {
      throw new Error(`invalid animation metadata for asset ${item.id}`);
    }
    if (["rgb565", "sprite", "tileset"].includes(normalizedType) &&
        (width === 0 || height === 0 ||
         data.length !== width * height * 2)) {
      throw new Error(`${item.id} must contain width*height RGB565 pixels`);
    }
    if (normalizedType === "rgb565" &&
        (width > 320 || height > 240)) {
      throw new Error(`${item.id} bitmap exceeds the 320x240 display`);
    }
    if (normalizedType === "tileset" &&
        (width > 320 || height > 320)) {
      throw new Error(`${item.id} tileset exceeds 320x320`);
    }
    if (normalizedType === "sprite" &&
        (width > 320 || height > 4096 ||
         frameCount < 1 || height % frameCount !== 0 ||
         height / frameCount > 240 ||
         frameDuration < 10)) {
      throw new Error(
        `${item.id} must be a vertical 1-32 frame RGB565 sprite sheet`,
      );
    }
    if (normalizedType === "pcm" &&
        (width < 8000 || width > 48000 || height !== 2 ||
         data.length === 0 || data.length > 512 * 1024 ||
         data.length % 4 !== 0)) {
      throw new Error(
        `${item.id} must be interleaved stereo signed 16-bit PCM`,
      );
    }
    items.push({
      ...item,
      type: normalizedType,
      typeId: types[normalizedType],
      width,
      height,
      frameCount,
      frameDuration,
      data,
    });
  }
  items.sort((left, right) => left.id.localeCompare(right.id));
  if (items.length > 256 ||
      new Set(items.map((item) => item.id)).size !== items.length) {
    throw new Error("assets must have at most 256 unique ids");
  }
  const headerSize = 16;
  const entrySize = 52;
  let offset = headerSize + items.length * entrySize;
  const entries = [];
  const payloads = [];
  for (const item of items) {
    const entry = Buffer.alloc(entrySize);
    entry.write(item.id, 0, 31, "utf8");
    entry.writeUInt8(item.typeId, 32);
    entry.writeUInt8(item.frameCount, 33);
    entry.writeUInt16LE(item.width, 34);
    entry.writeUInt16LE(item.height, 36);
    entry.writeUInt16LE(item.frameDuration, 38);
    entry.writeUInt32LE(offset, 40);
    entry.writeUInt32LE(item.data.length, 44);
    entry.writeUInt32LE(crc32(item.data), 48);
    entries.push(entry);
    payloads.push(item.data);
    offset += item.data.length;
  }
  const header = Buffer.alloc(headerSize);
  header.writeUInt32LE(0x53525043, 0);
  header.writeUInt16LE(1, 4);
  header.writeUInt16LE(items.length, 6);
  header.writeUInt32LE(offset, 8);
  header.writeUInt32LE(crc32(Buffer.concat(entries)), 12);
  return Buffer.concat([header, ...entries, ...payloads]);
}

function rgb565(color) {
  if (!/^#[0-9a-f]{6}$/i.test(color)) {
    throw new Error(`invalid iconColor ${color}`);
  }
  const value = Number.parseInt(color.slice(1), 16);
  const red = (value >> 16) & 0xff;
  const green = (value >> 8) & 0xff;
  const blue = value & 0xff;
  return ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3);
}

export function solidIcon(color) {
  const pixels = Buffer.alloc(160 * 160 * 2);
  const value = rgb565(color);

  for (let offset = 0; offset < pixels.length; offset += 2) {
    pixels.writeUInt16LE(value, offset);
  }
  return rgb565Icon(pixels);
}

export function rgb565Icon(pixels) {
  if (!Buffer.isBuffer(pixels) ||
      pixels.length !== 160 * 160 * 2) {
    throw new TypeError("icon pixels must be 160x160 RGB565");
  }
  const header = Buffer.alloc(16);

  header.writeUInt32LE(0x35495043, 0); // CPI5
  header.writeUInt16LE(1, 4);
  header.writeUInt16LE(160, 6);
  header.writeUInt16LE(160, 8);
  header.writeUInt16LE(1, 10);
  header.writeUInt32LE(pixels.length, 12);
  return Buffer.concat([header, pixels]);
}

export function deterministicZip(entries) {
  const local = [];
  const central = [];
  let offset = 0;
  for (const [name, data] of entries) {
    const filename = Buffer.from(name, "utf8");
    const crc = crc32(data);
    const localHeader = Buffer.alloc(30);
    localHeader.writeUInt32LE(0x04034b50, 0);
    localHeader.writeUInt16LE(20, 4);
    localHeader.writeUInt16LE(0, 6);
    localHeader.writeUInt16LE(0, 8);
    localHeader.writeUInt16LE(0, 10);
    localHeader.writeUInt16LE(33, 12);
    localHeader.writeUInt32LE(crc, 14);
    localHeader.writeUInt32LE(data.length, 18);
    localHeader.writeUInt32LE(data.length, 22);
    localHeader.writeUInt16LE(filename.length, 26);
    localHeader.writeUInt16LE(0, 28);
    local.push(localHeader, filename, data);

    const centralHeader = Buffer.alloc(46);
    centralHeader.writeUInt32LE(0x02014b50, 0);
    centralHeader.writeUInt16LE(20, 4);
    centralHeader.writeUInt16LE(20, 6);
    centralHeader.writeUInt16LE(0, 8);
    centralHeader.writeUInt16LE(0, 10);
    centralHeader.writeUInt16LE(0, 12);
    centralHeader.writeUInt16LE(33, 14);
    centralHeader.writeUInt32LE(crc, 16);
    centralHeader.writeUInt32LE(data.length, 20);
    centralHeader.writeUInt32LE(data.length, 24);
    centralHeader.writeUInt16LE(filename.length, 28);
    centralHeader.writeUInt16LE(0, 30);
    centralHeader.writeUInt16LE(0, 32);
    centralHeader.writeUInt16LE(0, 34);
    centralHeader.writeUInt16LE(0, 36);
    centralHeader.writeUInt32LE(0, 38);
    centralHeader.writeUInt32LE(offset, 42);
    central.push(centralHeader, filename);
    offset += localHeader.length + filename.length + data.length;
  }
  const centralData = Buffer.concat(central);
  const end = Buffer.alloc(22);
  end.writeUInt32LE(0x06054b50, 0);
  end.writeUInt16LE(0, 4);
  end.writeUInt16LE(0, 6);
  end.writeUInt16LE(entries.length, 8);
  end.writeUInt16LE(entries.length, 10);
  end.writeUInt32LE(centralData.length, 12);
  end.writeUInt32LE(offset, 16);
  end.writeUInt16LE(0, 20);
  return Buffer.concat([...local, centralData, end]);
}

export function nativeProfile() {
  const profile = Buffer.alloc(16);
  profile.writeUInt32LE(0x35415043, 0); // CPA5
  profile.writeUInt16LE(1, 4);
  profile.writeUInt16LE(profile.length, 6);
  profile.writeUInt16LE(NATIVE_ABI_MAJOR, 8);
  profile.writeUInt16LE(NATIVE_ABI_MINOR, 10);
  profile.writeUInt16LE(REACT_PROFILE, 12);
  profile.writeUInt16LE(0, 14);
  return profile;
}

export async function writeNativePackage(output, files) {
  if (!["app.arm", "app.dylib"].includes(files.binaryName)) {
    throw new Error(`invalid native binary name ${files.binaryName}`);
  }
  const entries = [
    ["manifest.json", files.manifest],
    [files.binaryName, files.binary],
    ["profile.bin", files.profile ?? nativeProfile()],
    ["assets.bin", files.assets],
    ["icon.bin", files.icon],
  ];
  await mkdir(path.dirname(output), { recursive: true });
  await writeFile(output, deterministicZip(entries));
}
