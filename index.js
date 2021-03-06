var hotkeys = require("./build/Release/hotkeys.node");

// module.exports = hotkeys;
var shortcuts = {};
var on;
var active = false;

module.exports.beginListener = (checkWindow) => {
    hotkeys.addHook(keyInfo => {
        inputEventHandler(keyInfo);
    }, activeChange => {
        activeChangeEventHandler(activeChange);
    }, checkWindow);
}

function inputEventHandler(keyInfo) {
    const input = JSON.parse(keyInfo);
    const hotkeys = Object.getOwnPropertyNames(shortcuts);

    hotkeys.some(hotkey => {
        const item = shortcuts[hotkey];
        if (isEquivalent(item.check, input)) {
            item.callback();
            return true;
        }
        return false;
    });
}

function activeChangeEventHandler(event) {
    const activeChange = JSON.parse(event);
    active = activeChange.Active;
    if (on) {
        on(active, activeChange.Path);
    }
}

module.exports.removeListener = () => {
    hotkeys.clearHook();
}

module.exports.on = (callback) => {
    on = callback;
    callback(active);
}

module.exports.register = (shortcut, callback) => {
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
    const checkShortcut = shortcut.replace(/\s/g, '').toLowerCase().split('+');

    checkShortcut.forEach((item, index) => {
        switch (item) {
            case 'ctrl':
            case 'cmdorctrl':
                formattedShortcut.check.Modifiers.Control = true;
                break;
            case 'shift':
                formattedShortcut.check.Modifiers.Shift = true;
                break;
            case 'alt':
                formattedShortcut.check.Modifiers.Alt = true;
                break;
            default:
                formattedShortcut.check.Key = item;
                break;
        }
    });
    shortcuts[shortcut] = formattedShortcut;
}

module.exports.unregister = (shortcut) => {
    delete shortcuts[shortcut];
}

module.exports.unregisterall = (shortcut) => {
    shortcuts = {};
}

function isEquivalent(a, b) {
    const aProps = Object.getOwnPropertyNames(a);
    const bProps = Object.getOwnPropertyNames(b);

    if (aProps.length !== bProps.length) {
        return false;
    }

    for (let i = 0; i < aProps.length; i++) {
        const propName = aProps[i];

        if (typeof a[propName] === 'object') {
            if (!isEquivalent(a[propName], b[propName])) {
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
