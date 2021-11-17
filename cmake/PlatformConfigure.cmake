if(UNIX)
    set(PLATFORM_INCLUDES ${PLATFORM_INCLUDES} include/Platform/Unix)
    set(PLATFORM_INCLUDES ${PLATFORM_INCLUDES} include/Platform/DirectXMath)
    add_definitions(-D "SAKURA_TARGET_PLATFORM_UNIX")
    if(APPLE)
        add_definitions(-D "SAKURA_TARGET_PLATFORM_MACOS")
        set(SAKURA_TARGET_PLATFORM "mac")
        set(TARGET_MAC 1)
    elseif(${CMAKE_SYSTEM_NAME} MATCHES ANDROID)
        add_definitions(-D "SAKURA_TARGET_PLATFORM_ANDROID")
        set(SAKURA_TARGET_PLATFORM "android")
        set(TARGET_ANDROID 1)
    elseif(${CMAKE_SYSTEM_NAME} MATCHES Emscripten)
        add_definitions(-D "SAKURA_TARGET_PLATFORM_EMSCRIPTEN")
        set(SAKURA_TARGET_PLATFORM "web")
        set(TARGET_WA 1)   #Web
    else(APPLE)
        add_definitions(-D "SAKURA_TARGET_PLATFORM_LINUX")
        set(SAKURA_TARGET_PLATFORM "linux")
        set(TARGET_LINUX 1)   #Web
    endif(APPLE)
elseif(WIN32)
    add_definitions(-D "SAKURA_TARGET_PLATFORM_WIN")
    set(SAKURA_TARGET_PLATFORM "windows")
    set(TARGET_WIN 1)   #Web
    add_definitions(-D "_CRT_SECURE_NO_WARNINGS")
    add_definitions(-D "UNICODE")
    add_definitions(-D "_UNICODE")
elseif(__COMPILER_PS5)
    message(STATUS "Platform PS5")
    set(SAKURA_TARGET_PLATFORM "prospero")
    add_definitions(-D "SAKURA_TARGET_PLATFORM_PLAYSTATION")
endif(UNIX)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CLANG_DISABLED_WARNINGS "${CLANG_DISABLED_WARNINGS} -Wno-ignored-attributes -Wno-logical-op-parentheses")
    set(CLANG_DISABLED_WARNINGS "${CLANG_DISABLED_WARNINGS} -Wno-nullability-completeness")
    set(CLANG_PROMOTED_WARNINGS "${CLANG_PROMOTED_WARNINGS} -Werror=return-type -Werror=incompatible-function-pointer-types")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CLANG_DISABLED_WARNINGS} ${CLANG_PROMOTED_WARNINGS}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CLANG_DISABLED_WARNINGS} ${CLANG_PROMOTED_WARNINGS}")
endif()