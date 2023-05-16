/*
  * Copyright (C)2017  Department of Robotics Brain and Cognitive Sciences - Istituto Italiano di Tecnologia
  * Author: Giulia Belgiovine, Jonas gonzalez
  * email: giulia.belgiovine@iit.it
  * Permission is granted to copy, distribute, and/or modify this program
  * under the terms of the GNU General Public License, version 2 or any
  * later version published by the Free Software Foundation.
  *
  * A copy of the license can be found at
  * http://www.robotcub.org/icub/license/gpl.txt
  *
  * This program is distributed in the hope that it will be useful, but
  * WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
  * Public License for more details
*/
//

#include <vector>
#include <algorithm>
#include <fstream>
#include <string>
#include <yarp/os/Log.h>
#include <yarp/sig/Image.h>

#include "yogaSupervisor.h"

using namespace yarp::os;
using namespace yarp::sig;

bool YogaSupervisor::configure(yarp::os::ResourceFinder &rf) {


    if (rf.check("help")) {
        printf("HELP \n");
        printf("====== \n");
        printf("--name           : changes the rootname of the module ports \n");
        printf("--robot          : changes the name of the robot where the module interfaces to  \n");
        printf("--name           : rootname for all the connection of the module \n");
        printf(" \n");
        printf("press CTRL-C to stop... \n");
        return true;
    }

    moduleName = rf.check("name",
                          Value("/YogaSupervisor"),
                          "module name (string)").asString();

    setName(moduleName.c_str());

    robotName = rf.check("robot",
                         Value("icub"),
                         "Robot name (string)").asString();

    handlerPortName = "";
    handlerPortName += getName();

    yInfo("Loading Configurations");
    bool ret = openPorts();

    current_state = IDLE;

    loadSpeech(rf);

    /******** From ini file ********/

    yarp::os::Bottle *posePlayersBottle = rf.find("poseCommands").asList();
    bottleToVector(posePlayersBottle, &spatialPositionVector);

    yarp::os::Bottle *poseWordsBottle = rf.find("poseWords").asList();
    bottleToVector(poseWordsBottle, &poseWordsVector);

    yarp::os::Bottle *partLabelBottle = rf.find("participantsLabel").asList();
    bottleToVector(partLabelBottle, &partLabelVector);

    level = rf.find("level").asInt();
    yInfo("level is %i ",level);
    pose_index = rf.find("pose_index").asInt();
    yInfo("pose_index is %i ", pose_index);

    //bottle->get(i).asString()
    //bottleToVector(levelBottle, &partLabelVector);

    if (!openIkinGazeCtrl()) {
        yError("Unable to open iKinGazeCtrl");
        return false;
    }

    /******** For Working Memory and MOT ********/
    previousImagePosX = previousImagePosY = -1;
    previous_box_coordinate = {-1, -1, 0, 0};
    distance_tracking_X = rf.check("tracking_distanceX", Value(8), "In pixel ").asInt();
    distance_tracking_Y = rf.check("tracking_distanceY", Value(25), "In pixel").asInt();

    yInfo("Initialization done");

    return ret;       // let the RFModule know everything went well so that it will then run the module
}


bool YogaSupervisor::close() {
    handlerPort.close();
    clientRPCEmotion.close();
    clientRpcAudioRecorder.close();
    eventsPort.close();
    speechOutputPort.close();
    clientRpcSoundLocaliser.close();
    clientRPCSpeechRecognition.close();
    clientRPCInteractionInterface.close();
    personRecognitionPort.close();
    speechRecognitionPort.close();
    clientRPCYogaTrainer.close();
    inputFaceCoordinatePort.close();
    markerSpeechPort.close();
    inputMotFacePort.close();
    clientRpcMOT.close();
    clientRpcObjectProprertiesCollector.close();

    yInfo("Stopping the thread");
    return true;
}


bool YogaSupervisor::interruptModule() {
    yInfo("Interrupting the thread \n");
    iGaze->restoreContext(gaze_context);
    clientGaze->close();


    handlerPort.interrupt();
    clientRPCEmotion.interrupt();
    clientRpcAudioRecorder.interrupt();
    eventsPort.interrupt();
    speechOutputPort.interrupt();
    clientRpcSoundLocaliser.interrupt();
    clientRPCSpeechRecognition.interrupt();
    clientRPCInteractionInterface.interrupt();
    personRecognitionPort.interrupt();
    speechRecognitionPort.interrupt();
    clientRPCYogaTrainer.interrupt();
    inputFaceCoordinatePort.interrupt();
    markerSpeechPort.interrupt();
    inputMotFacePort.interrupt();
    clientRpcMOT.interrupt();
    clientRpcObjectProprertiesCollector.interrupt();

    return true;
}


double YogaSupervisor::getPeriod() {
    /* module periodicity (seconds), called implicitly by myModule */
    return 0.08;
}


