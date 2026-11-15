# --- ts_analysis: the standards engine -------------------------------------
# Pure functions over arrays, Eigen only, NO Qt and no VTK (CLAUDE.md,
# settled decisions). It is the part every other target and every future
# binding stands on, and it must be usable without a GUI toolkit.
add_library(ts_analysis STATIC
    analysis/Curve.cpp
    analysis/Models.cpp
    analysis/Primitives.cpp
    analysis/Statistics.cpp
    analysis/Tension.cpp
    analysis/Version.cpp
)
target_include_directories(ts_analysis PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ts_analysis PUBLIC Eigen3::Eigen)
target_compile_definitions(ts_analysis PRIVATE TS_VERSION="${PROJECT_VERSION}")

# Configure-time guard: refuse a Qt or VTK dependency on the engine outright.
# The test `analysis_headers_compile_without_qt` covers the other route, a
# header that includes Qt without the link line saying so.
get_target_property(ts_analysis_links ts_analysis LINK_LIBRARIES)
foreach(lib IN LISTS ts_analysis_links)
    if(lib MATCHES "^(Qt|VTK)")
        message(FATAL_ERROR
            "ts_analysis must not depend on Qt or VTK (found ${lib}). "
            "See CLAUDE.md, settled decisions.")
    endif()
endforeach()

# --- ts_core: record, profiles, calibration, sequencer ----------------------
add_library(ts_core STATIC
    core/Annotations.cpp
    core/Exports.cpp
    core/ApplicationInfo.cpp
    core/Import.cpp
    core/Profile.cpp
    core/Record.cpp
    core/References.cpp
    core/Report.cpp
    core/Results.cpp
    core/Session.cpp
    core/Specimen.cpp
)
target_include_directories(ts_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ts_core PUBLIC ts_analysis Qt6::Core)

# The build identity written into every results file. Taken at configure
# time (tools/run-tests.sh and the hook configure on every run).
execute_process(COMMAND git describe --always --dirty --abbrev=12
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                OUTPUT_VARIABLE TS_BUILD OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET)
if(NOT TS_BUILD)
    set(TS_BUILD "unknown")
endif()
target_compile_definitions(ts_core PRIVATE TS_VERSION="${PROJECT_VERSION}"
                                           TS_BUILD="${TS_BUILD}")

# The profiles shipped with TenSense, compiled in so the CLI and the GUI
# find them without an install step: loadProfile("iso-6892-1-2019").
file(GLOB TS_SHIPPED_PROFILES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/profiles/*.json)
qt_add_resources(ts_core "shipped_profiles"
    PREFIX "/profiles"
    BASE ${CMAKE_SOURCE_DIR}/profiles
    FILES ${TS_SHIPPED_PROFILES})

# --- ts_gui: the application window -----------------------------------------
add_library(ts_gui STATIC
    gui/AnnotatedChart.cpp
    gui/ChartExport.cpp
    gui/MainWindow.cpp
)
target_include_directories(ts_gui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ts_gui PUBLIC ts_core Qt6::Widgets Qt6::OpenGLWidgets Qt6::Svg
    ${VTK_LIBRARIES})
vtk_module_autoinit(TARGETS ts_gui MODULES ${VTK_LIBRARIES})

add_executable(TenSense gui/main.cpp)
target_link_libraries(TenSense PRIVATE ts_gui)
vtk_module_autoinit(TARGETS TenSense MODULES ${VTK_LIBRARIES})

# --- ts-analyse: the headless command line ----------------------------------
add_executable(ts-analyse cli/main.cpp)
target_link_libraries(ts-analyse PRIVATE ts_core)

# --- Installation: the application, the command line, the desktop entry ----
include(GNUInstallDirs)
install(TARGETS TenSense ts-analyse RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ${CMAKE_SOURCE_DIR}/packaging/tensense.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES ${CMAKE_SOURCE_DIR}/packaging/tensense.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
install(FILES ${CMAKE_SOURCE_DIR}/LICENSE ${CMAKE_SOURCE_DIR}/NOTICE
        DESTINATION ${CMAKE_INSTALL_DOCDIR})
