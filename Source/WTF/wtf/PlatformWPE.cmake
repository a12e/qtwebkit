list(APPEND WTF_PUBLIC_HEADERS
    glib/GLibUtilities.h
    glib/GMutexLocker.h
    glib/GRefPtr.h
    glib/GTypedefs.h
    glib/GUniquePtr.h
    glib/RunLoopSourcePriority.h
    glib/WTFGType.h

    linux/ProcessMemoryFootprint.h
    linux/CurrentProcessMemoryStatus.h
)

list(APPEND WTF_SOURCES
    generic/MainThreadGeneric.cpp
    generic/MemoryFootprintGeneric.cpp
    generic/WorkQueueGeneric.cpp

    glib/FileSystemGlib.cpp
    glib/GLibUtilities.cpp
    glib/GRefPtr.cpp
    glib/RunLoopGLib.cpp
    glib/URLGLib.cpp

    linux/CurrentProcessMemoryStatus.cpp
    linux/MemoryPressureHandlerLinux.cpp

    posix/OSAllocatorPOSIX.cpp
    posix/ThreadingPOSIX.cpp

    text/unix/TextBreakIteratorInternalICUUnix.cpp

    unix/CPUTimeUnix.cpp
    unix/LanguageUnix.cpp
    unix/UniStdExtrasUnix.cpp
)

list(APPEND WTF_LIBRARIES
    ${CMAKE_THREAD_LIBS_INIT}
    ${GLIB_GIO_LIBRARIES}
    ${GLIB_GOBJECT_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${ZLIB_LIBRARIES}
)

list(APPEND WTF_SYSTEM_INCLUDE_DIRECTORIES
    ${GIO_UNIX_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
)
