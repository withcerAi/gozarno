# lwIP 2.2.1, BSD licensed; packets never enter the host IP stack.
enable_language(C)
set(LWIP_DIR "${CMAKE_CURRENT_LIST_DIR}/../external/lwip/src")
file(GLOB LWIP_CORE CONFIGURE_DEPENDS "${LWIP_DIR}/core/*.c" "${LWIP_DIR}/core/ipv4/*.c")
add_library(gozarno_lwip STATIC ${LWIP_CORE})
target_include_directories(gozarno_lwip PUBLIC "${LWIP_DIR}/include" "${CMAKE_CURRENT_LIST_DIR}/../src/client/lwip")
