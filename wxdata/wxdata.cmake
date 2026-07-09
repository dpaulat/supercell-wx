cmake_minimum_required(VERSION 3.24)

project(scwx-data)

include(CheckCXXSymbolExists)

find_package(Boost)
find_package(cpr)
find_package(LibXml2)
find_package(libzip)
find_package(OpenSSL)
find_package(range-v3)
find_package(re2)
find_package(spdlog)

check_cxx_symbol_exists(_LIBCPP_VERSION version LIBCPP)

if (LINUX)
    find_package(TBB)
endif()

set(HDR_AWIPS include/scwx/awips/coded_location.hpp
              include/scwx/awips/coded_time_motion_location.hpp
              include/scwx/awips/impact_based_warnings.hpp
              include/scwx/awips/message.hpp
              include/scwx/awips/phenomenon.hpp
              include/scwx/awips/pvtec.hpp
              include/scwx/awips/significance.hpp
              include/scwx/awips/text_product_file.hpp
              include/scwx/awips/text_product_message.hpp
              include/scwx/awips/ugc.hpp
              include/scwx/awips/wmo_header.hpp)
set(SRC_AWIPS source/scwx/awips/coded_location.cpp
              source/scwx/awips/coded_time_motion_location.cpp
              source/scwx/awips/impact_based_warnings.cpp
              source/scwx/awips/message.cpp
              source/scwx/awips/phenomenon.cpp
              source/scwx/awips/pvtec.cpp
              source/scwx/awips/significance.cpp
              source/scwx/awips/text_product_file.cpp
              source/scwx/awips/text_product_message.cpp
              source/scwx/awips/ugc.cpp
              source/scwx/awips/wmo_header.cpp)
set(HDR_COMMON include/scwx/common/application_state.hpp
               include/scwx/common/characters.hpp
               include/scwx/common/color_table.hpp
               include/scwx/common/constants.hpp
               include/scwx/common/geographic.hpp
               include/scwx/common/products.hpp
               include/scwx/common/sites.hpp
               include/scwx/common/types.hpp
               include/scwx/common/vcp.hpp)
set(SRC_COMMON source/scwx/common/application_state.cpp
               source/scwx/common/characters.cpp
               source/scwx/common/color_table.cpp
               source/scwx/common/geographic.cpp
               source/scwx/common/products.cpp
               source/scwx/common/sites.cpp
               source/scwx/common/vcp.cpp)
set(HDR_CONFIG include/scwx/config/ondas_config.hpp
               include/scwx/config/ondas_config_loader.hpp)
set(SRC_CONFIG source/scwx/config/ondas_config.cpp
               source/scwx/config/ondas_config_loader.cpp)
set(HDR_GR include/scwx/gr/color.hpp
           include/scwx/gr/gr_types.hpp
           include/scwx/gr/placefile.hpp)
set(SRC_GR source/scwx/gr/color.cpp
           source/scwx/gr/placefile.cpp)
set(HDR_NETWORK include/scwx/network/cpr.hpp
                include/scwx/network/dir_list.hpp
                include/scwx/network/ntp_client.hpp)
set(SRC_NETWORK source/scwx/network/cpr.cpp
                source/scwx/network/dir_list.cpp
                source/scwx/network/ntp_client.cpp)
set(HDR_PROVIDER include/scwx/provider/aws_level2_data_provider.hpp
                 include/scwx/provider/aws_level2_chunks_data_provider.hpp
                 include/scwx/provider/aws_level3_data_provider.hpp
                 include/scwx/provider/aws_nexrad_data_provider.hpp
                 include/scwx/provider/http_level3_data_provider.hpp
                 include/scwx/provider/http_level3_server_behavior.hpp
                 include/scwx/provider/http_nexrad_data_provider.hpp
                 include/scwx/provider/iem_api_provider.hpp
                 include/scwx/provider/iem_api_provider.ipp
                 include/scwx/provider/nexrad_data_provider.hpp
                 include/scwx/provider/nexrad_data_provider_factory.hpp
                 include/scwx/provider/nws_api_provider.hpp
                 include/scwx/provider/nws_level3_behavior.hpp
                 include/scwx/provider/ondas_level2_data_provider.hpp
                 include/scwx/provider/ondas_level3_behavior.hpp
                 include/scwx/provider/warnings_provider.hpp)