bool YogaSupervisor::respond(const yarp::os::Bottle &command, yarp::os::Bottle &reply) {

    std::vector<std::string> replyScript;
    std::string helpMessage = std::string(getName()) +
                              " commands are: \n" +
                              "help \n" +
                              "quit \n";
    reply.clear();

    if (command.get(0).asString() == "quit") {
        reply.addString("quitting");
        return false;
    }

    bool ok = false;
    bool rec = false;

    mutex.wait();

    switch (command.get(0).asVocab()) {
        case COMMAND_VOCAB_HELP:
            rec = true;
            {
                reply.addVocab(Vocab::encode("many"));
                reply.addString("Command available : ");
                reply.addString("To get or set the current state : set/get state <value>");

                ok = true;
            }
            break;
        case COMMAND_VOCAB_SET:
            rec = true;
            {
                switch (command.get(1).asVocab()) {

                    case COMMAND_VOCAB_STATE: {
                        ok = true;
                        const int new_state = command.get(2).asInt();
                        if (new_state == INIT_EXP){
                            yarp::os::Bottle cmd;
                            cmd.clear();
                            cmd.addString("exe");
                            cmd.addString("go_home");
                            sendRpcCommand(clientRPCInteractionInterface, cmd);
                        }
                        current_state = new_state;
                        break;
                    }

                    default:
                        ok = true;
                        yInfo("received an unknown request after SET");
                        break;
                }
            }
            break;
        case COMMAND_VOCAB_QUIT:{
            ok =true;
            yInfo("Stopping the module, bye bye");
            rec =true;
            this->stopModule();
            break;

        }
        case COMMAND_VOCAB_ADD:
            rec = true;
            {
                switch (command.get(1).asVocab()) {
                    default:
                        yInfo("received an unknown request after ADD");
                        break;
                }
            }
            break;
        case COMMAND_VOCAB_DEL:
            rec = true;
            {
                yInfo("Resetting grid");
                ok = true;
            }
            break;
        case COMMAND_VOCAB_GET:
            rec = true;
            {
                switch (command.get(1).asVocab()) {

                    case COMMAND_VOCAB_STATE: {
                        ok = true;
                        reply.addDouble(current_state);
                        break;
                    }
                    default:
                        yInfo("received an unknown request after a GET");
                        break;
                }
            }
            break;

        case COMMAND_VOCAB_SUSPEND:
            rec = true;
            {
                ok = true;
            }
            break;

        case COMMAND_VOCAB_RES:
            rec = true;
            {
                ok = true;
            }
            break;

        default:
            break;

    }
    mutex.post();

    if (!rec)
        ok = RFModule::respond(command, reply);
    if (!ok) {
        reply.clear();
        reply.addVocab(COMMAND_VOCAB_FAILED);
    } else
        reply.addVocab(COMMAND_VOCAB_OK);

    return ok;
}


