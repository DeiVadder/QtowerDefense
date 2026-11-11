# Writes the page of the browser build after linking (E-51): src/app/web/index.html.in with the size
# of the .wasm, so the loading bar shows real progress, the version of Qt for its credit and the
# place of the source.
#   cmake -DTEMPLATE=... -DWASM=... -DOUTPUT=... -DQT_VERSION=... -DSOURCE_URL=... -P web_page.cmake
foreach(variable TEMPLATE WASM OUTPUT QT_VERSION SOURCE_URL)
    if(NOT DEFINED ${variable})
        message(FATAL_ERROR "web_page.cmake needs -D${variable}=...")
    endif()
endforeach()

file(SIZE "${WASM}" QTD_WASM_BYTES)
set(QTD_QT_VERSION "${QT_VERSION}")
set(QTD_SOURCE_URL "${SOURCE_URL}")
string(REGEX MATCH "^[0-9]+\\.[0-9]+" QTD_QT_MINOR "${QT_VERSION}")
configure_file("${TEMPLATE}" "${OUTPUT}" @ONLY)
