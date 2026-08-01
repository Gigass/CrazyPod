import { PNG } from "pngjs";

function parseColor(value = "#000000") {
  if (!/^#[0-9a-f]{6}$/i.test(value)) {
    throw new TypeError("image background must be #RRGGBB");
  }
  const color = Number.parseInt(value.slice(1), 16);
  return [
    (color >> 16) & 0xff,
    (color >> 8) & 0xff,
    color & 0xff,
  ];
}

function rgb565(red, green, blue) {
  return ((red >> 3) << 11) |
    ((green >> 2) << 5) |
    (blue >> 3);
}

export function convertPngBuffer(data, options = {}) {
  const image = PNG.sync.read(data, {
    checkCRC: true,
    skipRescale: false,
  });
  const width = Number(options.width ?? image.width);
  const height = Number(options.height ?? image.height);
  if (!Number.isInteger(width) || !Number.isInteger(height) ||
      width < 1 || width > 320 ||
      height < 1 || height > 4096) {
    throw new RangeError(
      "converted PNG dimensions must be within 320x4096",
    );
  }
  const background = parseColor(options.background);
  const output = Buffer.alloc(width * height * 2);
  for (let y = 0; y < height; y++) {
    const sourceY = Math.min(
      image.height - 1, Math.floor(y * image.height / height));
    for (let x = 0; x < width; x++) {
      const sourceX = Math.min(
        image.width - 1, Math.floor(x * image.width / width));
      const source = (sourceY * image.width + sourceX) * 4;
      const alpha = image.data[source + 3];
      const inverse = 255 - alpha;
      const red = Math.round(
        (image.data[source] * alpha +
         background[0] * inverse) / 255);
      const green = Math.round(
        (image.data[source + 1] * alpha +
         background[1] * inverse) / 255);
      const blue = Math.round(
        (image.data[source + 2] * alpha +
         background[2] * inverse) / 255);
      output.writeUInt16LE(
        rgb565(red, green, blue),
        (y * width + x) * 2);
    }
  }
  return { data: output, width, height };
}