bool YogaSupervisor::updateModule() {

    if (this->isStopping()) {return false;}
    yarp::os::Bottle cmd;
    std::string msg;
    std::vector<int> participantID_memoryDB;

    bool success;

     switch (current_state) {
        case IDLE: {
            cmd.clear();
            cmd.addString("set");
            cmd.addString("track");
            cmd.addInt(1);
            //yDebug("sent command %s to MOT", cmd.toString().c_str());
            sendRpcCommand(clientRpcMOT, cmd);




            break;
        }

        case DEBUG:{

            current_state = IDLE;
            break;
        }

        case INIT_EXP:{
            if (localiseSoundFace()) {
                cmd = yarp::os::Bottle("stop");
                sendRpcCommand(clientRPCSpeechRecognition, cmd);
                readSpeechFile(initExpSpeechFile);
                yarp::os::Time::delay(2);
                current_state = POSITION_PHASE;
            }
            break;
        }

        case POSITION_PHASE:{
            // TO remove just for Debug
            yarp::os::Time::delay(2);

            participantID_memoryDB.clear();
            //attivare MOT
            cmd.clear();
            cmd.addString("start");
            //yDebug("sent command %s to MOT", cmd.toString().c_str());
            sendRpcCommand(clientRpcMOT, cmd);

            yarp::os::Bottle *bottle_coord  = inputMotFacePort.read(true);
            participants_faces_coord = *bottle_coord;
            yInfo("Recognize face %d", participants_faces_coord.size());

            Vector ang(3);
            iGaze->getAngles(ang);          // get the current angular configuration
            for(int i =0; i < participants_faces_coord.size(); ++i) {

                bottle_coord  = participants_faces_coord.get(i).asList();
        
                const std::string tracker_id = bottle_coord->get(0).asString(); 
                bottle_coord = bottle_coord->get(1).asList();
                const int center_x = bottle_coord->get(0).asInt() + ((bottle_coord->get(2).asInt() - bottle_coord->get(0).asInt()) / 2);
                const int center_y = bottle_coord->get(1).asInt() + ((bottle_coord->get(3).asInt() - bottle_coord->get(1).asInt()) / 2);

                yarp::os::Time::delay(2);
                lookFace(center_x, center_y);
                yarp::os::Time::delay(2);

                cmd.clear();
                cmd.addString("start");
                sendRpcCommand(clientRPCPersonIdentification, cmd);

                std::string person_id = "undefined";
                while(person_id == "undefined"){
                    person_id = recognizePerson();
                }

                cmd.clear();
                cmd.addString("stop");
                sendRpcCommand(clientRPCPersonIdentification, cmd);

                verify_identity(person_id);
                cmd.clear();
                cmd.addString("save");
                cmd.addString("face");
                cmd.addString(person_id);
                sendRpcCommand(clientRPCPersonIdentification, cmd);


                addParticipant(person_id);

                msg = " Vai " + poseWordsVector.at(i) + " " + person_id;

                cmd.clear();
                cmd.addString("set");
                cmd.addString("id");
                cmd.addString(tracker_id);
                cmd.addString(person_id);
                //yDebug("sent command %s to MOT", cmd.toString().c_str());
                sendRpcCommand(clientRpcMOT, cmd);

//                askParticipantId("name", person_id, participantID_memoryDB);
               // setParticipantProperty(participantID_memoryDB.at(0), "col", partLabelVector.at(i));
//                setParticipantProperty(participantID_memoryDB.at(0), "trainingDone", "0");

                /*
                cmd.clear();
                cmd.addString("exe");
                cmd.addString(spatialPositionVector.at(i));
                yDebug("Received command %s", cmd.toString().c_str());
                sendRpcCommand(clientRPCInteractionInterface, cmd);

                writeToiSpeak(msg);
                */
                //attivare tracking with icub head
                cmd.clear();
                cmd.addString("set");
                cmd.addString("track");
                cmd.addInt(1);
                //yDebug("sent command %s to MOT", cmd.toString().c_str());
                sendRpcCommand(clientRpcMOT, cmd);


                while(!getEvent("end-tracking", false)){
                    //yDebug("waiting end of tracking");
                    trackFaceMot();
                }

                msg = "Va bene " + person_id + ", grazie.";
                writeToiSpeak(msg);



                iGaze->lookAtAbsAngles(ang);    // move the gaze wrt the current configuration
                iGaze->waitMotionDone(0.1, 3);
                while (inputFaceCoordinatePort.getPendingReads() > 0) {
                    inputFaceCoordinatePort.read(false);
                }


            }

            //readSpeechFile(trainingIntroSpeechFile);
            current_state = IDLE; //FOR DEBUG
            break;
        }

        case TRAINING_INTRO:{

            msg = "Chi vuole fare la prossima posa?";
            writeToiSpeak(msg);
            std::string user_name;
            int id;

            askParticipantId("trainingDone", "0", participantID_memoryDB);

            if(participantID_memoryDB.size() == participantIdVector.size() and localiseSoundFace()) {
                //yDebug("TRue");
                Vector ang(3);
                iGaze->getAngles(ang);         // get the current angular configuration
                ang[1] = -8.0;
                iGaze->lookAtAbsAngles(ang);    // move the gaze wrt the current configuration

                std::string label = getLabelPlayer();
                participantID_memoryDB.clear();
                askParticipantId("col", label, participantID_memoryDB);
                msg = "Bene! Iniziamo con te ";

            }
            else {

                id = participantID_memoryDB.at(0);

                std::string userColor = getParticipantProperty(id, "col");
                double posTrainingPlayer = getPosePlayer(userColor);

                //Let iCub look at the user to train
                Vector ang(3);
                iGaze->getAngles(ang);
                ang[0] = posTrainingPlayer;
                iGaze->lookAtAbsAngles(ang);
                //TODO look at the face
                msg = "Bene! Tocca a te";

            }

            user_name = getParticipantProperty(id, "name");
            setParticipantProperty(id, "trainingDone", "1");

            msg += ", " + user_name + "!";
            writeToiSpeak(msg);

            setUserName(user_name);

            current_state = SHOW_POSE;
            break;
        }


        case SHOW_POSE:{

            msg = "Ecco la posa che dovrai fare. Si chiama " + getPoseName();
            writeToiSpeak(msg);

            cmd = yarp::os::Bottle("pose show");
            sendRpcCommand(clientRPCYogaTrainer, cmd);

            yarp::os::Time::delay(12);

            current_state = TRAINING;

            break;
        }

        case TRAINING: {

            cmd = yarp::os::Bottle("start");
            sendRpcCommand(clientRPCYogaTrainer, cmd);

            while (!getEvent("yoga-end", true)) { yarp::os::Time::delay(0.5); }

            current_state = IDLE;
            break;
        }

         case TEST:{

             //TODO get id from participantIdVector and do a loop over the people and perform test
             //setParticipantProperty(id, "trainingDone", "0");

             msg = "Bene! Adesso fammi vedere come fai la posa senza il mio aiuto!";
             writeToiSpeak(msg);

             cmd = yarp::os::Bottle("set test");
             sendRpcCommand(clientRPCYogaTrainer, cmd);

             while(!getEvent("yoga-end", true)){yarp::os::Time::delay(0.5);}

             msg = "Grande! Andiamo avanti!";
             writeToiSpeak(msg);


             cmd.clear();
             cmd = yarp::os::Bottle("pose next");
             sendRpcCommand(clientRPCYogaTrainer, cmd);
             current_state = TRAINING_INTRO;
             break;
         }

         /*************************** DEMO STATES *******************************/
         case INIT_DEMO: {

             sendEmotion("all", "hap");

             cmd.clear();
             cmd.addString("exe");
             cmd.addString("saluta");
             yDebug("Received command %s", cmd.toString().c_str());
             sendRpcCommand(clientRPCInteractionInterface, cmd);
             //yarp::os::Time::delay(2);

             readSpeechFile(initExpSpeechFile);
             yarp::os::Time::delay(2);

             current_state = IDLE;
             break;
         }

         case INVITE_FIRST: {

             setUserName("DEMO_1");

             cmd.clear();
             cmd.addString("set");
             cmd.addString("pose");
             cmd.addInt(level);
             cmd.addInt(pose_index);
             yDebug("command is %s", cmd.toString().c_str());
             sendRpcCommand(clientRPCYogaTrainer, cmd);

             msg = "Allora, tu sei il primo! Mi raccomando! Massima concentrazione!!";
             writeToiSpeak(msg);
             yarp::os::Time::delay(3);

             sendEmotion("all", "evi");

             current_state = IDLE;
             break;
         }

         case SHOW_POSE_DEMO:{

             sendEmotion("all", "hap");

             msg = "Ecco la posa che dovrai fare. Si chiama " + getPoseName();
             writeToiSpeak(msg);

             cmd = yarp::os::Bottle("pose show");
             sendRpcCommand(clientRPCYogaTrainer, cmd);

             yarp::os::Time::delay(12);

             current_state = IDLE;
             break;
         }

         case TRAINING_DEMO: {

             Vector ang(3);
             iGaze->getAngles(ang);         // get the current angular configuration
             ang[1] = -8.0;
             iGaze->lookAtAbsAngles(ang);
             yarp::os::Time::delay(1);

             msg = "Adesso tocca a te! Via! ";
             writeToiSpeak(msg);

             cmd = yarp::os::Bottle("start");
             sendRpcCommand(clientRPCYogaTrainer, cmd);

             while (!getEvent("yoga-end", true)) { yarp::os::Time::delay(0.5); }

             current_state = IDLE;
             break;

         }

         case TEST_DEMO: {

            demo_counter += 1;
            yDebug("Demo counter %i", demo_counter);
            setUserName("DEMO_" + std::to_string(demo_counter));

             cmd.clear();
             cmd.addString("set");
             cmd.addString("pose");
             cmd.addInt(level);
             cmd.addInt(pose_index);
             sendRpcCommand(clientRPCYogaTrainer, cmd);

            if (demo_counter >= demo_nparticipants) {
                current_state = IDLE;
                yDebug("final eval");
                break;
            }

             msg = "Bene, e il tuo turno! Mettiti in posa per favore!";
             writeToiSpeak(msg);

             sendEmotion("all", "cun");

             cmd = yarp::os::Bottle("set test");
             sendRpcCommand(clientRPCYogaTrainer, cmd);

             while(!getEvent("yoga-end", true)){yarp::os::Time::delay(0.5);}

             msg = "Bene! Andiamo avanti!";
             writeToiSpeak(msg);

             sendEmotion("all", "hap");

             current_state = IDLE;
             break;

         }

         case FIN_EVAL_DEMO: {

             msg = "Vediamo come ve la siete cavata! E il momento dell'ultima posa! Via! ";
             writeToiSpeak(msg);

             cmd = yarp::os::Bottle("eval");
             sendRpcCommand(clientRPCYogaTrainer, cmd);

             while(!getEvent("yoga-end", true)){yarp::os::Time::delay(0.5);}

             current_state = IDLE;
             break;

         }

         case END_DEMO: {

             msg = "Grazie per aver giocato con me! Alla prossima amici!!";
             writeToiSpeak(msg);

             cmd.clear();
             cmd.addString("exe");
             cmd.addString("saluta");
             yDebug("Received command %s", cmd.toString().c_str());
             sendRpcCommand(clientRPCInteractionInterface, cmd);

             current_state = IDLE;
             break;

         }

     }
    return true;
}



