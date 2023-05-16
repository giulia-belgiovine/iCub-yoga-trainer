#ifndef YOGA_SUPERVISOR_MODULE_H
#define YOGA_SUPERVISOR_MODULE_H

#include <iostream>
#include <string>
#include <yarp/os/RFModule.h>
#include <yarp/os/RpcClient.h>
#include <yarp/os/Network.h>
#include <yarp/os/Vocab.h>
#include <yarp/os/BufferedPort.h>
#include <yarp/os/Semaphore.h>
#include <yarp/dev/GazeControl.h>
#include <yarp/dev/PolyDriver.h>
#include <vector>
#include <cmath>

struct Boxe{
    double left;
    double top;
    double right;
    double bottom;
    int get_width() const { return right - left; }
    int get_height() const { return bottom - top; }
};


class YogaSupervisor : public yarp::os::RFModule {

public:

    bool configure(yarp::os::ResourceFinder &rf) override;

    bool interruptModule() override;

    bool close() override;

    bool respond(const yarp::os::Bottle &command, yarp::os::Bottle &reply) override;

    double getPeriod() override;

    bool updateModule() override;

private :
    enum states { IDLE = 0,
                  INIT_EXP = 1,
                  POSITION_PHASE = 2,
                  TRAINING_INTRO = 3,
                  SHOW_POSE = 4,
                  TRAINING = 5,
                  TEST = 6,
                  END_EXP = 7,
                  DEBUG = 8,

                  INIT_DEMO = 10,
                  INVITE_FIRST = 11,
                  SHOW_POSE_DEMO = 12,
                  TRAINING_DEMO = 13,
                  TEST_DEMO = 14,
                  FIN_EVAL_DEMO = 15,
                  END_DEMO = 16
                };

    enum {
        // general command vocab's
        COMMAND_VOCAB_OK = yarp::os::createVocab('o', 'k'),
        COMMAND_VOCAB_STATE = yarp::os::createVocab('e', 't', 'a', 't'),

        COMMAND_VOCAB_SET = yarp::os::createVocab('s', 'e', 't'),
        COMMAND_VOCAB_GET = yarp::os::createVocab('g', 'e', 't'),
        COMMAND_VOCAB_SUSPEND = yarp::os::createVocab('s', 'u', 's'),
        COMMAND_VOCAB_RES = yarp::os::createVocab('r', 'e', 's'),
        COMMAND_VOCAB_DEL = yarp::os::createVocab('d', 'e', 'l'),
        COMMAND_VOCAB_ADD = yarp::os::createVocab('a', 'd', 'd'),


        COMMAND_VOCAB_QUIT = yarp::os::createVocab('q', 'u', 'i', 't'),
        COMMAND_VOCAB_HELP = yarp::os::createVocab('h', 'e', 'l', 'p'),
        COMMAND_VOCAB_FAILED = yarp::os::createVocab('f', 'a', 'i', 'l'),
    };

    /* MODULE GENERAL PARAMETERS */
    std::string moduleName;                  // name of the module
    std::string handlerPortName;             // name of handler port
    std::string robotName;             // name of handler port
    int current_state;

    /* Ports */
    yarp::os::Port handlerPort;
    yarp::os::BufferedPort<yarp::os::Bottle> markerSpeechPort;
    yarp::os::BufferedPort<yarp::os::Bottle> eventsPort;
    yarp::os::Port speechOutputPort;
    yarp::os::BufferedPort<yarp::os::Bottle> personRecognitionPort;
    yarp::os::BufferedPort<yarp::os::Bottle> speechRecognitionPort;
    yarp::os::BufferedPort<yarp::os::Bottle> inputIdSpeakerPort;
    yarp::os::BufferedPort<yarp::os::Bottle> inputFaceCoordinatePort;
    yarp::os::BufferedPort<yarp::os::Bottle> inputMotFacePort;


    /* RpcClient */
    yarp::os::RpcClient clientRPCPersonIdentification;
    yarp::os::RpcClient clientRpcSoundLocaliser;
    yarp::os::RpcClient clientRpcAudioRecorder;
    yarp::os::RpcClient clientRPCEmotion;
    yarp::os::RpcClient clientRPCInteractionInterface;
    yarp::os::RpcClient clientRPCYogaTrainer;
    yarp::os::RpcClient clientRPCSpeechRecognition;
    yarp::os::RpcClient clientRpcMOT;
    yarp::os::RpcClient clientRpcObjectProprertiesCollector;

