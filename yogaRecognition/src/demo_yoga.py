import cv2
import matplotlib.pyplot as plt
import copy
import numpy as np
import os
import csv
import pickle
import torch
import json
import glob

from yogaRecognition.src import util
from yogaRecognition.src.body import Body
from yogaRecognition.src.pose import Pose
from yogaRecognition.src.user_profile import UserProfile
from trainer import YogaTrainer

ROOT_PATH = "/home/giulia/Projects/iCubYogaTeacher/"
MODEL_PATH = "/home/giulia/Projects/iCubYogaTeacher/model/body_pose_model.pth"

columns_profile = ["Exercise", "Level", "Accuracy", "Balance"]

def main():

    yoga_trainer = YogaTrainer(ROOT_PATH)
    user_profiler = UserProfile("Giulia", known_user=False)
    body_estimation = Body(MODEL_PATH)
    pose = Pose()

    pose_index = 0
    n_poses = 3
    #print(f"Torch device: {torch.cuda.get_device_name()}")

    cap = cv2.VideoCapture(0)
    cap.set(3, 320)
    cap.set(4, 240)

    level = user_profiler.initialize_training('unknown')

    filename = user_profiler.set_filename()
    file_exists = os.path.isfile(filename)
    with open(filename, 'w') as csv_file:
        writer = csv.DictWriter(csv_file, delimiter=',', lineterminator='\n', fieldnames=columns_profile)
        if not file_exists:
            writer.writeheader()

        name_target_pose, target_pose = yoga_trainer.select_pose(level, pose_index)
        threshold, period = yoga_trainer.set_exercise_parameters(level)

        while True:

            ret, oriImg = cap.read()
            candidate, subset = body_estimation(oriImg)
            canvas = copy.deepcopy(oriImg)
            canvas, joint_values = util.draw_bodypose(canvas, candidate, subset)
            canvas = cv2.resize(canvas, (800, 800), interpolation=cv2.INTER_AREA)

            canvas = cv2.putText(canvas, f'Do the {name_target_pose}', (20, 50), cv2.FONT_HERSHEY_SIMPLEX,
                                 1, (255, 0, 0), 2, cv2.LINE_AA)

            try:
                current_pose = pose(joint_values)
                diff_angles = yoga_trainer.evaluate_yoga_pose(current_pose, target_pose)
                performance = yoga_trainer.compute_performance(diff_angles)
                angles_to_check = yoga_trainer.fix_joints_angle(diff_angles, threshold)

                if performance < threshold:
                    balance = yoga_trainer.check_period_pose(period, current_pose, target_pose)
                    if balance == 1:
                        canvas = cv2.putText(canvas, 'GOOD JOB', (400, 50), cv2.FONT_HERSHEY_SIMPLEX,
                                             1, (255, 0, 0), 2, cv2.LINE_AA)
                        pose_index += 1
                else:
                    canvas = cv2.putText(canvas, f'Fix your {angles_to_check}', (20, 50), cv2.FONT_HERSHEY_SIMPLEX,
                                         1, (255, 0, 0), 2, cv2.LINE_AA)

                row = {'Exercise': name_target_pose, 'Level': level, 'Accuracy': performance, 'Balance': balance}
                writer.writerow(row)

                if pose_index == n_poses and level <= 3:
                    pose_index = 0
                    level += 1
                    threshold, period = yoga_trainer.set_exercise_parameters(level)

                if level > 3:
                    print("Exercise Complete!")
                    csv_file.close()

                name_target_pose, target_pose = yoga_trainer.select_pose(level, pose_index)

            except KeyError as e:
                print(e)
                print("Joint not detected")
                pass

            cv2.imshow('demo', canvas)
            if cv2.waitKey(1) & 0xFF == ord('q'):
                break

        cap.release()
        cv2.destroyAllWindows()


if __name__ == "__main__":
    main()