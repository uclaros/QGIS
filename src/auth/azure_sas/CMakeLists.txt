set(AUTH_AZURESAS_SRCS
  core/qgsauthazuresasmethod.cpp
)

set(AUTH_AZURESAS_HDRS
  core/qgsauthazuresasmethod.h
)

set(AUTH_AZURESAS_UIS_H "")

if (WITH_GUI)
  set(AUTH_AZURESAS_SRCS ${AUTH_AZURESAS_SRCS}
    gui/qgsauthazuresasedit.cpp
  )
  set(AUTH_AZURESAS_HDRS ${AUTH_AZURESAS_HDRS}
    gui/qgsauthazuresasedit.h
  )
  set(AUTH_AZURESAS_UIS gui/qgsauthazuresasedit.ui)
  qt_wrap_ui(AUTH_AZURESAS_UIS_H ${AUTH_AZURESAS_UIS})
endif()


# static library
add_library(authmethod_azuresas_a STATIC ${AUTH_AZURESAS_SRCS} ${AUTH_AZURESAS_HDRS} ${AUTH_AZURESAS_UIS_H})

target_include_directories(authmethod_azuresas_a PUBLIC ${CMAKE_SOURCE_DIR}/src/auth/azure_sas/core)

# require c++17
target_compile_features(authmethod_azuresas_a PRIVATE cxx_std_17)

target_link_libraries(authmethod_azuresas_a qgis_core)

if (WITH_GUI)
  target_include_directories(authmethod_azuresas_a PRIVATE
    ${CMAKE_SOURCE_DIR}/src/auth/azure_sas/gui
    ${CMAKE_BINARY_DIR}/src/auth/azure_sas
  )
  target_link_libraries (authmethod_azuresas_a qgis_gui)
endif()

target_compile_definitions(authmethod_azuresas_a PRIVATE "-DQT_NO_FOREACH")

if (FORCE_STATIC_LIBS)
  # for (external) mobile apps to be able to pick up provider for linking
  install (TARGETS authmethod_azuresas_a ARCHIVE DESTINATION ${QGIS_PLUGIN_DIR})
else()
  # dynamically loaded module
  add_library(authmethod_azuresas MODULE ${AUTH_AZURESAS_SRCS} ${AUTH_AZURESAS_HDRS} ${AUTH_AZURESAS_UIS_H})

  # require c++17
  target_compile_features(authmethod_azuresas PRIVATE cxx_std_17)

  target_link_libraries(authmethod_azuresas qgis_core)

  if (WITH_GUI)
    target_include_directories(authmethod_azuresas PRIVATE
      ${CMAKE_SOURCE_DIR}/src/auth/azure_sas/gui
      ${CMAKE_BINARY_DIR}/src/auth/azure_sas
    )
    target_link_libraries (authmethod_azuresas qgis_gui)
    add_dependencies(authmethod_azuresas ui)
  endif()

  target_compile_definitions(authmethod_azuresas PRIVATE "-DQT_NO_FOREACH")

  install (TARGETS authmethod_azuresas
    RUNTIME DESTINATION ${QGIS_PLUGIN_DIR}
    LIBRARY DESTINATION ${QGIS_PLUGIN_DIR}
  )
endif()
