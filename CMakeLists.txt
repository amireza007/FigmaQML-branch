cmake_minimum_required(VERSION 3.21)

project(FigmaQML LANGUAGES CXX)

include(FetchContent)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

message("Compiling for ${CMAKE_PREFIX_PATH}")
message("Compiling pre ${CMAKE_PROJECT_INCLUDE_BEFORE}")
message("Compiling tcn ${CMAKE_TOOLCHAIN_FILE}")

option(HAS_QUL "Build Qt for MCU support" TRUE)

if(EMSCRIPTEN)
    if(NOT DEFINED QT_HOST_PATH) # for github actions
        message("Host path not defined, use: ${HOST_ROOT}")
        set(QT_HOST_PATH "${HOST_ROOT}/gcc_64" CACHE STRING "" FORCE)
    endif()
    message(hellllllo)
    message("Host path: ${QT_HOST_PATH}")
    set(${QT_HOST_PATH} /gcc_64)
    find_package(Qt6 CONFIG COMPONENTS Core Quick Core5Compat ShaderTools QuickControls2 REQUIRED)
else()
    if(LINUX)
        set(HAS_EXECUTE TRUE)
    endif()
    if(HAS_QUL AND HAS_EXECUTE)
        set(EXTRA ${EXTRA}
            Qt6::SerialPort
        )
        set(EXTRA_PACKAGES ${EXTRA_PACKAGES}
            SerialPort
        )
    endif()
    find_package(Qt6 CONFIG COMPONENTS Core Quick Network Widgets Core5Compat ShaderTools QuickControls2 ${EXTRA_PACKAGES} REQUIRED)
endif()

# dog food
add_subdirectory(app_figma/FigmaQmlInterface)


set(SOURCES
    src/main.cpp
    qml/qml.qrc
    src/figmaget.cpp
    include/figmaget.h
    src/figmaqml.cpp
    include/figmaqml.h
    include/clipboard.h
    include/figmaparser.h
    include/downloads.h
    src/downloads.cpp
    include/figmadata.h
    include/figmadocument.h
    include/fontcache.h
    include/providers.h
    src/figmaparser.cpp
    include/orderedmap.h
    include/utils.h
    include/functorslot.h
    include/figmaprovider.h
    src/fontinfo.cpp
    include/fontinfo.h
    src/fontinfo.cpp
    src/appwrite.cpp
    include/appwrite.h
    include/execute_utils.h
    src/execute_utils.cpp
    app_figma/FigmaQmlInterface/FigmaQmlInterface.hpp
    app_figma/app.qrc # files needed to build an app
)

if(HAS_EXECUTE)
    if(DEFINED EMSCRIPTEN)
        message(FATAL_ERROR "Building & running is not supported in WASM")
    endif()
    set(SOURCES ${SOURCES}
        src/executeApp.cpp
    )
endif()

if(HAS_QUL)
    set(SOURCES ${SOURCES}
        src/qulwrite.cpp
        mcu_figma/mcu.qrc # files needed to build an app
    )
    if(HAS_EXECUTE)
        set(SOURCES ${SOURCES}
            include/qulInfo.h
            src/executeQul.cpp
        )
    endif()
endif()

if(EMSCRIPTEN)
    set(SOURCES ${SOURCES} src/wasmdialogs.cpp)
endif()


set(VERSION_NUMBER "3.1.1")

add_compile_definitions(VERSION_NUMBER=${VERSION_NUMBER})

set(NON_SOURCE
    todo.txt
    README.md
    aspell/spell_words.txt
    .github/workflows/figmaqml.yml
)

#add_executable(FigmaQML
qt_add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${NON_SOURCE}
    src/TestingFigmaQML.qml
)


if(NOT WIN32)
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic -Werror)
else()
  #  Windows does not support dual mode, we pick cmd mode
  #  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")
  #  set (EXTRA PUBLIC Qt5::WinMain)
endif()

if(MINGW)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations")
endif()

if(UNIX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations")
endif()

if (APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
            MACOSX_BUNDLE ON
            )
endif()

include_directories(include)

target_compile_definitions(${PROJECT_NAME} PRIVATE ASSERT_NESTED=1)

if(NOT EMSCRIPTEN)
target_compile_definitions(${PROJECT_NAME}
  PRIVATE $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>)
endif()

option(QT6_CONCURRENT FALSE)
option(QT6_SSL FALSE)

if(HAS_QUL)
    target_compile_definitions(${PROJECT_NAME} PRIVATE -DHAS_QUL)
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE -DUI_STYLE="Fusion")

set(COMMON_LIBS
    Qt6::Core
    Qt6::Quick
    Qt6::Network
    Qt6::Widgets
    Qt6::Core5Compat
    Qt6::ShaderTools
    Qt6::QuickControls2
    FigmaQmlInterface
    ${EXTRA})