set(SRC_PROVIDER source/scwx/provider/aws_level2_data_provider.cpp
                 source/scwx/provider/aws_level2_chunks_data_provider.cpp
                 source/scwx/provider/aws_level3_data_provider.cpp
                 source/scwx/provider/aws_nexrad_data_provider.cpp
                 source/scwx/provider/http_level3_data_provider.cpp
                 source/scwx/provider/http_nexrad_data_provider.cpp
                 source/scwx/provider/iem_api_provider.cpp
                 source/scwx/provider/nexrad_data_provider.cpp
                 source/scwx/provider/nexrad_data_provider_factory.cpp
                 source/scwx/provider/nws_api_provider.cpp
                 source/scwx/provider/nws_level3_behavior.cpp
                 source/scwx/provider/ondas_level2_data_provider.cpp
                 source/scwx/provider/ondas_level3_behavior.cpp
                 source/scwx/provider/warnings_provider.cpp)
set(HDR_TYPES include/scwx/types/iem_types.hpp
              include/scwx/types/ntp_types.hpp
              include/scwx/types/nws_types.hpp
              include/scwx/types/ondas_types.hpp)
set(SRC_TYPES source/scwx/types/iem_types.cpp
              source/scwx/types/ntp_types.cpp
              source/scwx/types/nws_types.cpp
              source/scwx/types/ondas_types.cpp)
set(HDR_UTIL include/scwx/util/digest.hpp
             include/scwx/util/enum.hpp
             include/scwx/util/environment.hpp
             include/scwx/util/float.hpp
             include/scwx/util/hash.hpp
             include/scwx/util/iterator.hpp
             include/scwx/util/json.hpp
             include/scwx/util/logger.hpp
             include/scwx/util/map.hpp
             include/scwx/util/rangebuf.hpp
             include/scwx/util/streams.hpp
             include/scwx/util/strings.hpp
             include/scwx/util/threads.hpp
             include/scwx/util/time.hpp
             include/scwx/util/vectorbuf.hpp)
set(SRC_UTIL source/scwx/util/digest.cpp
             source/scwx/util/environment.cpp
             source/scwx/util/float.cpp
             source/scwx/util/hash.cpp
             source/scwx/util/json.cpp
             source/scwx/util/logger.cpp
             source/scwx/util/rangebuf.cpp
             source/scwx/util/streams.cpp
             source/scwx/util/strings.cpp
             source/scwx/util/time.cpp
             source/scwx/util/threads.cpp
             source/scwx/util/vectorbuf.cpp)
set(HDR_WSR88D include/scwx/wsr88d/ar2v_file.hpp
               include/scwx/wsr88d/level3_file.hpp
               include/scwx/wsr88d/nexrad_file.hpp
               include/scwx/wsr88d/nexrad_file_factory.hpp
               include/scwx/wsr88d/wsr88d_types.hpp)
set(SRC_WSR88D source/scwx/wsr88d/ar2v_file.cpp
               source/scwx/wsr88d/level3_file.cpp
               source/scwx/wsr88d/nexrad_file.cpp
               source/scwx/wsr88d/nexrad_file_factory.cpp
               source/scwx/wsr88d/wsr88d_types.cpp)