/****************************************************** General FUNCTIONS *************************************************/

bool YogaSupervisor::sendRpcCommand(yarp::os::RpcClient& RpcPort, const yarp::os::Bottle& cmd){
    if (RpcPort.getOutputCount()) {

        yarp::os::Bottle response;
        RpcPort.write(cmd, response);

        return response.toString().find("ok") != std::string::npos;
    }

    return false;
}


bool YogaSupervisor::getEvent(const std::string& evenToRecognize, bool blocking) {
    // flush
    while(eventsPort.getPendingReads() > 0 ){
        yarp::os::Bottle *eventBottle = eventsPort.read(false);
        if (eventBottle->get(0).asString() == evenToRecognize){
            return true;
        }
    }
    yarp::os::Bottle *eventBottle = eventsPort.read(blocking);
    std::string eventMsg = eventBottle != nullptr ? eventBottle->get(0).asString() : "";
//    yDebug("Received events %s", eventMsg.c_str());
    return eventMsg == evenToRecognize;
}


bool YogaSupervisor::sendEmotion(std::string iCubFacePart, std::string emotionCmd) {
    if (clientRPCEmotion.getOutputCount()) {
        yarp::os::Bottle cmd;
        cmd.addString("set");
        cmd.addString(iCubFacePart);
        cmd.addString(emotionCmd);

        yarp::os::Bottle response;
        clientRPCEmotion.write(cmd, response);
//      yDebug("Send %s face emotion get response : %s", cmd.toString().c_str(), response.toString().c_str());

        return response.toString().find("[ok]") != std::string::npos;
    }
    return false;
}


bool YogaSupervisor::readSpeechFile(const std::string& fileNamePath) {

    std::ifstream infile(fileNamePath);
    std::string line;
    bool ret = true;
    while (std::getline(infile, line)) {
        if (line.find("wait") != std::string::npos){
            const int delay_time = std::atoi(line.substr(5,2).c_str());
            yarp::os::Time::delay(delay_time);
            continue;
        }
        ret &= writeToiSpeak(line);
        if (!ret) return ret;
    }
    return ret;
}

bool YogaSupervisor::writeToiSpeak(const std::string& toSpeak) {

    if(speechOutputPort.getOutputCount()){
        std::string new_msg = toSpeak + "\\mrk=1\\.";
        yarp::os::Bottle toSpeakBottle;
        toSpeakBottle.addString(new_msg);
        yDebug("Speech received is %s", toSpeakBottle.toString().c_str());
        bool ok = speechOutputPort.write(toSpeakBottle);
        yarp::os::Time::delay(1.5);

        return ok;
    }
    return false;
}


Boxe YogaSupervisor::bottleToBox(yarp::os::Bottle *rectBottle) {


    rectBottle  = rectBottle->get(0).asList();
    rectBottle = rectBottle->get(1).asList();
    Boxe boxe = {rectBottle->get(0).asDouble(), rectBottle->get(1).asDouble(),
                 rectBottle->get(2).asDouble(), rectBottle->get(3).asDouble()};

    return boxe;
}

