export interface Latest {
  begin(): () => boolean;
  cancel(): void;
}

export function createLatest(): Latest {
  let current = 0;
  return {
    begin() {
      const id = ++current;
      return () => id === current;
    },
    cancel() {
      current += 1;
    }
  };
}
