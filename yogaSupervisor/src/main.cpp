#include "yogaSupervisor.h"
#include <yarp/os/Log.h>

using namespace yarp::os;

int main(int argc, char *argv[]) {

    Network yarp;
    if (yarp.checkNetwork()) {

        YogaSupervisor module;

        ResourceFinder rf;
        rf.setVerbose(true);
        rf.setDefaultConfigFile("yogaSupervisor.ini");      //overridden by --from parameter
        rf.setDefaultContext("yogaSupervisor");              //overridden by --context parameter
        rf.configure(argc, argv);

        yInfo("resourceFinder: %s", rf.toString().c_str());

        module.runModule(rf);
    }

}