set(HDR_WSR88D_RDA include/scwx/wsr88d/rda/clutter_filter_bypass_map.hpp
                   include/scwx/wsr88d/rda/clutter_filter_map.hpp
                   include/scwx/wsr88d/rda/digital_radar_data.hpp
                   include/scwx/wsr88d/rda/digital_radar_data_generic.hpp
                   include/scwx/wsr88d/rda/generic_radar_data.hpp
                   include/scwx/wsr88d/rda/level2_message.hpp
                   include/scwx/wsr88d/rda/level2_message_factory.hpp
                   include/scwx/wsr88d/rda/level2_message_header.hpp
                   include/scwx/wsr88d/rda/performance_maintenance_data.hpp
                   include/scwx/wsr88d/rda/rda_adaptation_data.hpp
                   include/scwx/wsr88d/rda/rda_prf_data.hpp
                   include/scwx/wsr88d/rda/rda_status_data.hpp
                   include/scwx/wsr88d/rda/rda_types.hpp
                   include/scwx/wsr88d/rda/volume_coverage_pattern_data.hpp)
set(SRC_WSR88D_RDA source/scwx/wsr88d/rda/clutter_filter_bypass_map.cpp
                   source/scwx/wsr88d/rda/clutter_filter_map.cpp
                   source/scwx/wsr88d/rda/digital_radar_data.cpp
                   source/scwx/wsr88d/rda/digital_radar_data_generic.cpp
                   source/scwx/wsr88d/rda/generic_radar_data.cpp
                   source/scwx/wsr88d/rda/level2_message.cpp
                   source/scwx/wsr88d/rda/level2_message_factory.cpp
                   source/scwx/wsr88d/rda/level2_message_header.cpp
                   source/scwx/wsr88d/rda/performance_maintenance_data.cpp
                   source/scwx/wsr88d/rda/rda_adaptation_data.cpp
                   source/scwx/wsr88d/rda/rda_prf_data.cpp
                   source/scwx/wsr88d/rda/rda_status_data.cpp
                   source/scwx/wsr88d/rda/volume_coverage_pattern_data.cpp)
set(HDR_WSR88D_RPG include/scwx/wsr88d/rpg/ccb_header.hpp
                   include/scwx/wsr88d/rpg/cell_trend_data_packet.hpp
                   include/scwx/wsr88d/rpg/cell_trend_volume_scan_times.hpp
                   include/scwx/wsr88d/rpg/digital_precipitation_data_array_packet.hpp
                   include/scwx/wsr88d/rpg/digital_radial_data_array_packet.hpp
                   include/scwx/wsr88d/rpg/digital_raster_data_array_packet.hpp
                   include/scwx/wsr88d/rpg/general_status_message.hpp
                   include/scwx/wsr88d/rpg/generic_data_packet.hpp
                   include/scwx/wsr88d/rpg/generic_radial_data_packet.hpp
                   include/scwx/wsr88d/rpg/graphic_alphanumeric_block.hpp
                   include/scwx/wsr88d/rpg/graphic_product_message.hpp
                   include/scwx/wsr88d/rpg/hda_hail_symbol_packet.hpp
                   include/scwx/wsr88d/rpg/level3_message.hpp
                   include/scwx/wsr88d/rpg/level3_message_factory.hpp
                   include/scwx/wsr88d/rpg/level3_message_header.hpp
                   include/scwx/wsr88d/rpg/linked_contour_vector_packet.hpp
                   include/scwx/wsr88d/rpg/linked_vector_packet.hpp
                   include/scwx/wsr88d/rpg/mesocyclone_symbol_packet.hpp
                   include/scwx/wsr88d/rpg/packet.hpp
                   include/scwx/wsr88d/rpg/packet_factory.hpp
                   include/scwx/wsr88d/rpg/point_feature_symbol_packet.hpp
                   include/scwx/wsr88d/rpg/point_graphic_symbol_packet.hpp
                   include/scwx/wsr88d/rpg/precipitation_rate_data_array_packet.hpp
                   include/scwx/wsr88d/rpg/product_description_block.hpp
                   include/scwx/wsr88d/rpg/product_symbology_block.hpp
                   include/scwx/wsr88d/rpg/radar_coded_message.hpp
                   include/scwx/wsr88d/rpg/radial_data_packet.hpp
                   include/scwx/wsr88d/rpg/raster_data_packet.hpp
                   include/scwx/wsr88d/rpg/rpg_types.hpp
                   include/scwx/wsr88d/rpg/scit_data_packet.hpp
                   include/scwx/wsr88d/rpg/set_color_level_packet.hpp
                   include/scwx/wsr88d/rpg/special_graphic_symbol_packet.hpp
                   include/scwx/wsr88d/rpg/sti_circle_symbol_packet.hpp
                   include/scwx/wsr88d/rpg/storm_id_symbol_packet.hpp
                   include/scwx/wsr88d/rpg/storm_tracking_information_message.hpp
                   include/scwx/wsr88d/rpg/tabular_alphanumeric_block.hpp
                   include/scwx/wsr88d/rpg/tabular_product_message.hpp
                   include/scwx/wsr88d/rpg/text_and_special_symbol_packet.hpp
                   include/scwx/wsr88d/rpg/unlinked_contour_vector_packet.hpp
                   include/scwx/wsr88d/rpg/unlinked_vector_packet.hpp
                   include/scwx/wsr88d/rpg/vector_arrow_data_packet.hpp
                   include/scwx/wsr88d/rpg/wind_barb_data_packet.hpp)
