# Apply only to the exact pinned VSTGUI revision. No Windows changes.
# Upstream issues #376/#377: Cairo caches a device beyond reference drops.
# Own a reference for the XCB connection; finish only after LAST frame closes,
# before disconnecting. Finishing per-frame could invalidate another live UI.
execute_process(COMMAND git -C "${VSTGUI_ROOT}" rev-parse HEAD
  OUTPUT_VARIABLE pinned OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
if(NOT pinned STREQUAL "5db272256172557818b6158cf0bb2c4410bddb25")
  message(FATAL_ERROR "Unexpected VSTGUI revision for Linux XCB lifetime patch")
endif()
function(replace_exact name before after)
  set(path "${VSTGUI_ROOT}/vstgui/lib/platform/linux/${name}")
  file(READ "${path}" source)
  string(FIND "${source}" "${after}" already)
  if(NOT already EQUAL -1)
    return()
  endif()
  string(FIND "${source}" "${before}" offset)
  if(offset EQUAL -1)
    message(FATAL_ERROR "Missing expected upstream anchor: ${name}")
  endif()
  string(REPLACE "${before}" "${after}" result "${source}")
  file(WRITE "${path}" "${result}")
endfunction()
replace_exact(x11platform.cpp
  "xcb_connection_t* xcbConnection {nullptr};"
  "xcb_connection_t* xcbConnection {nullptr};\n\tcairo_device_t* connectionDevice {nullptr};")
replace_exact(x11platform.cpp
  "			xcb_disconnect (xcbConnection);"
  "			// Cairo caches XCB devices; finish before the last connection closes.\n			if (connectionDevice)\n			{\n				cairo_device_finish (connectionDevice);\n				cairo_device_destroy (connectionDevice);\n				connectionDevice = nullptr;\n			}\n			xcb_disconnect (xcbConnection);\n			xcbConnection = nullptr;\n			xkbUnprocessedState = nullptr;\n			xkbState = nullptr;\n			xkbKeymap = nullptr;\n			xkbContext = nullptr;\n			cursorContext = nullptr;\n			cursors.fill (XCB_CURSOR_NONE);")
replace_exact(x11platform.cpp
  "xcb_connection_t* RunLoop::getXcbConnection () const"
  "void RunLoop::setDevice (cairo_device_t* device)\n{\n\tif (device && !impl->connectionDevice)\n\t\timpl->connectionDevice = cairo_device_reference (device);\n\tvstgui_assert (!device || impl->connectionDevice == device);\n}\n\nxcb_connection_t* RunLoop::getXcbConnection () const")
replace_exact(x11frame.cpp
  "windowSurface.assign (s);"
  "windowSurface.assign (s);\n\t\tRunLoop::instance ().setDevice (cairo_surface_get_device (s));")
replace_exact(x11frame.cpp
  "~DrawHandler ()\n\t{\n\t\tgetPlatformFactory ().asLinuxFactory ()->getCairoGraphicsDeviceFactory ().removeDevice (\n\t\t\tcairo_surface_get_device (windowSurface));\n\t}"
  "~DrawHandler ()\n\t{\n\t\tauto* nativeDevice = cairo_surface_get_device (windowSurface);\n\t\tdrawContext.reset ();\n\t\tbackBuffer.reset ();\n\t\twindowSurface.reset ();\n\t\tgetPlatformFactory ().asLinuxFactory ()->getCairoGraphicsDeviceFactory ().removeDevice (nativeDevice);\n\t}")
replace_exact(x11utils.cpp
  "ChildWindow::~ChildWindow () noexcept {}"
  "ChildWindow::~ChildWindow () noexcept\n{\n\tauto connection = RunLoop::instance ().getXcbConnection ();\n\txcb_destroy_window (connection, id);\n\txcb_flush (connection);\n}")
message(STATUS "Applied pinned VSTGUI Linux XCB connection-lifetime patch")
