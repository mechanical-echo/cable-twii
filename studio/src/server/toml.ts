export function tomlString(value: string): string {
  let out = '"';
  for (let i = 0; i < value.length; i++) {
    const code = value.charCodeAt(i);
    if (code >= 0xd800 && code <= 0xdbff && i + 1 < value.length && value.charCodeAt(i + 1) >= 0xdc00 && value.charCodeAt(i + 1) <= 0xdfff) {
      out += value[i]! + value[i + 1]!;
      i++;
    } else if (code >= 0xd800 && code <= 0xdfff) {
      out += "\\uFFFD";
    } else if (code === 0x22) {
      out += '\\"';
    } else if (code === 0x5c) {
      out += "\\\\";
    } else if (code < 0x20 || code === 0x7f) {
      out += `\\u${code.toString(16).toUpperCase().padStart(4, "0")}`;
    } else {
      out += value[i]!;
    }
  }
  return `${out}"`;
}

export function tomlInt(value: number, label: string): string {
  if (!Number.isSafeInteger(value)) throw new Error(`${label} must be an integer, got ${value}`);
  return String(value);
}
