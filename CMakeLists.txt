cmake_minimum_required(VERSION 3.24)

project(kwin4_effect_window_shelf VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_SKIP_RPATH TRUE)

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

find_package(Qt6 REQUIRED COMPONENTS Core Gui CoreTools Widgets)
find_package(KF6CoreAddons REQUIRED)
find_package(KF6Config REQUIRED)
find_package(KF6GlobalAccel REQUIRED)
find_package(KF6I18n REQUIRED)
find_package(KF6KCMUtils REQUIRED)
find_package(KWin CONFIG REQUIRED)

# KWin discovers native effects among Qt plugins, which may be installed in a
# different directory from other KDE plugins.
get_target_property(_qtpaths_executable Qt6::qtpaths IMPORTED_LOCATION)
if(NOT _qtpaths_executable)
    get_target_property(_qtpaths_executable Qt6::qtpaths IMPORTED_LOCATION_RELEASE)
endif()
execute_process(
    COMMAND "${_qtpaths_executable}" --query QT_INSTALL_PLUGINS
    RESULT_VARIABLE _qtpaths_result
    OUTPUT_VARIABLE KDE_INSTALL_PLUGINDIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT _qtpaths_result EQUAL 0 OR NOT KDE_INSTALL_PLUGINDIR)
    message(FATAL_ERROR "Could not determine Qt's plugin install directory")
endif()

kcoreaddons_add_plugin(kwin4_effect_window_shelf
    SOURCES
        main.cpp
        windowshelfeffect.cpp
        metadata.json
    INSTALL_NAMESPACE "kwin/effects/plugins"
)

set_property(TARGET kwin4_effect_window_shelf APPEND PROPERTY AUTOMOC_MACRO_NAMES KWIN_EFFECT_FACTORY)

target_link_libraries(kwin4_effect_window_shelf PRIVATE
    Qt6::Core
    Qt6::Gui
    KF6::ConfigCore
    KF6::CoreAddons
    KF6::GlobalAccel
    KWin::kwin
)

kcoreaddons_add_plugin(kwin4_effect_window_shelf_config
    SOURCES
        windowshelfeffectconfig.cpp
        config.json
    INSTALL_NAMESPACE "kwin/effects/configs"
)

target_link_libraries(kwin4_effect_window_shelf_config PRIVATE
    Qt6::Core
    Qt6::Widgets
    KF6::ConfigCore
    KF6::CoreAddons
    KF6::I18n
    KF6::KCMUtils
)
