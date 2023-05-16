import yarp
import sys
import numpy as np
import pandas as pd
import os
import csv
import time

from yogaRecognition.src import util
from yogaRecognition.src.body import Body
from yogaRecognition.src.pose import Pose
from yogaRecognition.src.user_profile import UserProfile
from trainer import YogaTrainer
from torchvision import transforms
from PIL import Image
import cv2
import torch
import copy

torch.manual_seed(17)


def yarpdebug(msg):
    print("[DEBUG]" + msg)


def yarpinfo(msg):
    print("[INFO] {}".format(msg))


class YogaModule(yarp.RFModule):
    """
    Description:
        Class to recognize yoga pose from iCub cameras
    Args:
        input_port  : images from cameras
    """

    def __init__(self):
        yarp.RFModule.__init__(self)

        # handle port for the RFModule
        self.handle_port = yarp.Port()
        self.attach(self.handle_port)

        self.i = 0

        self.yoga_trainer = None
        self.yoga_pose_path = None
        self.body_estimation = None
        self.pose = None
        self.process = False
        #self.known_user = False
        self.iCub_show_pose = True

        # Input port
        self.image_in_port = yarp.BufferedPortImageRgb()
        self.label_port = yarp.Port()
        self.cmd_port = yarp.BufferedPortBottle()
        self.person_recognition_port = yarp.BufferedPortBottle()


        # Output port
        self.output_img_port = yarp.BufferedPortImageRgb()
        self.display_buf_array = None
        self.display_buf_image = yarp.ImageRgb()
        self.eventOutputPort = yarp.BufferedPortBottle()
        self.iSpeak = yarp.BufferedPortBottle()


        self.width_img = None
        self.height_img = None
        self.input_img_array = None
        self.user_name = None
        self.test_phase = False
        self.eval_phase = False
        self.train_phase = False

        self.module_name = None
        self.model_path = None
        self.root_path = None
        self.user_profile_path = None
        self.model = None
        self.device = None
        self.user_profilers = {}

        self.test_phase = False
        self.feedback_once = True
        self.success_counter = 20
        self.frequency_feedback = 30

    def configure(self, rf):

        self.module_name = rf.check("name",
                                    yarp.Value("YogaTeacher"),
                                    "module name (string)").asString()

        self.model_path = rf.check("model_path",
                                   yarp.Value(
                                       "/home/icub/Documents/Jonas/iCubYogaTeacher/yogaRecognition/model/body_pose_model.pth")).asString()

        self.root_path = rf.check("yoga_pose_path",
                                  yarp.Value("/home/icub/Documents/Jonas/iCubYogaTeacher/yogaRecognition")).asString()

        self.user_profile_path = rf.check("user_profile_path",
                                  yarp.Value("")).asString()

        if not os.path.exists(self.user_profile_path):
            print("[ERROR] user_profile_path does not exist")
            return False

        # Create handle port to read message
        self.handle_port.open('/' + self.module_name)

        # Open ports
        self.label_port.open('/' + self.module_name + '/label:o')
        self.cmd_port.open('/' + self.module_name + '/cmd:o')
        self.person_recognition_port.open('/' + self.module_name + '/person_id:i')
        self.image_in_port.open('/' + self.module_name + '/image:i')
        self.iSpeak.open('/' + self.module_name + '/speak:o')
        self.eventOutputPort.open('/' + self.module_name + '/events:o')

        self.output_img_port.open('/' + self.module_name + '/image:o')

        # Initialize variables
        self.width_img = rf.check('width', yarp.Value(320),
                                  'Width of the input image').asInt()

        self.height_img = rf.check('height', yarp.Value(244),
                                   'Height of the input image').asInt()

        self.input_img_array = np.zeros((self.height_img, self.width_img, 3), dtype=np.uint8).tobytes()

        # classes
        self.pose = Pose()
        self.yoga_trainer = YogaTrainer(self.root_path)
        self.body_estimation = Body(self.model_path)

        try:
            self.model = Body(self.model_path)
        except Exception as e:
            print(f"[ERROR] Cannot load model please check the path {self.model_path}")
            print(e)
            return False

        print("Model successfully loaded, running")

        self.device = torch.device('cuda:0' if torch.cuda.is_available() else 'cpu')
        print('Running on device: {}'.format(self.device))

        yarpdebug("Initialization complete")

        return True

    def interruptModule(self):
        yarpdebug("stopping the module")
        self.show_yoga_pose("go_home")

        self.handle_port.interrupt()
        self.image_in_port.interrupt()
        self.label_port.interrupt()
        self.cmd_port.interrupt()
        self.person_recognition_port.interrupt()
        self.output_img_port.interrupt()
        self.iSpeak.interrupt()
        self.eventOutputPort.interrupt()

        return True

    def close(self):
        self.handle_port.close()
        self.image_in_port.close()
        self.label_port.close()
        self.cmd_port.close()
        self.person_recognition_port.close()
        self.output_img_port.close()
        self.iSpeak.close()
        self.eventOutputPort.close()

        return True

    def respond(self, command, reply):
        ok = False

        # Is the command recognized
        rec = False

        reply.clear()

        if command.get(0).asString() == "pose":
            if command.get(1).asString() == "show":
                target_pose_name, _ = self.yoga_trainer.get_pose()

                self.show_yoga_pose(target_pose_name)
                reply.addString("ok")

            elif command.get(1).asString() == "next":
                self.yoga_trainer.update_exercise()
                reply.addString("ok")

            elif command.get(1).asString() == "name":
                pose_name = self.yoga_trainer.get_pose_name()
                pose_name = pose_name.split("_")[0]
                reply.addString(pose_name)

            else:
                reply.addString("nack")

        if command.get(0).asString() == "set":
            if command.get(1).asString() == "username":
                self.user_name = command.get(2).asString()
                if self.user_name not in self.user_profilers.keys():
                    self.user_profilers[self.user_name] = UserProfile(self.user_name, self.user_profile_path)

                self.yoga_trainer.set_user_profile(self.user_profilers[self.user_name])
                reply.addString("ok")

            elif command.get(1).asString() == "test":
                self.test_phase = True
                self.process = True
                self.train_phase = False
                self.eval_phase = False
                self.yoga_trainer.init_params()
                print(self.yoga_trainer.get_pose_name())
                reply.addString("ok")

            elif command.get(1).asString() == "pose":
                self.yoga_trainer.current_level = command.get(2).asInt()
                self.yoga_trainer.pose_index = command.get(3).asInt()
                reply.addString("ok")

            else:
                reply.addString("nack")

        elif command.get(0).asString() == "start":
            self.process = True
            self.train_phase = True
            self.feedback_once = True
            self.eval_phase = False
            self.test_phase = False
            self.yoga_trainer.init_params()

            reply.addString("ok")

        elif command.get(0).asString() == "eval":
            self.yoga_trainer.init_params()
            self.process = True
            self.eval_phase = True
            self.train_phase = False
            self.test_phase = False

            reply.addString("ok")

        elif command.get(0).asString() == "stop":
            self.process = False
            reply.addString("ok")

        elif command.get(0).asString() == "quit":
            reply.addString("quitting")
            return False

        elif command.get(0).asString() == "help":
            help_msg = "Yoga teacher module command are:"
            help_msg += "start/stop: To start and run the module"
            reply.addString(help_msg)

        return True

    def getPeriod(self):
        """
           Module refresh rate.
           Returns : The period of the module in seconds.
        """
        return 0.05

    def get_username(self):
        user_name = None
        if self.person_recognition_port.getInputCount():
            user_name = self.person_recognition_port.read(False)

        return user_name

    def updateModule(self):

        if self.process:

            input_yarp_image = self.image_in_port.read(False)

            # read yarp image
            if input_yarp_image and self.user_name is not None:
                frame_input = self.format_yarp_image(input_yarp_image)
                candidate, subset = self.model(frame_input)
                canvas = copy.deepcopy(frame_input)
                canvas, joint_values = util.draw_bodypose(canvas, candidate, subset)
                canvas = cv2.resize(canvas, (800, 800), interpolation=cv2.INTER_AREA)

                try:
                    current_pose = self.pose(joint_values)

                    if self.train_phase:

                        if self.feedback_once and self.yoga_trainer.user.known_user:
                            pers_feedback = self.yoga_trainer.get_personalize_feedback()
                            self.write_iSpeak(pers_feedback)
                            self.feedback_once = False
                            time.sleep(5)

                        msg_instruction = self.yoga_trainer.get_pose_feedback(current_pose)  

                        if self.i % self.frequency_feedback == 0:
                            self.write_iSpeak(msg_instruction) 
                        self.i += 1

                        self.process = self.yoga_trainer.ctr_pose_success <= self.success_counter

                    elif self.test_phase:
                        self.process = self.yoga_trainer.test_pose(current_pose)

                    elif self.eval_phase:
                        msg_evaluation = self.yoga_trainer.give_generic_evaluation(current_pose)
                        if msg_evaluation:
                            self.write_iSpeak(msg_evaluation)
                            self.process = False

                    if not self.process:
                        self.test_phase = False
                        self.eval_phase = False
                        self.train_phase = False
                        self.send_event("yoga-end")

                except KeyError as e:
                    print("Joint not detected")
                    pass

                if self.output_img_port.getOutputCount():
                    self.write_yarp_image(canvas)

        return True

    ##########################################################################
    def send_event(self, event):
        """
        Send event to the event:o port
        @param state: event to send
        @type state: string
        @return:
        @rtype:
        """
        if self.eventOutputPort.getOutputCount():
            cmd_bottle = self.eventOutputPort.prepare()
            cmd_bottle.clear()
            cmd_bottle.addString(event)
            self.eventOutputPort.write()

    def write_yarp_image(self, frame):
        """
        Handle function to stream the recognize faces with their bounding rectangles
        :param img_array:
        :return:
        """
        cv2.cvtColor(frame, cv2.COLOR_BGR2RGB, frame)
        display_buf_image = self.output_img_port.prepare()
        display_buf_image.resize(800, 800)
        display_buf_image.setExternal(frame.tobytes(), 800, 800)
        self.output_img_port.write()


    def format_yarp_image(self, yarp_img):
        """
        Format a yarp image to openCV Mat
        :param yarp_img:
        :return: openCV::Mat
        """
        yarp_img.setExternal(self.input_img_array, self.width_img, self.height_img)
        frame = np.frombuffer(self.input_img_array, dtype=np.uint8).reshape(
            (self.height_img, self.width_img, 3)).copy()

        cv2.cvtColor(frame, cv2.COLOR_RGB2BGR, frame)

        return frame

    def show_yoga_pose(self, yoga_pose_name):
        """
        Send command for the interaction interface module
        :param yoga_pose_name:
        :return: None
        """
        cmd_bottle = self.cmd_port.prepare()
        cmd_bottle.clear()
        cmd_bottle.addString("exe")
        cmd_bottle.addString(yoga_pose_name)
        self.cmd_port.write()

    def write_iSpeak(self, msg):
        """
        Send text to the speak module
        :param msg:
        :return: None
        """
        speak_bottle = self.iSpeak.prepare()
        speak_bottle.clear()
        speak_bottle.addString(msg)
        self.iSpeak.write()


if __name__ == '__main__':

    # Initialise YARP
    if not yarp.Network.checkNetwork():
        print("Unable to find a yarp server exiting ...")
        sys.exit(1)

    yarp.Network.init()
    yogaTeacherModule = YogaModule()

    rf = yarp.ResourceFinder()
    rf.setVerbose(True)
    rf.setDefaultContext('YogaTeacher')
    rf.setDefaultConfigFile('YogaTeacher.ini')

    if rf.configure(sys.argv):
        yogaTeacherModule.runModule(rf)

    sys.exit()
