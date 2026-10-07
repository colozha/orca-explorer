# SPDX-FileCopyrightText: 2026 Orca Explorer Contributors
# SPDX-License-Identifier: CC0-1.0
add_library(orca-portal-dialog STATIC filechooserdialog.cpp filechooserdialog.h portaltypes.h)
target_include_directories(orca-portal-dialog PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/src)
target_link_libraries(orca-portal-dialog PUBLIC dolphinstatic dolphinprivate KF6::KIOFileWidgets KF6::WindowSystem Qt6::DBus)
add_executable(orca-explorer-portal main.cpp filechooserbackend.cpp filechooserbackend.h)
target_link_libraries(orca-explorer-portal PRIVATE orca-portal-dialog)
target_compile_definitions(orca-explorer-portal PRIVATE ORCA_INSTALL_PLUGINDIR="${KDE_INSTALL_FULL_PLUGINDIR}")
set_target_properties(orca-explorer-portal PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/apps" INSTALL_RPATH "$ORIGIN/${ORCA_BINARY_RPATH}")
install(TARGETS orca-explorer-portal ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
install(FILES orca.portal DESTINATION ${KDE_INSTALL_DATADIR}/xdg-desktop-portal/portals)
ecm_generate_dbus_service_file(NAME org.freedesktop.impl.portal.desktop.orca
    EXECUTABLE "${ORCA_LAUNCHER_BINDIR}/orca-explorer-portal"
    SYSTEMD_SERVICE orca-explorer-portal.service DESTINATION ${KDE_INSTALL_DBUSSERVICEDIR})
ecm_install_configured_files(INPUT orca-explorer-portal.service.in DESTINATION ${KDE_INSTALL_SYSTEMDUSERUNITDIR})
if(ORCA_INSTALL_DEFAULT_FILE_MANAGER)
    ecm_generate_dbus_service_file(NAME org.freedesktop.FileManager1
        EXECUTABLE "${ORCA_LAUNCHER_BINDIR}/orca-explorer --daemon --file-manager-service"
        SYSTEMD_SERVICE orca-explorer-filemanager.service DESTINATION ${KDE_INSTALL_DBUSSERVICEDIR})
    ecm_install_configured_files(INPUT orca-explorer-filemanager.service.in DESTINATION ${KDE_INSTALL_SYSTEMDUSERUNITDIR})
endif()
if(BUILD_TESTING)
    find_package(Qt6Test CONFIG REQUIRED)
    include(ECMAddTests)
    find_program(DBUS_RUN_SESSION dbus-run-session REQUIRED)
    add_executable(orca-portalbackendtest portalbackendtest.cpp filechooserbackend.cpp filechooserbackend.h)
    target_link_libraries(orca-portalbackendtest PRIVATE orca-portal-dialog Qt6::Test)
    add_test(NAME orca-portalbackendtest COMMAND ${DBUS_RUN_SESSION} -- $<TARGET_FILE:orca-portalbackendtest>)
    target_compile_definitions(orca-portalbackendtest PRIVATE ORCA_TEST_EXECUTABLE="$<TARGET_FILE:dolphin>" ORCA_TEST_PORTAL_EXECUTABLE="$<TARGET_FILE:orca-explorer-portal>")
    ecm_add_test(filechoosertest.cpp TEST_NAME orca-filechoosertest LINK_LIBRARIES orca-portal-dialog Qt6::Test)
endif()
