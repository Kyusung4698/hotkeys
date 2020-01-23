export function beginListener(checkWindow: boolean): void;
export function removeListener(): void;

export function on(callback: (active: boolean) => void): void;
export function register(shortcut: string, callback: () => void): void;
export function unregister(shortcut: string): void;
export function unregisterall(): void;