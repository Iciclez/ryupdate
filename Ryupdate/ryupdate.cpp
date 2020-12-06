#include "ryupdate.hpp"
#include "mainwindow.hpp"

#include <windows.h>

#include "qapplication.h"

#include <memory>
#include <thread>
#include <chrono>
#include <QtPlugin>

Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin);
Q_IMPORT_PLUGIN(QWindowsVistaStylePlugin);
//Q_IMPORT_PLUGIN(QICOPlugin);
//Q_IMPORT_PLUGIN(QJpegPlugin);
//Q_IMPORT_PLUGIN(QGifPlugin);

zephyrus z;

#pragma comment(linker, "\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

typedef void *lpvoid;

HINSTANCE instance;

ryupdate::ryupdate()
{
}

ryupdate::~ryupdate()
{
}

void ryupdate::initialize()
{
	static LPTHREAD_START_ROUTINE mainwindow_initialize = [](void *) -> unsigned long {
		int32_t argc = 0;
		QApplication a(argc, 0);

		mainwindow m;
		m.show();

		int32_t result = a.exec();

		ExitThread(0);

		return result;
	};

	CreateThread(0, 0, mainwindow_initialize, 0, 0, 0);
}

void ryupdate::destroy()
{
	FreeLibraryAndExitThread(instance, 0);
}

BOOL WINAPI DllMain(_In_ HINSTANCE hinstDLL, _In_ DWORD fdwReason, _In_ LPVOID lpvReserved)
{
	UNREFERENCED_PARAMETER(lpvReserved);

	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		instance = hinstDLL;
		DisableThreadLibraryCalls(hinstDLL);
		ryupdate::initialize();
		break;

	case DLL_PROCESS_DETACH:
		ryupdate::destroy();
		break;
	}

	return TRUE;
}