    yarp::os::Semaphore mutex;

    std::string yogaSupervisorIniFile;
    std::string initExpSpeechFile;
    std::string trainingIntroSpeechFile;

    bool trainingFlag = false;
    bool verify_name = false;

    std::vector<std::string> spatialPositionVector;
    std::vector<std::string> poseWordsVector;
    std::vector<std::string> namePositionVector;
    std::vector<std::string> partLabelVector;
    std::vector<int> participantIdVector;

    /*  IKINGAZECTRL  PARAMETERS */
    int ikinGazeCtrl_Startcontext{}, gaze_context{};
    yarp::dev::PolyDriver *clientGaze{};
    yarp::dev::IGazeControl *iGaze{};

    /*  MEMORY+MOT  PARAMETERS */
    Boxe previous_box_coordinate;
    double previousImagePosX;
    double previousImagePosY;
    int distance_tracking_X;
    int distance_tracking_Y;
    int memorised_pose_azimuth;
    yarp::os::Bottle participants_faces_coord;

    /* DEMO VARIABLES */
    int demo_counter = 1;
    int demo_nparticipants = 20;
    int level, pose_index;


/************************************************* General functions **********************************************/

    bool openPorts();
    /**
     * Check if a .txt file exist
     * @param rf
     * @return true is file path exist false otherwise
     */
    bool checkSpeechFiles(yarp::os::ResourceFinder &rf);

    /**
     * Read a .txt line by line and send the content to speech:o yarp Port
     * @param fileNamePath
     * @return true if successfully send to the port
     */
    bool readSpeechFile(const std::string& fileNamePath);

    /**
     * Write to the Yarp Port speech:o
     * @param toSpeak
     * @return true is success false otherwise
     */
    bool writeToiSpeak(const std::string& toSpeak);
    void bottleToVector(yarp::os::Bottle *bottle, std::vector<std::string> *vector);
    bool loadSpeech(yarp::os::ResourceFinder &rf);
    bool sendEmotion(std::string iCubFacePart , std::string emotionCmd);

    /**
    * Read event from yarp port event:i, blocking port will wait until an event arrives
    * @return empty string is no event was red else the read event
    */
    bool getEvent(const std::string& evenToRecognize, bool blocking);
    bool sendRpcCommand(yarp::os::RpcClient& RpcPort, const yarp::os::Bottle& cmd);
    double getIOU(Boxe rect_1, Boxe rect_2);

    /************************************************* iKinGazeCtrl functions **********************************************/
    void getCenterFace(const yarp::os::Bottle& coordinate, int &center_x, int &center_y);
    void lookFace(const int x, const int y);
    bool openIkinGazeCtrl();
    bool inline localiseSoundFace();

    /************************************************* Working Memory and MOT functions **********************************************/

    bool trackFaceMot();
    Boxe bottleToBox(yarp::os::Bottle *rectBottle);
    bool trackIkinGazeCtrl(double x, double y, int width, int height);
    /**
    * Get color label from Mot rpc port, given head position of iCub robot
    * @return string of the color
    */
    std::string getLabelPlayer();
    /**
    * Get spatial position from the memory, given the color label of player
    * @return spatial position based on iCub head reference frame
    */
    double getPosePlayer(std::string colorPlayer);

    /************************************************* Person recognition functions **********************************************/
    std::string recognizePerson();

    /************************************************* Speech recognition functions **********************************************/
    void verify_identity(std::string &person_id);
    bool sentenceDone();
    std::string inline recognizeSpeech();
    std::string readSpeech();

    /************************************************* YogaTeacher Functions *********************************/
    bool setUserName(const std::string& user_name);
    std::string getPoseName();

   /************************************************* objectPropertiesCollector Functions *********************************/
    //bool addObjectProperty(std::string property, std::string& value); //general function
    int addParticipant(std::string& participant_name);
    void askParticipantId(std::string property, std::string value, std::vector<int> &particpantsID);
    bool setParticipantProperty(int& participant_id, std::string property, std::string value);
    std::string getParticipantProperty(int& participant_id, std::string property);
    std::string askAllId();

};

#endif //YOGA_SUPERVISOR_MODULE_H