bool YogaSupervisor::openPorts() {

    if (!handlerPort.open(handlerPortName)) {
        yInfo(" Unable to open port %s", this->handlerPortName.c_str());
        return false;
    }


    //audio recorder
    clientRpcAudioRecorder.setRpcMode(true);
    if(!clientRpcAudioRecorder.open(getName("/audioRecorder:rpc").c_str())){
        yError("unable to open port to save audio ");
        return false;
    }

    //emotions port
    clientRPCEmotion.setRpcMode(true);
    if(!clientRPCEmotion.open(getName("/emotions:rpc").c_str())){
        yError("unable to open port to send emotions ");
        return false;
    }

    clientRpcMOT.setRpcMode(true);
    if(!clientRpcMOT.open(getName("/mot:rpc").c_str())){
        yError("unable to open port to Mot commands ");
        return false;
    }

    clientRpcObjectProprertiesCollector.setRpcMode(true);
    if(!clientRpcObjectProprertiesCollector.open(getName("/objectsPropertiesCollector:rpc").c_str())){
        yError("unable to open port to Mot commands ");
        return false;
    }

    //interaction interface port
    clientRPCInteractionInterface.setRpcMode(true);
    if(!clientRPCInteractionInterface.open(getName("/cmd:rpc").c_str())){
        yError("unable to open port to send interaction command ");
        return false;
    }
    //SoundLocaliser port
    clientRpcSoundLocaliser.setRpcMode(true);
    if(!clientRpcSoundLocaliser.open(getName("/soundLocaliser:rpc").c_str())){
        yError("unable to open port to send soundLocaliser ");
        return false;
    }

    //SpeechRecognition port
    if (!speechRecognitionPort.open(handlerPortName + "/words:i")) {
        yError("unable to open port %s/%s ", getName().c_str(), "/words:i");
        return false;
    }

    clientRPCSpeechRecognition.setRpcMode(true);
    if (!clientRPCSpeechRecognition.open(handlerPortName + "/speechToText:rpc")) {
        yError("unable to open port %s/%s ", getName().c_str(), "/speechToText:rpc");
        return false;
    }

    //PersonRecognition port
    if (!personRecognitionPort.open(handlerPortName + "/person_id:i")) {
        yError("unable to open port %s/%s ", getName().c_str(), "person_id:i");
        return false;
    }

    clientRPCPersonIdentification.setRpcMode(true);
    if (!clientRPCPersonIdentification.open(handlerPortName + "/personRecognition:rpc")) {
        yError("unable to open port %s/%s ", getName().c_str(), "/personRecognition:rpc");
        return false;
    }

    //Yoga Trainer port
    clientRPCYogaTrainer.setRpcMode(true);
    if(!clientRPCYogaTrainer.open(getName("/yogaTrainer:rpc").c_str())){
        yError("unable to open port to send soundLocaliser ");
        return false;
    }

    //Speech Port
    if (!speechOutputPort.open(handlerPortName + "/speech:o")) {
        yError("unable to open port %s/%s ", getName().c_str(), "speech:o");
        return false;
    }

    // Face detection
    if (!inputFaceCoordinatePort.open(handlerPortName + "/faceCoordinate:i")) {
        yInfo("Unable to open port /faceCoordinate:i");
        return false;  // unable to open; let RFModule know so that it won't run
    }

    if (!inputMotFacePort.open(getName("/motCoordinate:i"))) {
        yInfo("Unable to open port /motCoordinate:i");
        return false;  // unable to open; let RFModule know so that it won't run
    }

    inputMotFacePort.setStrict(false);

    //Event Port
    if (!eventsPort.open(handlerPortName + "/events:i")) {
        yError("unable to open port %s/%s ", getName().c_str(), "events:i");
        return false;
    }
    eventsPort.setStrict(true);

    //Marker Speech Port
    if (!markerSpeechPort.open(handlerPortName + "/markerSpeech:i")) {
        yError("unable to open port %s/%s ", getName().c_str(), "markerSpeech:i");
        return false;
    }
    markerSpeechPort.setStrict(true);


    attach(handlerPort);                  // attach to port
    return true;
}


double YogaSupervisor::getIOU(Boxe boxA, Boxe boxB) {


    // determine the (x, y)-coordinates of the intersection rectangle
    const double xA = std::max(boxA.left, boxB.left);
    const double yA = std::max(boxA.top, boxB.top);
    const double xB = std::max(boxA.right, boxB.right);
    const double yB = std::max(boxA.bottom, boxB.bottom);

    // compute the area of intersection rectangle
    const double interArea = std::max(0.0, (xB - xA + 1.0)) * std::max(0.0, (yB - yA + 1.0));

    // Compute the area of both the prediction and ground-truth rectangles
    const double boxAArea = (boxA.right - boxA.left + 1) * (boxA.bottom - boxA.top + 1);
    const double boxBArea = (boxB.right - boxB.left + 1) * (boxB.bottom - boxB.top + 1);

    // compute the intersection over union by taking the intersection area and dividing it
    // by the sum of prediction + ground-truth areas - the interesection area
    const double iou = interArea / float(boxAArea + boxBArea - interArea);


    return iou;


    return 0;
}

/************************************************* iKinGazeCtrl functions **********************************************/
inline bool YogaSupervisor::localiseSoundFace(){
    yarp::os::Bottle cmd = yarp::os::Bottle("start");

    sendRpcCommand(clientRpcSoundLocaliser, cmd);
    sendEmotion("all", "hap");

    while(!getEvent("sound-localised", true)){yarp::os::Time::delay(0.5);}
    while (inputFaceCoordinatePort.getPendingReads() > 0) {
        inputFaceCoordinatePort.read(false);
    }
    yarp::os::Time::delay(1);

    yarp::os::Bottle * coordinate_face = inputFaceCoordinatePort.read(false);
    if (coordinate_face != nullptr) {
        cmd = yarp::os::Bottle("stop");
        sendRpcCommand(clientRpcSoundLocaliser, cmd);

        int center_facesX = 0, center_facesY = 0;
        // Compute the center of the face in the image reference frame and gaze at it
        getCenterFace(*coordinate_face, center_facesX, center_facesY);
        lookFace(center_facesX, center_facesY);

        return true;
    }


    return false;
}

bool YogaSupervisor::openIkinGazeCtrl() {

    //---------------------------------------------------------------
    yInfo("Opening the connection to the iKinGaze");
    yarp::os::Property optGaze; //("(device gazecontrollerclient)");
    optGaze.put("device", "gazecontrollerclient");
    optGaze.put("remote", "/iKinGazeCtrl");
    optGaze.put("local", "/soundLocalizer/gaze");

    clientGaze = new yarp::dev::PolyDriver();
    clientGaze->open(optGaze);
    iGaze = nullptr;
    yInfo("Connecting to the iKinGaze");
    if (!clientGaze->isValid()) {
        return false;
    }

    clientGaze->view(iGaze);
    iGaze->storeContext(&ikinGazeCtrl_Startcontext);


//    //Set trajectory time:
//    iGaze->blockNeckRoll();
//    iGaze->clearNeckPitch();
//
////    iGaze->blockEyes();
    iGaze->setNeckTrajTime(0.7);
    iGaze->setEyesTrajTime(0.6);
    iGaze->setTrackingMode(true);
    iGaze->setVORGain(1.3);
    iGaze->setOCRGain(0.8);

    iGaze->storeContext(&gaze_context);

    yInfo("Initialization of iKingazeCtrl completed");
    return true;
}

