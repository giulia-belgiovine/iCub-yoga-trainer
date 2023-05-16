import cv2
import matplotlib.pyplot as plt
import copy
import numpy as np
import os

from yogaRecognition.src import util
from yogaRecognition.src.body import Body
import json
import glob

MODEL_PATH = "/home/giulia/Projects/iCubYogaTeacher/model/body_pose_model.pth"
ROOT_PATH = "/home/giulia/Projects/iCubYogaTeacher/"

body_estimation = Body(MODEL_PATH)
list_dir = os.listdir(os.path.join(ROOT_PATH, "yoga_poses"))

for current_dir in list_dir:
    for image in (os.listdir(os.path.join(ROOT_PATH, "yoga_poses", current_dir))):
        if image.endswith('.jpeg'):
            oriImg = cv2.imread(os.path.join(ROOT_PATH, "yoga_poses", current_dir, image)) # B,G,R order
            candidate, subset = body_estimation(oriImg)
            canvas = copy.deepcopy(oriImg)
            canvas, joint_values = util.draw_bodypose(canvas, candidate, subset)

            yoga_angle = util.get_yoga_angles(joint_values)
            json.dump(yoga_angle, open("{}.txt".format(os.path.join(ROOT_PATH, "yoga_poses", current_dir,image).split(".")[0]), 'w'))
            #cv2.imwrite('test_image.png', canvas)
