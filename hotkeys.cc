#include <nan.h>
#include "windows.h"
#include "winbase.h"
#include "winuser.h"
#include "psapi.h"
#include <string>
#include <sstream>
#include <iomanip>
#include <unordered_map>

using v8::Boolean;
using v8::Context;
using v8::Function;
using v8::FunctionTemplate;
using v8::Local;
using v8::Number;
using v8::Object;
using v8::Value;

HHOOK g_hKeyboardHook;
HHOOK g_hMouseHook;
Nan::Callback *g_fKeyboardCallback;
Nan::Callback *g_fWindowActiveCallback;

std::unordered_map<size_t, bool> g_dModifiers;
bool g_bCheckWindow;
bool g_bIsWindowActive;
std::string g_strWindowProcessPath = std::string("");
HWND g_hWnd;

std::string escape_json(const std::string &s)
{
	std::ostringstream o;
	for (auto c = s.cbegin(); c != s.cend(); c++)
	{
		switch (*c)
		{
		case '"':
			o << "\\\"";
			break;
		case '\\':
			o << "\\\\";
			break;
		case '\b':
			o << "\\b";
			break;
		case '\f':
			o << "\\f";
			break;
		case '\n':
			o << "\\n";
			break;
		case '\r':
			o << "\\r";
			break;
		case '\t':
			o << "\\t";
			break;
		default:
			if ('\x00' <= *c && *c <= '\x1f')
			{
				o << "\\u"
				  << std::hex << std::setw(4) << std::setfill('0') << (int)*c;
			}
			else
			{
				o << *c;
			}
		}
	}
	return o.str();
}

void _buildAndSendKeyboardCallback(std::string key)
{
	std::stringstream ss;
	ss << "{ "
	   << "\"Modifiers\": {"
	   << "\"Control\": " << (g_dModifiers[VK_CONTROL] ? "true" : "false") << ", "
	   << "\"Shift\": " << (g_dModifiers[VK_SHIFT] ? "true" : "false") << ", "
	   << "\"Alt\": " << (g_dModifiers[VK_MENU] ? "true" : "false")
	   << "}, "
	   << "\"Key\": \"" << key << "\""
	   << "}";

	const unsigned argc = 1;
	Local<Value> argv[argc] = {Nan::New(ss.str()).ToLocalChecked()};
	g_fKeyboardCallback->Call(argc, argv);
}

void _buildAndSendWindowActiveCallback()
{
	std::stringstream ss;
	ss << "{ "
	   << "\"Active\": " << (g_bIsWindowActive ? "true" : "false") << ", "
	   << "\"Path\": \"" << escape_json(g_strWindowProcessPath) << "\""
	   << "}";

	const unsigned argc = 1;
	Local<Value> argv[argc] = {Nan::New(ss.str()).ToLocalChecked()};
	g_fWindowActiveCallback->Call(argc, argv);
}

std::string _getKeyName(unsigned int virtualKey)
{
	switch (virtualKey)
	{
	case 48:
		return "0";
	case 49:
		return "1";
	case 50:
		return "2";
	case 51:
		return "3";
	case 52:
		return "4";
	case 53:
		return "5";
	case 54:
		return "6";
	case 55:
		return "7";
	case 56:
		return "8";
	case 57:
		return "9";
	case 65:
		return "a";
	case 66:
		return "b";
		// case 67:	return "c"; disable ctrl + c
	case 68:
		return "d";
	case 69:
		return "e";
	case 70:
		return "f";
	case 71:
		return "g";
	case 72:
		return "h";
	case 73:
		return "i";
	case 74:
		return "j";
	case 75:
		return "k";
	case 76:
		return "l";
	case 77:
		return "m";
	case 78:
		return "n";
	case 79:
		return "o";
	case 80:
		return "p";
	case 81:
		return "q";
	case 82:
		return "r";
	case 83:
		return "s";
	case 84:
		return "t";
	case 85:
		return "u";
		// case 86:	return "v"; disable ctrl + v
	case 87:
		return "w";
	case 88:
		return "x";
	case 89:
		return "y";
	case 90:
		return "z";
	case VK_NUMPAD0:
		return "num0";
	case VK_NUMPAD1:
		return "num1";
	case VK_NUMPAD2:
		return "num2";
	case VK_NUMPAD3:
		return "num3";
	case VK_NUMPAD4:
		return "num4";
	case VK_NUMPAD5:
		return "num5";
	case VK_NUMPAD6:
		return "num6";
	case VK_NUMPAD7:
		return "num7";
	case VK_NUMPAD8:
		return "num8";
	case VK_NUMPAD9:
		return "num9";
	case VK_F1:
		return "f1";
	case VK_F2:
		return "f2";
	case VK_F3:
		return "f3";
	case VK_F4:
		return "f4";
	case VK_F5:
		return "f5";
	case VK_F6:
		return "f6";
	case VK_F7:
		return "f7";
	case VK_F8:
		return "f8";
	case VK_F9:
		return "f9";
	case VK_F10:
		return "f10";
	case VK_F11:
		return "f11";
	case VK_F12:
		return "f12";
	case VK_F13:
		return "f13";
	case VK_F14:
		return "f14";
	case VK_F15:
		return "f15";
	case VK_F16:
		return "f16";
	case VK_F17:
		return "f17";
	case VK_F18:
		return "f18";
	case VK_F19:
		return "f19";
	case VK_F20:
		return "f20";
	case VK_F21:
		return "f21";
	case VK_F22:
		return "f22";
	case VK_F23:
		return "f23";
	case VK_F24:
		return "f24";
	default:
		return "";
	}
}