void YogaSupervisor::lookFace(const int x, const int y){
    yarp::sig::Vector px(3);   // specify the pixel where to look

    yarp::sig::Vector imageFramePosition(2);
    yarp::sig::Vector rootFramePosition(3);

    imageFramePosition[0] = x;
    imageFramePosition[1] = y;

    // On the 3D frame reference of the robot the X axis is the depth
    iGaze->get3DPoint(0, imageFramePosition, 1.0, px);
    iGaze->lookAtFixationPoint(px);
    iGaze->waitMotionDone(0.1, 1);

    yInfo("Face found, gazing at  %d %d", x, y);
}

void YogaSupervisor::getCenterFace(const yarp::os::Bottle &coordinate, int &center_x, int &center_y) {


     for(int i=0; i < coordinate.size(); ++i){
         yarp::os::Bottle *bottle_coord = coordinate.get(i).asList();
         bottle_coord = bottle_coord->get(2).asList();

         center_x += bottle_coord->get(0).asInt() + ((bottle_coord->get(2).asInt() - bottle_coord->get(0).asInt()) / 2);
         center_y += bottle_coord->get(1).asInt() + ((bottle_coord->get(3).asInt() - bottle_coord->get(1).asInt()) / 2);

     }

     center_x /= coordinate.size();
     center_y /=  coordinate.size();


}

bool YogaSupervisor::trackIkinGazeCtrl(double x, double y, int width, int height) {


    // Calcul the center of the Rectangle
    const double imagePositionX = x + (width / 2);
    const double imagePositionY = y + (height / 2);

    if (previousImagePosX < 0) {
        previousImagePosX = imagePositionX;
        previousImagePosY = imagePositionY;
        return false;
    }


    yarp::sig::Vector imageFramePosition(2);
    yarp::sig::Vector rootFramePosition(3);

    imageFramePosition[0] = imagePositionX ;
    imageFramePosition[1] = imagePositionY;

    // On the 3D frame reference of the robot the X axis is the depth
    iGaze->get3DPoint(0, imageFramePosition, 1, rootFramePosition);


    // Calcul the Euclidian distance in image plane
    const double distancePreviousCurrent_X = sqrt(
            pow((imagePositionX - previousImagePosX), 2));

    const double distancePreviousCurrent_Y = sqrt(
            pow((imagePositionY - previousImagePosY), 2));


    if (distancePreviousCurrent_X > distance_tracking_X || distancePreviousCurrent_Y > distance_tracking_Y) {


        // Storing the previous coordinate in the Robot frame reference

        //yInfo("Position  is %f %f %f", rootFramePosition[0], rootFramePosition[1], rootFramePosition[2]);
        iGaze->lookAtFixationPoint(rootFramePosition);

        //iGaze->lookAtMonoPixel(drivingCamera, imageFramePosition );
        previousImagePosX = imagePositionX;
        previousImagePosY = imagePositionY;
        return true;
    }
    return false;
}



/************************************************* Speech recognition functions **********************************************/

bool YogaSupervisor::sentenceDone() {
    // flush
    while(markerSpeechPort.getInputCount() > 0){
        yarp::os::Bottle *markerBottle = markerSpeechPort.read(true);
        if (markerBottle->get(0).asInt() == 1){
            yDebug("Received end of sentence");
            yarp::os::Time::delay(0.5);
            return true;
        }
    }
    return false;
}

std::string inline YogaSupervisor::recognizeSpeech(){
    std::string speech_detected = "";
    yarp::os::Bottle cmd = yarp::os::Bottle("start");
    sendRpcCommand(clientRPCSpeechRecognition, cmd);

    do {
        speech_detected = readSpeech();
    } while (speech_detected.empty());

    cmd = yarp::os::Bottle("stop");
    sendRpcCommand(clientRPCSpeechRecognition, cmd);

    return speech_detected;
}

std::string YogaSupervisor::recognizePerson(){

    std::string personId = "undefined";
    if(personRecognitionPort.getInputCount()){
        yarp::os::Bottle *personIdBottle = personRecognitionPort.read();
        personId = personIdBottle->get(0).asString().c_str();
        yInfo("Person recognized: %s", personId.c_str());
    }
   return personId;
}

std::string YogaSupervisor::readSpeech(){

    std::string wordDetected = "";
    if(speechRecognitionPort.getInputCount()){
        yarp::os::Bottle *wordDetectedBottle = speechRecognitionPort.read(false);
        if (wordDetectedBottle){
            wordDetected = wordDetectedBottle->toString();
            yInfo("Word Detected: %s", wordDetected.c_str());
        }
    }
   return wordDetected;
}

