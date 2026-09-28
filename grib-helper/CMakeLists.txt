# Builds decode_grib, the out-of-process eccodes helper GribManager and
# WindBarbManager invoke via QProcess (see grib-helper/README.md for why
# it isn't linked directly into wxdata). eccodes isn't on conancenter, so
# unlike the rest of the project's dependencies this one comes from the
# system package manager -- optional: if it isn't installed, this target
# is skipped and the GRIB/wind-barb overlays are unavailable at runtime
# (GribManager/WindBarbManager already log and no-op when decode_grib
# can't be started) rather than failing the whole configure.
find_package(PkgConfig)

if (PkgConfig_FOUND)
    pkg_check_modules(ECCODES IMPORTED_TARGET eccodes)
endif()

if (TARGET PkgConfig::ECCODES)
    add_executable(decode_grib src/decode_grib.cpp)
    target_link_libraries(decode_grib PRIVATE PkgConfig::ECCODES)
    set_target_properties(decode_grib PROPERTIES
        FOLDER               grib-helper
        CXX_STANDARD          20
        CXX_STANDARD_REQUIRED ON)

    # Debug tool: reads a .frame file back and prints its stats, with no
    # dependency on eccodes or a JSON library, so a frame's contents can
    # be sanity-checked independently of decode_grib itself.
    add_executable(read_frame src/read_frame.cpp)
    set_target_properties(read_frame PROPERTIES
        FOLDER               grib-helper
        CXX_STANDARD          20
        CXX_STANDARD_REQUIRED ON)
else()
    message(STATUS
        "eccodes not found: decode_grib will not be built, GRIB/wind-barb "
        "overlays will be unavailable at runtime "
        "(see grib-helper/README.md)")
endif()
