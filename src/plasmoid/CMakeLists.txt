add_library(usbeekeeper-plasmoid-plugin SHARED
    plugin/USBeeKeeperPlugin.cpp
    plugin/DeviceModel.cpp
    plugin/PhysicalDeviceProfile.cpp
    plugin/UsbeehiveClient.cpp
)

# Profile JSON files are discovered automatically.  Adding a new profile file
# triggers reconfiguration and does not require a CMake file-list edit.
file(GLOB PROFILE_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/profiles/*.json")
qt_add_resources(usbeekeeper-plasmoid-plugin physical_profiles
    PREFIX "/io/github/_2crazy4nick/usbeekeeper/profiles"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/profiles"
    FILES ${PROFILE_FILES})

target_link_libraries(usbeekeeper-plasmoid-plugin PRIVATE
    Qt6::Core
    Qt6::DBus
    Qt6::Quick
)

# Install the QML plugin to the Qt QML import path so
# "import io.github._2crazy4nick.usbeekeeper" works.
set(QML_INSTALL_DIR "${KDE_INSTALL_QMLDIR}/io/github/_2crazy4nick/usbeekeeper")
install(TARGETS usbeekeeper-plasmoid-plugin DESTINATION ${QML_INSTALL_DIR})
install(FILES plugin/qmldir DESTINATION ${QML_INSTALL_DIR})

# Build a kpackagetool6-installable package directory under build/. This is a
# separate target so QML/configuration edits refresh the local package even
# when the C++ plugin itself does not relink.
set(PKG_DIR "${CMAKE_BINARY_DIR}/pkg/io.github._2crazy4nick.usbeekeeper")
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
add_custom_target(usbeekeeper-plasmoid-package ALL
    DEPENDS usbeekeeper-plasmoid-plugin ${PKG_QML_SOURCES}
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
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/qml/io/github/_2crazy4nick/usbeekeeper"
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:usbeekeeper-plasmoid-plugin>"
        "${CMAKE_BINARY_DIR}/qml/io/github/_2crazy4nick/usbeekeeper/"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/plugin/qmldir"
        "${CMAKE_BINARY_DIR}/qml/io/github/_2crazy4nick/usbeekeeper/"
    COMMENT "Assembling plasmoid package and QML plugin"
)

# Install the same clean package assembled for kpackagetool6 and release
# archives.  Installing the source directory itself would also copy the C++
# plugin sources and bundled profile definitions into the Plasma package.
plasma_install_package("${PKG_DIR}" io.github._2crazy4nick.usbeekeeper)

if(BUILD_TESTING)
    add_executable(physical_profile_match_tests
        "${CMAKE_SOURCE_DIR}/tests/physical_profile_match_test.cpp"
        plugin/PhysicalDeviceProfile.cpp)
    qt_add_resources(physical_profile_match_tests physical_profiles_test
        PREFIX "/io/github/_2crazy4nick/usbeekeeper/profiles"
        BASE "${CMAKE_CURRENT_SOURCE_DIR}/profiles"
        FILES ${PROFILE_FILES})
    target_include_directories(physical_profile_match_tests PRIVATE plugin)
    target_link_libraries(physical_profile_match_tests PRIVATE
        usbeekeeper-plasmoid-plugin Qt6::Core)
    add_test(NAME physical_profile_match_tests COMMAND physical_profile_match_tests)

    add_executable(device_model_regression_tests
        "${CMAKE_SOURCE_DIR}/tests/device_model_regression_test.cpp"
        plugin/DeviceModel.cpp
        plugin/PhysicalDeviceProfile.cpp
        plugin/UsbeehiveClient.cpp)
    qt_add_resources(device_model_regression_tests physical_profiles_model_test
        PREFIX "/io/github/_2crazy4nick/usbeekeeper/profiles"
        BASE "${CMAKE_CURRENT_SOURCE_DIR}/profiles"
        FILES ${PROFILE_FILES})
    target_include_directories(device_model_regression_tests PRIVATE plugin)
    target_link_libraries(device_model_regression_tests PRIVATE
        Qt6::Core Qt6::DBus Qt6::Quick)
    add_test(NAME device_model_regression_tests COMMAND device_model_regression_tests)
endif()