void YogaSupervisor::verify_identity(std::string &person_id){
    std::string msg;
    std::string speech_detected;

    if (person_id == "unknown") {
        msg = "Credo che non ci conosciamo. Giusto?";
        writeToiSpeak(msg);

        if(sentenceDone()) speech_detected = recognizeSpeech();
        if(speech_detected.find("sì") != std::string::npos || speech_detected.find("giusto") != std::string::npos){
            msg = "Come ti chiami?";
            writeToiSpeak(msg);
        }
        else {
            msg = "Scusami! A volte ho una pessima memoria! Puoi ricordarmi il tuo nome?";
            writeToiSpeak(msg);
        }
    }
    else {
        msg = "Mi sembra di averti già visto! Giusto?";
        writeToiSpeak(msg);
        if(sentenceDone()) speech_detected = recognizeSpeech();

        bool  recognized = false;
        while(!recognized){

            if(speech_detected.find("sì") != std::string::npos || speech_detected.find("giusto") != std::string::npos){
                verify_name = true;
                recognized = true;
            }
            else if(speech_detected.find("no") != std::string::npos) {
                msg = "Scusa! Devo averti scambiato per qualcun altro! Come ti chiami?";
                writeToiSpeak(msg);
                recognized = true;

            }
            else{
                speech_detected = recognizeSpeech();
                continue;
            }
        }
       

    }

    if (!verify_name){
        if(sentenceDone()) speech_detected = recognizeSpeech();
        std::size_t found = speech_detected.find_last_of(" ");
        person_id = speech_detected.substr(found+1);
        // Clean the string
        person_id.erase(std::remove_if(person_id.begin(), person_id.end(), [](char c) { return !isalnum(c); }), person_id.end());
        verify_name = true;
    }


    while(verify_name){
        msg = "Ti chiami  " + person_id + ",  vero?";
        writeToiSpeak(msg);

        if(sentenceDone()) speech_detected = recognizeSpeech();
        if(speech_detected.find("sì") != std::string::npos || speech_detected.find("vero") != std::string::npos){
            verify_name = false;
        }
        else if (speech_detected.find("no") != std::string::npos ){
            msg = "Scusami, mi puoi ripetere il tuo nome per favore?";
            writeToiSpeak(msg);

            if(sentenceDone()) speech_detected = recognizeSpeech();
            std::size_t found = speech_detected.find_last_of(" ");
            person_id = speech_detected.substr(found+1);
            // Clean the string
            person_id.erase(std::remove_if(person_id.begin(), person_id.end(), [](char c) { return !isalnum(c); }), person_id.end());
        }
        else{
            continue;
        }
    }
}

/************************************************* YogaTeacher Functions *********************************/

bool YogaSupervisor::setUserName(const std::string& user_name) {
    if (clientRPCYogaTrainer.getOutputCount()) {

        yarp::os::Bottle response,cmd;
        cmd.addString("set");
        cmd.addString("username");
        cmd.addString(user_name);
        clientRPCYogaTrainer.write(cmd, response);
        yDebug("Sent %s command to YogaTrainer, got response %s", cmd.toString().c_str(), response.toString().c_str());

        return response.toString().find("ok") != std::string::npos;
    }

    return false;
 }


void YogaSupervisor::bottleToVector(yarp::os::Bottle *bottle, std::vector<std::string> *vector) {
    std::string playerLog = "";
    for (int i = 0; i < bottle->size(); i++) {
        vector->push_back(bottle->get(i).asString());
        playerLog += bottle->get(i).asString() + " ";
    }
    yInfo("Reading playerLog: %s", playerLog.c_str());
}


std::string YogaSupervisor::getPoseName(){
    if (clientRPCYogaTrainer.getOutputCount()) {

        yarp::os::Bottle cmd("pose name"), response;
        clientRPCYogaTrainer.write(cmd, response);
        yDebug("Sent %s command to YogaTrainer, got response %s", cmd.toString().c_str(), response.toString().c_str());

        return response.get(0).asString();
    }

    return "";
}


bool YogaSupervisor::loadSpeech(yarp::os::ResourceFinder &rf){

    std::string fullpath = "";
    std::string pathSearch = rf.check("pathSearch",yarp::os::Value("/usr/local/src/robot/cognitiveInteraction/iCubYogaTeacher/yogaSupervisor/app/conf"),
                          "Path where to find for the .ini file").asString();

    // Check for yogaSupervisor .ini filename
    yogaSupervisorIniFile = rf.check("yoga_supervisor_ini_file", yarp::os::Value("yogaSupervisor.ini"),
                                        " FileName for iCub ini file (string)").asString();
    fullpath = pathSearch + "/" + yogaSupervisorIniFile;
    yogaSupervisorIniFile = rf.findFile(fullpath);
    if (yogaSupervisorIniFile.empty()) {
        yError("yoga_supervisor_ini_file not found at %s ", fullpath.c_str());
        return false;
    }


    // Check for init exp speech filename
    initExpSpeechFile = rf.check("init_exp_filename", yarp::os::Value("init_exp.txt"),
                                 " FileName for iCub speech in phase init_exp (string)").asString();
    fullpath = pathSearch + "/" + initExpSpeechFile;
    initExpSpeechFile = rf.findFile(fullpath);
    if (initExpSpeechFile.empty()) {
        yError("init_exp_filename not found at %s ", fullpath.c_str());
        return false;
    }


    //Check for training_intro speech filename
    trainingIntroSpeechFile = rf.check("training_intro filename", yarp::os::Value("training_intro.txt"),
                                 " FileName for iCub speech in phase training intro (string)").asString();
    fullpath = pathSearch + "/" + trainingIntroSpeechFile;
    trainingIntroSpeechFile = rf.findFile(fullpath);
    if (trainingIntroSpeechFile.empty()) {
        yError("training_intro not found at %s ", fullpath.c_str());
        return false;
    }

    return true;

}

/************************************************* Working Memory and MOT functions **********************************************/

bool YogaSupervisor::trackFaceMot(){
    yarp::os::Bottle *faceCoordinateBottle = inputMotFacePort.read(false);
    if (faceCoordinateBottle != nullptr) {
        Boxe current_face_box = bottleToBox(faceCoordinateBottle);


        trackIkinGazeCtrl(current_face_box.left, current_face_box.top, current_face_box.get_width(),
                          current_face_box.get_height());


        return true;
    }
    return false;
}


std::string YogaSupervisor::getLabelPlayer() {

    if (clientRpcMOT.getOutputCount()) {

        yarp::os::Bottle cmd;
        yarp::os::Bottle response;

        cmd.clear();
        cmd.addString("get");
        cmd.addString("player");

        clientRpcMOT.write(cmd, response);
        yDebug("Sent %s command to MOT, got response %s", cmd.toString().c_str(), response.toString().c_str());

        if (response.toString().find("nack") == std::string::npos){
            yDebug("Returning label %s", response.get(0).asString().c_str());
            return response.get(0).asString();
        }
    }

    return "";
}

