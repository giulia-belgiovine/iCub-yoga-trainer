# Copyright: (C) 2020 RBCS Robotics Brain and Cognitive Science
# Authors: Giulia Belgiovine, Jonas Gonzalez
# CopyPolicy: Released under the terms of the GNU GPL v2.0.

SET(KEYWORD "yogaSupervisor")
PROJECT(${KEYWORD})

INCLUDE_DIRECTORIES(
        ${PROJECT_SOURCE_DIR}/include
        ${ICUB_INCLUDE_DIRS}
        ${ICUB_CONTRIB_DIRS}
        ${OpenCV_INCLUDE_DIRS}
        ${Yarp_OpenCV_INCLUDE_DIR}
)



# Search for source code.
FILE(GLOB folder_source src/*.cpp src/*.cc src/*.c)
FILE(GLOB folder_header include/*.h)
SOURCE_GROUP("Source Files" FILES ${folder_source})
SOURCE_GROUP("Header Files" FILES ${folder_header})

# Set up the main executable.
IF (folder_source)
    ADD_EXECUTABLE(${KEYWORD}
            ${folder_source}
            ${folder_header})

    TARGET_LINK_LIBRARIES(${KEYWORD}
            ${OpenCV_LIBS}
            ${YARP_LIBRARIES}
            )

    INSTALL_TARGETS(/bin ${KEYWORD})
ELSE (folder_source)
    MESSAGE(FATAL_ERROR "No source code files found. Please add something")
ENDIF (folder_source)


add_subdirectory("app")
