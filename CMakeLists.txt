cmake_minimum_required(VERSION 3.20)
project(VEMCODE VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Generate compile_commands.json for IDE language servers (clangd, IntelliSense)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Prefer native system Qt6 over cross-compiled versions
list(PREPEND CMAKE_PREFIX_PATH /usr/lib64)

# Qt6 modules we need right now
# We'll add WebEngineWidgets later for the Monaco editor
# Note: QIcon/QPixmap load .svg (e.g. the app logo) via the qsvgicon/qsvg
# plugins at runtime automatically -- no Qt6::Svg link needed for that.
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

# Auto-run Qt's code generators on files that need it
# MOC  = processes Q_OBJECT macros
# UIC  = processes .ui files (we don't have any yet)
# RCC  = processes .qrc resource files (docs/resources.qrc, for the app logo)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Embed injected_header.h as a C string at configure time.
# set_property tells the build system to re-run cmake when the file changes.
# avr_registers.inc / avr_timers.inc are concatenated in here (not treated as
# conditional libraries like Servo/Wire/SPI below) since they must be
# injected into every sketch unconditionally -- see the comment at the top
# of avr_registers.inc for why.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/core/build/injected_header.inc"
    "${CMAKE_SOURCE_DIR}/src/core/build/libs/avr_registers.inc"
    "${CMAKE_SOURCE_DIR}/src/core/build/libs/avr_timers.inc")
file(READ "${CMAKE_SOURCE_DIR}/src/core/build/injected_header.inc" INJECTED_HEADER_CONTENT)
file(READ "${CMAKE_SOURCE_DIR}/src/core/build/libs/avr_registers.inc" AVR_REGISTERS_CONTENT)
file(READ "${CMAKE_SOURCE_DIR}/src/core/build/libs/avr_timers.inc" AVR_TIMERS_CONTENT)
string(APPEND INJECTED_HEADER_CONTENT "\n${AVR_REGISTERS_CONTENT}")
string(APPEND INJECTED_HEADER_CONTENT "\n${AVR_TIMERS_CONTENT}")
configure_file(
    "${CMAKE_SOURCE_DIR}/src/core/build/injected_header.cpp.in"
    "${CMAKE_BINARY_DIR}/generated/injected_header.cpp"
    @ONLY
)

# Embed injectable library .inc files as C strings at configure time. Each one's content
# replaces that library's #include line in the sketch (see Preprocessor::strip_includes), so
# it's only ever present in sketches that actually use it. Drop a <name>.inc into
# src/core/build/libs/ and its g_<name>_lib symbol is generated automatically -- still needs
# an extern decl in preprocessor.h and a strip_includes() map entry to actually be used.
file(GLOB LIB_INC_FILES "${CMAKE_SOURCE_DIR}/src/core/build/libs/*.inc")
# avr_registers.inc/avr_timers.inc are unconditional (concatenated into every sketch above),
# not one of these #include-triggered optional libs -- excluded so their content isn't embedded twice.
list(REMOVE_ITEM LIB_INC_FILES
    "${CMAKE_SOURCE_DIR}/src/core/build/libs/avr_registers.inc"
    "${CMAKE_SOURCE_DIR}/src/core/build/libs/avr_timers.inc")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${LIB_INC_FILES})

set(GENERATED_LIB_SOURCES "")
foreach(lib_inc ${LIB_INC_FILES})
    get_filename_component(lib_name "${lib_inc}" NAME_WE)
    file(READ "${lib_inc}" lib_content)
    set(gen_cpp "${CMAKE_BINARY_DIR}/generated/${lib_name}_lib.cpp")
    file(WRITE "${gen_cpp}" "const char* g_${lib_name}_lib = R\"VBLIB(\n${lib_content}\n)VBLIB\";\n")
    list(APPEND GENERATED_LIB_SOURCES "${gen_cpp}")
endforeach()

# Component plugin files — drop a .cpp into src/components/ and it's picked up automatically
file(GLOB COMPONENT_SOURCES "${CMAKE_SOURCE_DIR}/src/components/*.cpp")

# All source files for the main application
set(SOURCES
    src/main.cpp
    src/ui/mainwindow.cpp
    src/ui/editor/sketchlinter.cpp
    src/ui/editor/keybindmanager.cpp
    src/ui/editor/findreplacebar.cpp
    src/ui/canvaswidget.cpp
    src/ui/editor/codehighlighter.cpp
    src/ui/editor/linenumberarea.cpp
    src/ui/panels/signaltimeline.cpp
    src/ui/panels/serialplotter.cpp
    src/ui/panels/variablewatch.cpp
    src/ui/panels/devicespanel.cpp
    src/ui/panels/spipanel.cpp
    src/ui/panels/addcomponentdialog.cpp
    src/ui/settingsdialog.cpp
    src/ui/apptheme.cpp
    src/ui/timelinerecorder.cpp
    src/core/runtime/arduinoruntime.cpp
    src/core/host/sketchhost.cpp
    src/core/host/sketchhostthread.cpp
    src/core/host/timeline.cpp
    src/core/build/compiler.cpp
    src/core/build/preprocessor.cpp
    src/core/circuit/circuitdetector.cpp
    src/core/circuit/componentitem.cpp
    src/core/circuit/componentregistry.cpp
    src/lsan_suppressions.cpp
    docs/resources.qrc
    ${CMAKE_BINARY_DIR}/generated/injected_header.cpp
    ${GENERATED_LIB_SOURCES}
    ${COMPONENT_SOURCES}
)

set(HEADERS
    src/ui/mainwindow.h
    src/ui/editor/sketchlinter.h
    src/ui/editor/keybindmanager.h
    src/ui/editor/findreplacebar.h
    src/ui/canvaswidget.h
    src/ui/editor/codehighlighter.h
    src/ui/editor/linenumberarea.h
    src/ui/panels/signaltimeline.h
    src/ui/panels/serialplotter.h
    src/ui/panels/variablewatch.h
    src/ui/panels/devicespanel.h
    src/ui/panels/spipanel.h
    src/ui/panels/addcomponentdialog.h
    src/ui/settingsdialog.h
    src/ui/apptheme.h
    src/ui/timelinerecorder.h
    src/core/runtime/arduinoapi.h
    src/core/runtime/arduinoruntime.h
    src/core/host/sketchhost.h
    src/core/host/sketchhostthread.h
    src/core/host/timeline.h
    src/core/build/compiler.h
    src/core/build/preprocessor.h
    src/core/circuit/circuitdetector.h
)

# Create the executable
qt_add_executable(VEMCODE ${SOURCES} ${HEADERS})
set_target_properties(VEMCODE PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/app
)

# Include paths -- everything resolves from the project root
target_include_directories(VEMCODE PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core/runtime
    ${CMAKE_SOURCE_DIR}/src/core/host
    ${CMAKE_SOURCE_DIR}/src/ui
)

# Link Qt modules
target_link_libraries(VEMCODE PRIVATE
    Qt6::Core
    Qt6::Widgets
)

target_compile_definitions(VEMCODE PRIVATE
    PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

# Linux: link against dl for dlopen/dlsym used by SketchHost
if(UNIX AND NOT APPLE)
    target_link_libraries(VEMCODE PRIVATE dl)
endif()

# Windows-specific: hide the console window in release builds
# In debug builds we keep it so we can see std::cout output
if(WIN32)
    set_target_properties(VEMCODE PROPERTIES
        WIN32_EXECUTABLE $<CONFIG:Release>
    )
endif()