cmake_minimum_required(VERSION 3.16)
project(client-tests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
find_package(Qt6 REQUIRED COMPONENTS Core Network Widgets Test)
enable_testing()
add_executable(icon_generator ../tools/generate_icons.cpp)
target_link_libraries(icon_generator PRIVATE Qt6::Gui)
add_executable(client_tests client_tests.cpp
    ../src/client/TrafficPolicy.cpp ../src/client/ServerCatalog.cpp
    ../src/client/SocksBridge.cpp ../src/client/VpnRoutes.cpp ../src/client/AppRouter.cpp
    ../src/client/GamingMode.cpp
    ../src/dialog/ServerLibrary.cpp ../src/dialog/TrafficPolicyDialog.cpp
    ../src/cryptdata.cpp ../src/logger.cpp)
target_sources(client_tests PRIVATE ../src/dialog/SessionActivity.cpp ../src/client/ClientTheme.cpp ../src/openconnect-gui.qrc)
target_sources(client_tests PRIVATE ../src/client/ClientLanguage.cpp ../src/dialog/AboutDialog.cpp)
target_sources(client_tests PRIVATE ../src/client/InstalledComponents.cpp ../src/dialog/ComponentsWidget.cpp)
target_include_directories(client_tests PRIVATE ../src)
target_link_libraries(client_tests PRIVATE Qt6::Core Qt6::Network Qt6::Widgets Qt6::Test)
if(WIN32)
    target_link_libraries(client_tests PRIVATE ws2_32 iphlpapi advapi32 shell32 ole32 uuid version)
endif()
add_test(NAME client_tests COMMAND client_tests)
set_tests_properties(client_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