set(SRC_WSR88D_RPG source/scwx/wsr88d/rpg/ccb_header.cpp
                   source/scwx/wsr88d/rpg/cell_trend_data_packet.cpp
                   source/scwx/wsr88d/rpg/cell_trend_volume_scan_times.cpp
                   source/scwx/wsr88d/rpg/digital_precipitation_data_array_packet.cpp
                   source/scwx/wsr88d/rpg/digital_radial_data_array_packet.cpp
                   source/scwx/wsr88d/rpg/digital_raster_data_array_packet.cpp
                   source/scwx/wsr88d/rpg/general_status_message.cpp
                   source/scwx/wsr88d/rpg/generic_data_packet.cpp
                   source/scwx/wsr88d/rpg/generic_radial_data_packet.cpp
                   source/scwx/wsr88d/rpg/graphic_alphanumeric_block.cpp
                   source/scwx/wsr88d/rpg/graphic_product_message.cpp
                   source/scwx/wsr88d/rpg/hda_hail_symbol_packet.cpp
                   source/scwx/wsr88d/rpg/level3_message.cpp
                   source/scwx/wsr88d/rpg/level3_message_factory.cpp
                   source/scwx/wsr88d/rpg/level3_message_header.cpp
                   source/scwx/wsr88d/rpg/linked_contour_vector_packet.cpp
                   source/scwx/wsr88d/rpg/linked_vector_packet.cpp
                   source/scwx/wsr88d/rpg/mesocyclone_symbol_packet.cpp
                   source/scwx/wsr88d/rpg/packet.cpp
                   source/scwx/wsr88d/rpg/packet_factory.cpp
                   source/scwx/wsr88d/rpg/point_feature_symbol_packet.cpp
                   source/scwx/wsr88d/rpg/point_graphic_symbol_packet.cpp
                   source/scwx/wsr88d/rpg/precipitation_rate_data_array_packet.cpp
                   source/scwx/wsr88d/rpg/product_description_block.cpp
                   source/scwx/wsr88d/rpg/product_symbology_block.cpp
                   source/scwx/wsr88d/rpg/radar_coded_message.cpp
                   source/scwx/wsr88d/rpg/radial_data_packet.cpp
                   source/scwx/wsr88d/rpg/raster_data_packet.cpp
                   source/scwx/wsr88d/rpg/scit_data_packet.cpp
                   source/scwx/wsr88d/rpg/set_color_level_packet.cpp
                   source/scwx/wsr88d/rpg/special_graphic_symbol_packet.cpp
                   source/scwx/wsr88d/rpg/sti_circle_symbol_packet.cpp
                   source/scwx/wsr88d/rpg/storm_id_symbol_packet.cpp
                   source/scwx/wsr88d/rpg/storm_tracking_information_message.cpp
                   source/scwx/wsr88d/rpg/tabular_alphanumeric_block.cpp
                   source/scwx/wsr88d/rpg/tabular_product_message.cpp
                   source/scwx/wsr88d/rpg/text_and_special_symbol_packet.cpp
                   source/scwx/wsr88d/rpg/unlinked_contour_vector_packet.cpp
                   source/scwx/wsr88d/rpg/unlinked_vector_packet.cpp
                   source/scwx/wsr88d/rpg/vector_arrow_data_packet.cpp
                   source/scwx/wsr88d/rpg/wind_barb_data_packet.cpp)
