#!/bin/bash
#######################################################################################
# FLATWOKEN ICON THEME CONFIGURATION SCRIPT
# Copyright: (C) 2014 FlatWoken icons7 Robotics Brain and Cognitive Sciences
# Author:  Jonas Gonzalez
# email:  jonas.gonzalez@iit.it
# Permission is granted to copy, distribute, and/or modify this program
# under the terms of the GNU General Public License, version 2 or any
# later version published by the Free Software Foundation.
#  *
# A copy of the license can be found at
# http://www.robotcub.org/icub/license/gpl.txt
#  *
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
# Public License for more details
#######################################################################################


#######################################################################################
# USEFUL FUNCTIONS:                                                                  #
#######################################################################################
usage() {
cat << EOF
***************************************************************************************
DEA SCRIPTING
Author:  Alessandro Roncone   <alessandro.roncone@iit.it> 

This script scripts through the commands available for the DeA Kids videos.

USAGE:
        $0 options

***************************************************************************************
OPTIONS:

***************************************************************************************
EXAMPLE USAGE:

***************************************************************************************
EOF
}

#######################################################################################
# FUNCTIONS:                                                                         #
#######################################################################################

goHome(){
	goHomeArms 2.0
	echo "ctpq time 2.0 off 0 pos (0 0 0)" | yarp rpc /ctpservice/torso/rpc

}


goHomeHead(){

	echo "abs 0 0 0" | yarp write ... /iKinGazeCtrl/angles:i
	goHomeArms 2.0
}

goHomeArms(){

	echo "ctpq time $1 off 0 pos (-4.65 9.77 -7.38 23.68 0.02 -0.10 -0.18 14.62 29.86 56.0 0.0 0.0 0.0 0.0 -0.36 0.46)" | yarp rpc /ctpservice/left_arm/rpc
	echo "ctpq time $1 off 0 pos (-4.65 9.77 -7.38 23.68 0.02 -0.10 -0.18 14.62 29.86 56.0 0.0 0.0 0.0 0.0 -0.36 0.46)" | yarp rpc /ctpservice/right_arm/rpc

}

pointRightPose(){

	echo "ctpq time 2.0 off 0 pos (-22.28 46.99 -10.78 24.02 -59.60 7.50 -13.88 37.09 10.39 55.14 0.0 0.0 0.37 0.0 0.0 2.34)" | yarp rpc /ctpservice/right_arm/rpc
	echo "ctpq time 2.0 off 0 pos (15 0 0)" | yarp rpc /ctpservice/torso/rpc
}

pointCenterPose(){

	echo "ctpq time 2.0 off 0 pos (-65.83 18.12 -7.82 14.98 -54.01 -0.37 1.04 41.40 60.48 56.0 1.87 0.0 0.0 0.0 0.37 0.46)" | yarp rpc /ctpservice/right_arm/rpc
}

pointLeftPose(){

	echo "ctpq time 2.0 off 0 pos (-5.36 52.8 -31.3 -31.64 23.68 -60.018 0.065 0.082 14.36 29.86 56.04 0.0 0.0 0.0 0.0 0.0 0.46)" | yarp rpc /ctpservice/left_arm/rpc
	echo "ctpq time 2.0 off 0 pos (-13 0 0)" | yarp rpc /ctpservice/torso/rpc
}

saluta() {

    echo "ctpq time 1.5 off 0 pos (-60.0 44.0 -2.0 96.0 53.0 -17.0 -11.0 0.0 0.0 0.0 0.0 0.0 0.0 0.0 0.0 0.0)" | yarp rpc /ctpservice/right_arm/rpc
    sleep 1.0
    echo "ctpq time 0.3 off 0 pos (-60.0 44.0 -2.0 96.0 53.0 -17.0  25.0 0.0 0.0 0.0 0.0 0.0 0.0 0.0 0.0 0.0)" | yarp rpc /ctpservice/right_arm/rpc
    sleep 0.5
    echo "ctpq time 0.3 off 0 pos (-60.0 44.0 -2.0 96.0 53.0 -17.0 -11.0 0.0 0.0 0.0 0.0 0.0 0.0 0.0 0.0 0.0)" | yarp rpc /ctpservice/right_arm/rpc
    sleep 0.5
    echo "ctpq time 0.3 off 0 pos (-60.0 44.0 -2.0 96.0 53.0 -17.0  25.0 0.0 0.0 0.0 0.0 0.0 0.0 0.0 0.0 0.0)" | yarp rpc /ctpservice/right_arm/rpc
    sleep 0.5
    echo "ctpq time 0.3 off 0 pos (-60.0 44.0 -2.0 96.0 53.0 -17.0 -11.0 0.0 0.0 0.0 0.0 0.0 0.0 0.0 0.0 0.0)" | yarp rpc /ctpservice/right_arm/rpc

    sleep 1.0

    go_home
}



#######################################################################################
# "MAIN" FUNCTION:                                                                    #
#######################################################################################
echo "********************************************************************************"
echo ""

$1 "$2"

if [[ $# -eq 0 ]] ; then 
    echo "No options were passed!"
    echo ""
    usage
    exit 1
fi