# Create documentation and do spellcheck in the Linux
if(LINUX AND NOT NO_DOC)
    FetchContent_Declare(
      cmakedoc
      GIT_REPOSITORY https://github.com/mmertama/cmakedoc.git
      GIT_TAG        main
    )
    FetchContent_MakeAvailable(cmakedoc)
    include(${cmakedoc_SOURCE_DIR}/cmakedoc.cmake)
    set(CMAKEDOC_DOXYGEN_DOCUMENTS
        app_figma/FigmaQmlInterface/FigmaQmlInterface.hpp
        mcu_figma/FigmaQmlInterface/FigmaQmlInterface.hpp
    )
    add_doxygen(${PROJECT_NAME})
    set(CMAKEDOC_SPELL_DICTIONARY ${CMAKE_SOURCE_DIR}/aspell/spell_words.txt)
    set(CMAKEDOC_SPELL_EXCLUDE_DIRS
        build
        aspell
        modules
        res
        .git
        FigmaQML
        OpenSSL*)
    set(CMAKEDOC_SPELL_EXCLUDE_FILES
        \*.qmlproject*)
    add_spellcheck(${PROJECT_NAME})
endif()

if(EMSCRIPTEN)
    set(QT_WASM_INITIAL_MEMORY, "300MB")
    subdirs(modules/zlib)
    add_custom_target(zlib_target DEPENDS zlibstatic)
    set(ZLIB_INCLUDE  ${CMAKE_SOURCE_DIR}/modules/zlib)
    set(ZCONF_INCLUDE  ${CMAKE_BINARY_DIR}/modules/zlib)
    set(ZLIB_LIBRARY  zlibstatic)
    subdirs(modules/quazip)
    execute_process(COMMAND em++ --version OUTPUT_VARIABLE out_p OUTPUT_STRIP_TRAILING_WHITESPACE)
    add_custom_target(quazip DEPENDS QuaZip)
    add_dependencies(quazip zlib_target)
    include_directories(${CMAKE_SOURCE_DIR}/modules/quazip ${ZLIB_INCLUDE} ${ZCONF_INCLUDE})
    string(REGEX MATCH "[0-9]+\.[0-9]+\.[0-9]+" ver_p "${out_p}")
    #if(NOT ${ver_p} STREQUAL "3.1.14")
    #    message(FATAL_ERROR "emsdk 3.1.14 expected now!, you have ${ver_p}")
    #endif()
    message("emsdk ${ver_p}")
    configure_file(res/favicon.ico "${CMAKE_CURRENT_BINARY_DIR}/favicon.ico" COPYONLY)
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        SET(EM_FLAGS -sASSERTIONS=2 -sRUNTIME_LOGGING=1 -sSAFE_HEAP=1)
    endif()
    target_link_options(${PROJECT_NAME} PUBLIC -sASYNCIFY -Os -sASYNCIFY_STACK_SIZE=65535 ${EM_FLAGS})
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        -DNO_CONCURRENT
        -DNO_SSL
        -DWASM_FILEDIALOGS)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_NAME}.html $<CONFIG>/${PROJECT_NAME}.html
        COMMAND ${CMAKE_COMMAND} -E copy qtloader.js $<CONFIG>/qtloader.js
        COMMAND ${CMAKE_COMMAND} -E copy qtlogo.svg $<CONFIG>/qtlogo.svg
        COMMAND ${CMAKE_COMMAND} -E copy favicon.ico $<CONFIG>/favicon.ico
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Obvious bug in QT 6.4 and these files are in the wrong place")
    target_link_libraries(${PROJECT_NAME}
      PRIVATE QuaZip
      PRIVATE ${COMMON_LIBS})
else()
    if(NOT WIN32) # windows finds internal?
        find_package(OpenSSL REQUIRED)
        string(REPLACE "." ";" VERSION_LIST OPENSSL_VERSION)
        list(GET VERSION_LIST 0 SSL_MAJOR)
        if(${SSL_MAJOR} LESS 3 )
            message(WARNING "SSL version must be at least 3.0.0, your runtime have ${OPENSSL_VERSION}. Hint for QtCreator try add $\{QT_DIR\}/plugins/tls in Run settings (LD_LIBRARY_PATH in Linux)")
        endif()
    endif()
    if(NOT QT6_CONCURRENT)
        target_compile_definitions(${PROJECT_NAME} PRIVATE -DNO_CONCURRENT)
    endif()
    if(NOT QT6_SSL)
        target_compile_definitions(${PROJECT_NAME} PRIVATE -DNO_SSL)
    endif()

    if(HAS_EXECUTE)
        target_compile_definitions(${PROJECT_NAME} PRIVATE -DHAS_EXECUTE)
    endif()

    if(COPY_SSL AND LINUX)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/OpenSSL_ubuntu/*" "${CMAKE_CURRENT_BINARY_DIR}")
    endif()

    target_link_libraries(${PROJECT_NAME}
      PRIVATE ${COMMON_LIBS})
endif()

