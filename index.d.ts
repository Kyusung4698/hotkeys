export interface KeyInfo {
  Modifiers: ModifierInfo;
  Key: string;
}
export interface ModifierInfo {
  Control: boolean;
  Shift: boolean;
  Alt: boolean;
}
export function addHook(callback: (keyInfo: KeyInfo) => void) : void;
export function clearHook() : void;