double YogaSupervisor::getPosePlayer(std::string colorPlayer) {

    if (clientRpcMOT.getOutputCount()) {
        yarp::os::Bottle cmd;
        cmd.clear();
        cmd.addString("get");
        cmd.addString("pose");
        cmd.addString(colorPlayer);
        yarp::os::Bottle response;
        clientRpcMOT.write(cmd, response);
        yDebug("Sent %s command to MOT, got response %s", cmd.toString().c_str(), response.toString().c_str());

        if (response.toString().find("nack") == std::string::npos){
            yDebug("Returning angle %d", response.get(0).asInt());
            return response.get(0).asInt();
        }
    }
    return -666;
}

/************************************************* Database functions **********************************************/

/*
bool YogaSupervisor::addObjectProperty(std::string property, std::string& value){

    if (clientRpcObjectProprertiesCollector.getOutputCount()) {
        yarp::os::Bottle response, cmd, cmdList, cmdFinal;
        cmd.clear();
        cmdList.clear();
        cmdFinal.clear();

        cmd.addString(property);
        cmd.addString(value);

        cmdList.addList() = cmd;

        cmdFinal.addVocab(COMMAND_VOCAB_ADD);
        cmdFinal.addList() = cmdList;

        clientRpcObjectProprertiesCollector.write(cmdFinal, response);
        yDebug("sent command %s to ObjectPropertiesCollector, got response %s", cmdFinal.toString().c_str(), response.toString().c_str());

        return response.toString().find("ok") != std::string::npos;

    }

    return false;
}
*/

int YogaSupervisor::addParticipant(std::string& participant_id){

    if (clientRpcObjectProprertiesCollector.getOutputCount()) {
        yarp::os::Bottle response, cmd, cmdList, cmdFinal;
        response.clear();
        cmd.clear();
        cmdList.clear();
        cmdFinal.clear();

        cmd.addString("name");
        cmd.addString(participant_id);

        cmdList.addList() = cmd;

        cmdFinal.addVocab(COMMAND_VOCAB_ADD);
        cmdFinal.addList() = cmdList;

        clientRpcObjectProprertiesCollector.write(cmdFinal, response);
        yDebug("sent command %s to ObjectPropertiesCollector, got response %s", cmdFinal.toString().c_str(), response.toString().c_str());

        return response.get(1).asList()->get(1).asInt();
    }

}


void YogaSupervisor::askParticipantId(std::string property, std::string value, std::vector<int> &particpantsID){

    if (clientRpcObjectProprertiesCollector.getOutputCount()) {
        yarp::os::Bottle response, cmd, cmdList, cmdFinal;
        cmd.clear();
        cmdList.clear();
        cmdFinal.clear();

        cmd.addString(property);
        cmd.addString("==");
        cmd.addString(value);

        cmdList.addList() = cmd;

        cmdFinal.addString("ask");
        cmdFinal.addList() = cmdList;

        clientRpcObjectProprertiesCollector.write(cmdFinal, response);
        yDebug("sent command %s to ObjectPropertiesCollector, got response %s", cmdFinal.toString().c_str(), response.toString().c_str());
        //yDebug("string is %s", response.get(1).asList()->get(1).toString().c_str());

        const yarp::os::Bottle *listId = response.get(1).asList()->get(1).asList();
        for (int i=0; i< listId->size(); ++i){
            particpantsID.push_back(listId->get(i).asInt());
        }
    }

}

bool YogaSupervisor::setParticipantProperty(int& participant_id, std::string property, std::string value){

    if (clientRpcObjectProprertiesCollector.getOutputCount()) {

        yarp::os::Bottle response, cmd, idCmd, cmdList, cmdFinal;
        cmd.clear();
        idCmd.clear();
        cmdList.clear();
        cmdFinal.clear();

        idCmd.addString("id");
        idCmd.addInt(participant_id);

        cmd.addString(property);
        cmd.addString(value);

        cmdList.addList() = idCmd;
        cmdList.addList() = cmd;

        cmdFinal.addVocab(COMMAND_VOCAB_SET);
        cmdFinal.addList() = cmdList;

        clientRpcObjectProprertiesCollector.write(cmdFinal, response);
        yDebug("sent command %s to ObjectPropertiesCollector, got response %s", cmdFinal.toString().c_str(), response.toString().c_str());

        return response.toString().find("ok") != std::string::npos;
    }

    return false;
}

std::string YogaSupervisor::getParticipantProperty(int& participant_id, std::string property){

    if (clientRpcObjectProprertiesCollector.getOutputCount()) {

        yarp::os::Bottle response, cmd, idCmd, cmdList, prop, cmdFinal;
        cmd.clear();
        idCmd.clear();
        cmdList.clear();
        cmdFinal.clear();

        idCmd.addString("id");
        idCmd.addInt(participant_id);

        cmd.addString("propSet");
        prop.addString(property);
        cmd.addList() = prop;

        cmdList.addList() = idCmd;
        cmdList.addList() = cmd;

        cmdFinal.addVocab(COMMAND_VOCAB_GET);
        cmdFinal.addList() = cmdList;

        clientRpcObjectProprertiesCollector.write(cmdFinal, response);
        yDebug("sent command %s to ObjectPropertiesCollector, got response %s", cmdFinal.toString().c_str(), response.toString().c_str());

        return response.get(1).asList()->get(0).asList()->get(1).asString();
    }
}

std::string YogaSupervisor::askAllId(){

    if (clientRpcObjectProprertiesCollector.getOutputCount()) {

        yarp::os::Bottle response, cmd, cmdFinal;

        cmd.clear();
        cmdFinal.clear();

        cmd.addString("all");
        cmdFinal.addString("ask");
        cmdFinal.addList() = cmd;

        clientRpcObjectProprertiesCollector.write(cmdFinal, response);
        yDebug("sent command %s to ObjectPropertiesCollector, got response %s", cmdFinal.toString().c_str(), response.toString().c_str());

        return response.get(1).asList()->get(1).asList()->toString();
    }
}


