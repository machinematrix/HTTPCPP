#ifndef __EXPORT_MACROS__
#define __EXPORT_MACROS__

#ifdef DLL
	#ifdef _WIN32
		#ifdef BUILD_DLL
			#define EXPORT __declspec(dllexport)
		#else
			#define EXPORT __declspec(dllimport)
		#endif
	#elif defined(__linux__)
		#define EXPORT __attribute__((visibility("default")))
	#endif
#else
	#define EXPORT
#endif

#endif