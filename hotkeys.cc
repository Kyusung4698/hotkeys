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

	if (
		ends_with(strName, "PathOfExile_x64_KG.exe") || ends_with(strName, "PathOfExile_KG.exe") ||
		ends_with(strName, "PathOfExile_x64Steam.exe") || ends_with(strName, "PathOfExileSteam.exe") ||
		ends_with(strName, "PathOfExile_x64.exe") || ends_with(strName, "PathOfExile.exe"))
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