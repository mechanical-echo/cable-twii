import sharp from "sharp";
import { atomicWriteContained } from "./contain.js";

const ICON_SIZE = 128;

export class NotImageError extends Error {
  constructor() {
    super("That file is not an image. Use a PNG, JPEG, GIF, WebP or SVG file.");
    this.name = "NotImageError";
  }
}

export async function makeIcon(root: string, input: Buffer, dst: string): Promise<void> {
  let png: Buffer;
  try {
    const meta = await sharp(input).metadata();
    if (!meta.format || !meta.width || !meta.height) throw new NotImageError();
    png = await sharp(input)
      .resize(ICON_SIZE, ICON_SIZE, { fit: "contain", background: { r: 0, g: 0, b: 0, alpha: 0 } })
      .png()
      .toBuffer();
  } catch {
    throw new NotImageError();
  }
  await atomicWriteContained(root, dst, png);
}