set(HDR_ZIP include/scwx/zip/zip_stream_reader.hpp
            include/scwx/zip/zip_stream_writer.hpp)
set(SRC_ZIP source/scwx/zip/zip_stream_reader.cpp
            source/scwx/zip/zip_stream_writer.cpp)

set(CMAKE_FILES wxdata.cmake)

add_library(wxdata OBJECT ${HDR_AWIPS}
                          ${SRC_AWIPS}
                          ${HDR_COMMON}
                          ${SRC_COMMON}
                          ${HDR_CONFIG}
                          ${SRC_CONFIG}
                          ${HDR_GR}
                          ${SRC_GR}
                          ${HDR_NETWORK}
                          ${SRC_NETWORK}
                          ${HDR_PROVIDER}
                          ${SRC_PROVIDER}
                          ${HDR_TYPES}
                          ${SRC_TYPES}
                          ${HDR_UTIL}
                          ${SRC_UTIL}
                          ${HDR_WSR88D}
                          ${SRC_WSR88D}
                          ${HDR_WSR88D_RDA}
                          ${SRC_WSR88D_RDA}
                          ${HDR_WSR88D_RPG}
                          ${SRC_WSR88D_RPG}
                          ${HDR_ZIP}
                          ${SRC_ZIP}
                          ${CMAKE_FILES})

source_group("Header Files\\awips"       FILES ${HDR_AWIPS})
source_group("Source Files\\awips"       FILES ${SRC_AWIPS})
source_group("Header Files\\common"      FILES ${HDR_COMMON})
source_group("Source Files\\common"      FILES ${SRC_COMMON})
source_group("Header Files\\config"      FILES ${HDR_CONFIG})
source_group("Source Files\\config"      FILES ${SRC_CONFIG})
source_group("Header Files\\gr"          FILES ${HDR_GR})
source_group("Source Files\\gr"          FILES ${SRC_GR})
source_group("Header Files\\network"     FILES ${HDR_NETWORK})
source_group("Source Files\\network"     FILES ${SRC_NETWORK})
source_group("Header Files\\provider"    FILES ${HDR_PROVIDER})
source_group("Source Files\\provider"    FILES ${SRC_PROVIDER})
source_group("Header Files\\types"       FILES ${HDR_TYPES})
source_group("Source Files\\types"       FILES ${SRC_TYPES})
source_group("Header Files\\util"        FILES ${HDR_UTIL})
source_group("Source Files\\util"        FILES ${SRC_UTIL})
source_group("Header Files\\wsr88d"      FILES ${HDR_WSR88D})
source_group("Source Files\\wsr88d"      FILES ${SRC_WSR88D})
source_group("Header Files\\wsr88d\\rda" FILES ${HDR_WSR88D_RDA})
source_group("Source Files\\wsr88d\\rda" FILES ${SRC_WSR88D_RDA})
source_group("Header Files\\wsr88d\\rpg" FILES ${HDR_WSR88D_RPG})
source_group("Source Files\\wsr88d\\rpg" FILES ${SRC_WSR88D_RPG})
source_group("Header Files\\zip"         FILES ${HDR_ZIP})
source_group("Source Files\\zip"         FILES ${SRC_ZIP})


