// The version of kurva-switcher, in one place. Resource.rc turns it into the VERSIONINFO
// resource (file properties, Task Manager), the About dialog shows it. Bump it before a release.
//
// Resource.rc includes this file, so it holds plain #defines only: the resource compiler
// understands nothing else.
#ifndef KURVA_VERSION_H
#define KURVA_VERSION_H

#define KURVA_VERSION_MAJOR 0
#define KURVA_VERSION_MINOR 4
#define KURVA_VERSION_PATCH 0
#define KURVA_VERSION_STRING "0.4.0"

#ifndef RC_INVOKED
#define KURVA_WIDEN_(text) L##text
#define KURVA_WIDEN(text) KURVA_WIDEN_(text)
#define KURVA_VERSION_WSTRING KURVA_WIDEN(KURVA_VERSION_STRING)
#endif

#endif  // KURVA_VERSION_H
