var hotkeys = require("./build/Release/hotkeys.node");

// module.exports = hotkeys;
var shortcuts = [];

export function beginListener() {
    hotkeys.addHook(inputEventHandler);
}

function inputEventHandler(keyInfo) {
    let found = false;
    const input = JSON.parse(keyInfo);
    shortcuts.some((item, index) => {
        if (!found && this.isEquivalent(item.check, input)) {
            item.callback();
            return true;
        }
        return false;
    });
}

export function removeListener() {
    hotkeys.clearHook();
}

export function register(shortcut, callback) {
    const formattedShortcut = {
        check: {
            Key: '',
            Modifiers: {
                Control: false,
                Shift: false,
                Alt: false
            },
        },
        callback: callback
    };
    const checkShortcut = shortcut.replace(/\s/g, '').toUpperCase().split('+');

    checkShortcut.forEach((item, index) => {
        switch (item) {
            case 'CTRL':
            case 'CMDORCTRL':
                formattedShortcut.check.Modifiers.Control = true;
                break;
            case 'SHIFT':
                formattedShortcut.check.Modifiers.Shift = true;
                break;
            case 'ALT':
                formattedShortcut.check.Modifiers.Alt = true;
                break;
            default:
                formattedShortcut.check.Key = item;
                break;
        }
    });
    this.shortcuts.push(formattedShortcut);
}

isEquivalent(a, b) {
    const aProps = Object.getOwnPropertyNames(a);
    const bProps = Object.getOwnPropertyNames(b);

    if (aProps.length !== bProps.length) {
        return false;
    }

    for (let i = 0; i < aProps.length; i++) {
        const propName = aProps[i];

        if (typeof a[propName] === 'object') {
            if (!this.isEquivalent(a[propName], b[propName])) {
              return false;
            }
        } else {
            if (a[propName] !== b[propName]) {
                return false;
            }
        }
    }
    return true;
}