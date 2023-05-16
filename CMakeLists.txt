cmake_minimum_required(VERSION 3.1)
project(YogaTeacher)


set(ICUB_CONTRIB_DIRS $ENV{ICUB_DIR}/include)
find_package(YARP REQUIRED COMPONENTS sig cv)
FIND_PACKAGE(ICUBcontrib REQUIRED)
FIND_PACKAGE(OpenCV REQUIRED)


list(APPEND CMAKE_MODULE_PATH ${YARP_MODULE_PATH})
list(APPEND CMAKE_MODULE_PATH ${ICUB_MODULE_PATH})
list(APPEND CMAKE_MODULE_PATH ${ICUBCONTRIB_MODULE_PATH})
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(YarpInstallationHelpers)
include(ICUBcontribHelpers)
include(ICUBcontribOptions)

icubcontrib_set_default_prefix()
set(ICUB_APPLICATIONS_PREFIX "$ENV{ICUB_ROOT}" CACHE PATH "Application path prefix")


add_subdirectory("yogaSupervisor")
icubcontrib_add_uninstall_target()