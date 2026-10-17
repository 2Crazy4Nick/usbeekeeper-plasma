add_library(whatcable-plasmoid-plugin SHARED
    plugin/WhatCablePlugin.cpp
    plugin/DeviceModel.cpp
    plugin/PhysicalDeviceProfile.cpp
    plugin/UsbeehiveClient.cpp
)

# Profile JSON files are discovered automatically.  Adding a new profile file
# triggers reconfiguration and does not require a CMake file-list edit.
file(GLOB PROFILE_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/profiles/*.json")
qt_add_resources(whatcable-plasmoid-plugin physical_profiles
    PREFIX "/org/kde/whatcable/profiles"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/profiles"
    FILES ${PROFILE_FILES})

target_link_libraries(whatcable-plasmoid-plugin PRIVATE
    Qt6::Core
    Qt6::DBus
    Qt6::Quick
)

# Install the QML plugin to the Qt QML import path so "import org.kde.whatcable" works
set(QML_INSTALL_DIR "${KDE_INSTALL_QMLDIR}/org/kde/whatcable")
install(TARGETS whatcable-plasmoid-plugin DESTINATION ${QML_INSTALL_DIR})
install(FILES plugin/qmldir DESTINATION ${QML_INSTALL_DIR})

plasma_install_package(. org.kde.whatcable)

# Build a kpackagetool6-installable package directory under build/. This is a
# separate target so QML/configuration edits refresh the local package even
# when the C++ plugin itself does not relink.
set(PKG_DIR "${CMAKE_BINARY_DIR}/pkg/org.kde.whatcable")
set(PKG_QML_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/metadata.json"
    "${CMAKE_CURRENT_SOURCE_DIR}/contents/ui/main.qml"
    "${CMAKE_CURRENT_SOURCE_DIR}/contents/ui/CompactView.qml"
    "${CMAKE_CURRENT_SOURCE_DIR}/contents/ui/DeviceCard.qml"
    "${CMAKE_CURRENT_SOURCE_DIR}/contents/ui/PowerSourceList.qml"
    "${CMAKE_CURRENT_SOURCE_DIR}/contents/config/config.qml"
    "${CMAKE_CURRENT_SOURCE_DIR}/contents/config/main.xml"
    "${CMAKE_CURRENT_SOURCE_DIR}/contents/ui/config/ConfigGeneral.qml"
)
add_custom_target(whatcable-plasmoid-package ALL
    DEPENDS whatcable-plasmoid-plugin ${PKG_QML_SOURCES}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PKG_DIR}/contents/ui"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/metadata.json" "${PKG_DIR}/metadata.json"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/contents/ui/main.qml"
        "${CMAKE_CURRENT_SOURCE_DIR}/contents/ui/CompactView.qml"
        "${CMAKE_CURRENT_SOURCE_DIR}/contents/ui/DeviceCard.qml"
        "${CMAKE_CURRENT_SOURCE_DIR}/contents/ui/PowerSourceList.qml"
        "${PKG_DIR}/contents/ui/"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PKG_DIR}/contents/config" "${PKG_DIR}/contents/ui/config"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/contents/config/config.qml"
        "${CMAKE_CURRENT_SOURCE_DIR}/contents/config/main.xml"
        "${PKG_DIR}/contents/config/"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/contents/ui/config/ConfigGeneral.qml"
        "${PKG_DIR}/contents/ui/config/"
    # Also stage the QML plugin for local development
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/qml/org/kde/whatcable"
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:whatcable-plasmoid-plugin>"
        "${CMAKE_BINARY_DIR}/qml/org/kde/whatcable/"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/plugin/qmldir"
        "${CMAKE_BINARY_DIR}/qml/org/kde/whatcable/"
    COMMENT "Assembling plasmoid package and QML plugin"
)

if(BUILD_TESTING)
    add_executable(physical_profile_match_tests
        "${CMAKE_SOURCE_DIR}/tests/physical_profile_match_test.cpp"
        plugin/PhysicalDeviceProfile.cpp)
    qt_add_resources(physical_profile_match_tests physical_profiles_test
        PREFIX "/org/kde/whatcable/profiles"
        BASE "${CMAKE_CURRENT_SOURCE_DIR}/profiles"
        FILES ${PROFILE_FILES})
    target_include_directories(physical_profile_match_tests PRIVATE plugin)
    target_link_libraries(physical_profile_match_tests PRIVATE
        whatcable-plasmoid-plugin Qt6::Core)
    add_test(NAME physical_profile_match_tests COMMAND physical_profile_match_tests)

    add_executable(device_model_regression_tests
        "${CMAKE_SOURCE_DIR}/tests/device_model_regression_test.cpp"
        plugin/DeviceModel.cpp
        plugin/PhysicalDeviceProfile.cpp
        plugin/UsbeehiveClient.cpp)
    qt_add_resources(device_model_regression_tests physical_profiles_model_test
        PREFIX "/org/kde/whatcable/profiles"
        BASE "${CMAKE_CURRENT_SOURCE_DIR}/profiles"
        FILES ${PROFILE_FILES})
    target_include_directories(device_model_regression_tests PRIVATE plugin)
    target_link_libraries(device_model_regression_tests PRIVATE
        Qt6::Core Qt6::DBus Qt6::Quick)
    add_test(NAME device_model_regression_tests COMMAND device_model_regression_tests)
endif()