inline bool ends_with(std::string const &value, std::string const &ending)
{
	if (ending.size() > value.size())
		return false;
	return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

bool _isPoEActive()
{
	if (!g_bCheckWindow)
	{
		return true;
	}

	HWND hWnd = GetForegroundWindow();
	if (hWnd == NULL)
	{
		return false;
	}

	if (hWnd == g_hWnd)
	{
		return true;
	}

	char win_title[255];
	GetWindowText(hWnd, win_title, sizeof(win_title));
	if (std::string(win_title) != "Path of Exile")
	{
		return false;
	}

	DWORD dwPID;
	GetWindowThreadProcessId(hWnd, &dwPID);

	std::string strName = std::string("");
	HANDLE hHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, dwPID);
	if (hHandle)
	{
		TCHAR buffer[MAX_PATH];
		if (GetModuleFileNameEx(hHandle, 0, buffer, MAX_PATH))
		{
			strName = std::string(buffer);
		}
		CloseHandle(hHandle);
	}

	if (strName.empty())
	{
		return false;
	}

	if (ends_with(strName, "PathOfExile_x64Steam.exe") || ends_with(strName, "PathOfExileSteam.exe"))
	{
		g_hWnd = hWnd;
		g_strWindowProcessPath = strName;
		return true;
	}
	return false;
}

void _checkIsPoEActive()
{
	bool active = _isPoEActive();
	if (g_bIsWindowActive != active)
	{
		g_bIsWindowActive = active;
		_buildAndSendWindowActiveCallback();
	}
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode < 0 || nCode != HC_ACTION)
	{ // do not process message
		return CallNextHookEx(g_hKeyboardHook, nCode, wParam, lParam);
	}

	_checkIsPoEActive();

	KBDLLHOOKSTRUCT *p = (KBDLLHOOKSTRUCT *)lParam;
	switch (wParam)
	{
	case WM_SYSKEYDOWN:
	case WM_KEYDOWN:
	{
		switch (p->vkCode)
		{
		case VK_CONTROL:
		case VK_LCONTROL:
		case VK_RCONTROL:
			g_dModifiers[VK_CONTROL] = true;
			break;
		case VK_SHIFT:
		case VK_LSHIFT:
		case VK_RSHIFT:
			g_dModifiers[VK_SHIFT] = true;
			break;
		case VK_MENU:
		case VK_LMENU:
		case VK_RMENU:
			g_dModifiers[VK_MENU] = true;
			break;
		default:
			if (g_bIsWindowActive)
			{
				std::string key = _getKeyName(p->vkCode);
				if (key != "")
				{
					_buildAndSendKeyboardCallback(key);
				}
			}
		}
		break;
	}
	case WM_SYSKEYUP:
	case WM_KEYUP:
	{
		switch (p->vkCode)
		{
		case VK_CONTROL:
		case VK_LCONTROL:
		case VK_RCONTROL:
			g_dModifiers[VK_CONTROL] = false;
			break;
		case VK_SHIFT:
		case VK_LSHIFT:
		case VK_RSHIFT:
			g_dModifiers[VK_SHIFT] = false;
			break;
		case VK_MENU:
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

LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode < 0 || nCode != HC_ACTION)
	{ // do not process message
		return CallNextHookEx(g_hMouseHook, nCode, wParam, lParam);
	}

	_checkIsPoEActive();

	if (wParam == WM_MOUSEWHEEL)
	{
		std::string mwheel = "";

		MSLLHOOKSTRUCT *p = (MSLLHOOKSTRUCT *)lParam;
		int zDelta = GET_WHEEL_DELTA_WPARAM(p->mouseData);
		if (zDelta == 120)
		{
			mwheel = "mousewheelup";
		}
		else if (zDelta == -120)
		{
			mwheel = "mousewheeldown";
		}
		else
		{
			return CallNextHookEx(g_hMouseHook, nCode, wParam, lParam);
		}

		if (g_bIsWindowActive)
		{
			_buildAndSendKeyboardCallback(mwheel);
		}
	}

	return CallNextHookEx(g_hMouseHook, nCode, wParam, lParam);
}

void _addHook(const Nan::FunctionCallbackInfo<Value> &info)
{
	g_fKeyboardCallback = new Nan::Callback(info[0].As<Function>());
	g_fWindowActiveCallback = new Nan::Callback(info[1].As<Function>());

	g_dModifiers[VK_CONTROL] = false;
	g_dModifiers[VK_SHIFT] = false;
	g_dModifiers[VK_MENU] = false;

	Local<Boolean> checkWindow = info[2].As<Boolean>();
	g_bCheckWindow = checkWindow->Value();

	g_hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(NULL), 0);
	g_hMouseHook = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, GetModuleHandle(NULL), 0);

	_checkIsPoEActive();

	info.GetReturnValue().Set(Nan::New("true").ToLocalChecked());
}

void _clearHook(const Nan::FunctionCallbackInfo<Value> &info)
{
	UnhookWindowsHookEx(g_hKeyboardHook);
	info.GetReturnValue().Set(Nan::New("true").ToLocalChecked());
}

void Init(Local<Object> exports)
{
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