try_compile(CHRONO_HAS_TIMEZONES_AND_CALENDERS
            ${CMAKE_BINARY_DIR}
            ${PROJECT_SOURCE_DIR}/cpp-feature-tests/chrono_feature_test.cpp
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED ON
            CXX_EXTENSIONS OFF)
message("CHRONO_HAS_TIMEZONES_AND_CALENDERS: ${CHRONO_HAS_TIMEZONES_AND_CALENDERS}")

target_include_directories(wxdata PRIVATE ${Boost_INCLUDE_DIR}
                                          ${HSLUV_C_INCLUDE_DIR}
                                          ${scwx-data_SOURCE_DIR}/include
                                          ${scwx-data_SOURCE_DIR}/source)
target_include_directories(wxdata INTERFACE ${scwx-data_SOURCE_DIR}/include)

target_compile_options(wxdata PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>
)

# GCC 14+ produces false positives for -Wmaybe-uninitialized
target_compile_options(wxdata PRIVATE
    $<$<AND:$<CXX_COMPILER_ID:GNU>,$<VERSION_GREATER_EQUAL:$<CXX_COMPILER_VERSION>,14>>:-Wno-maybe-uninitialized>
)

# Temporary workaround for Boost and GCC 15+ where -Warray-bounds causes false positives
target_compile_options(wxdata PRIVATE
    $<$<AND:$<CXX_COMPILER_ID:GNU>,$<VERSION_GREATER_EQUAL:$<CXX_COMPILER_VERSION>,15>>:-Wno-array-bounds>
)

# Temporary workaround for Boost and GCC 15+ where -Wstringop-overflow causes false positives
target_compile_options(wxdata PRIVATE
    $<$<AND:$<CXX_COMPILER_ID:GNU>,$<VERSION_GREATER_EQUAL:$<CXX_COMPILER_VERSION>,15>>:-Wno-stringop-overflow>
)

if (MSVC)
    # Don't include Windows macros
    target_compile_options(wxdata PRIVATE -DNOMINMAX)

    # Enable multi-processor compilation
    target_compile_options(wxdata PRIVATE "/MP")
endif()

if (MSVC)
    # Produce PDB file for debug
    target_compile_options(wxdata PRIVATE "$<$<CONFIG:Release>:/Zi>")
else()
    target_compile_options(wxdata PRIVATE "$<$<CONFIG:Release>:-g>")
endif()

target_link_libraries(wxdata PUBLIC aws-cpp-sdk-core
                                    aws-cpp-sdk-s3
                                    cpr::cpr
                                    LibXml2::LibXml2
                                    libzip::zip
                                    OpenSSL::Crypto
                                    range-v3::range-v3
                                    re2::re2
                                    spdlog::spdlog
                                    units::units)
target_link_libraries(wxdata INTERFACE Boost::iostreams
                                       Boost::url
                                       BZip2::BZip2
                                       hsluv-c)

if (WIN32)
    target_link_libraries(wxdata INTERFACE Ws2_32)
endif()

if (NOT CHRONO_HAS_TIMEZONES_AND_CALENDERS)
    target_link_libraries(wxdata PUBLIC date::date-tz)
endif()

if (LINUX)
    target_link_libraries(wxdata PUBLIC TBB::tbb)
endif()

if (LIBCPP)
    # Enable support for parallel algorithms
    target_compile_options(wxdata PUBLIC -fexperimental-library)
    target_link_libraries(wxdata INTERFACE c++experimental)
endif()

set_target_properties(wxdata PROPERTIES CXX_STANDARD 20
                                        CXX_STANDARD_REQUIRED ON
                                        CXX_EXTENSIONS OFF)

# Address Sanitizer options
if (SCWX_ADDRESS_SANITIZER)
    target_compile_options(wxdata PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/fsanitize=address /EHsc /D_DISABLE_STRING_ANNOTATION /D_DISABLE_VECTOR_ANNOTATION>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-fsanitize=address -fsanitize-recover=address>
    )
endif()
