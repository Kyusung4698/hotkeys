#include <nan.h>
#include "windows.h"
#include "winbase.h"
#include "winuser.h"
#include <string>
#include <sstream>
#include <unordered_map>

using v8::Context;
using v8::Local;
using v8::Number;
using v8::Object;
using v8::Value;
using v8::Function;
using v8::FunctionTemplate;

HHOOK g_hKeyboardHook;
Nan::Callback *g_fKeyboardCallback;

std::unordered_map<size_t, bool> g_dModifiers;

void _buildAndSendCallback(std::string key) {
  std::stringstream ss;
  ss << "{ "
        << "\"Modifiers\": {"
          << "\"Control\": " << (g_dModifiers[VK_CONTROL] ? "true" : "false") << ", "
          << "\"Control\": " << (g_dModifiers[VK_SHIFT] ? "true" : "false") << ", "
          << "\"Control\": " << (g_dModifiers[VK_MENU] ? "true" : "false")
        << "}, "
        << "\"Key\": \"" << key << "\""
      << "}";

  const unsigned argc = 1;
  Local<Value> argv[argc] = {Nan::New(ss.str()).ToLocalChecked()};
  g_fKeyboardCallback->Call(argc, argv);
}

std::string _getKeyName(unsigned int virtualKey) {
    unsigned int scanCode = MapVirtualKey(virtualKey, MAPVK_VK_TO_VSC);

    // because MapVirtualKey strips the extended bit for some keys
    switch (virtualKey) {
        case VK_LEFT: case VK_UP: case VK_RIGHT: case VK_DOWN: // arrow keys
        case VK_PRIOR: case VK_NEXT: // page up and page down
        case VK_END: case VK_HOME:
        case VK_INSERT: case VK_DELETE:
        case VK_DIVIDE: // numpad slash
        case VK_NUMLOCK: {
            scanCode |= 0x100; // set extended bit
            break;
        }
    }

    char keyName[50];
    if (GetKeyNameText(scanCode << 16, keyName, sizeof(keyName)) != 0) {
        return keyName;
    }
    return "";
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
  if (nCode < 0 || nCode != HC_ACTION) { // do not process message
    return CallNextHookEx(g_hKeyboardHook, nCode, wParam, lParam);
  }

  KBDLLHOOKSTRUCT *p = (KBDLLHOOKSTRUCT *)lParam;
  switch (wParam) {
    case WM_KEYDOWN: {
      switch (p->vkCode) {
        case VK_LCONTROL:
        case VK_RCONTROL:
          g_dModifiers[VK_CONTROL] = true;
          break;
        case VK_LSHIFT:
        case VK_RSHIFT:
          g_dModifiers[VK_SHIFT] = true;
          break;
        case VK_LMENU:
        case VK_RMENU:
          g_dModifiers[VK_MENU] = true;
          break;
        default:
          std::string key = _getKeyName(p->vkCode);
          if (key != "") {
            _buildAndSendCallback(key);
          }
      }
      break;
    }
    case WM_KEYUP: {
      switch (p->vkCode) {
        case VK_LCONTROL:
        case VK_RCONTROL:
          g_dModifiers[VK_CONTROL] = false;
          break;
        case VK_LSHIFT:
        case VK_RSHIFT:
          g_dModifiers[VK_SHIFT] = false;
          break;
        case VK_LMENU:
        case VK_RMENU:
          g_dModifiers[VK_MENU] = false;
          break;
      }
      break;
    }
  }

  return CallNextHookEx(g_hKeyboardHook, nCode, wParam, lParam);
}

void _addHook(const Nan::FunctionCallbackInfo<Value> &info) {
  g_fKeyboardCallback = new Nan::Callback(info[0].As<Function>());

  g_dModifiers[VK_CONTROL] = false;
  g_dModifiers[VK_SHIFT] = false;
  g_dModifiers[VK_MENU] = false;

  g_hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(NULL), 0);
  info.GetReturnValue().Set(Nan::New("true").ToLocalChecked());
}

void _clearHook(const Nan::FunctionCallbackInfo<Value> &info) {
  UnhookWindowsHookEx(g_hKeyboardHook);
  info.GetReturnValue().Set(Nan::New("true").ToLocalChecked());
}

void Init(Local<Object> exports) {
  Local<Context> context = exports->CreationContext();
  exports->Set(context,
               Nan::New("addHook").ToLocalChecked(),
               Nan::New<FunctionTemplate>(_addHook)
                   ->GetFunction(context)
                   .ToLocalChecked());
  exports->Set(context,
               Nan::New("clearHook").ToLocalChecked(),
               Nan::New<FunctionTemplate>(_clearHook)
                   ->GetFunction(context)
                   .ToLocalChecked());
}

NODE_MODULE(hotkeys, Init)