import json
import glob, os
import time
import numpy as np
from util import JOINT_NAMES_IT


class YogaTrainer(object):

    def __init__(self, yoga_data_path):
        self.root_dir = yoga_data_path
        self.yoga_poses = {}
        self.yoga_feedbacks = {}
        self.yoga_pers_feedbacks = {}
        self.yoga_show_correction = {}

        self._load_yoga_poses()

        self.joint_angles_errors = {}
        self.user = None
        self.current_level = 1
        self.pose_index = 0
        self.total_pose_index = 0
        self.n_poses_session = 5

        # Testing phase parameters
        self._start_timer = None
        self.ctr_pose = 0
        self.ctr_pose_success = 0

    def init_params(self):
        self.ctr_pose = 0
        self.ctr_pose_success = 0
        self._start_timer = time.time()

    def set_user_profile(self, current_user):
        self.user = current_user
        self.current_level = self.user.skill_level
        print("User Level is: {}".format(self.current_level))

    def _load_yoga_poses(self):
        list_dir = os.listdir(os.path.join(self.root_dir, "yoga_poses"))

        for current_dir in list_dir:
            level = int(current_dir.split("_")[1])
            self.yoga_poses[level] = {}
            self.yoga_feedbacks[level] = {}
            self.yoga_pers_feedbacks[level] = {}
            yoga_files = glob.glob(os.path.join(self.root_dir, "yoga_poses", current_dir, "*.txt"))
            for f in yoga_files:
                pose_name = f.split(".txt")[0].split('/')[-1]
                yoga_dict = json.load(open(f))
                self.yoga_poses[level][pose_name] = yoga_dict["value"]
                self.yoga_feedbacks[level][pose_name] = yoga_dict["ins"]
                self.yoga_pers_feedbacks[level][pose_name] = yoga_dict["critical_joints"]

    def evaluate_yoga_pose(self, current_pose, target_pose):
        diff_angles_l = {}
        max_diff_joint = 1e-1
        joint_to_fix = None

        if self.user.known_user:
            pose_name = self.get_pose_name()
            self.user.personalize_threshold(pose_name)

        for name_joint, angle in target_pose.items():
            diff = abs(angle) - abs(current_pose[name_joint])
            diff_angles_l[name_joint] = diff

            if self.user.threshold < abs(diff) > max_diff_joint:
                max_diff_joint = abs(diff)
                joint_to_fix = name_joint

        return diff_angles_l, joint_to_fix

    def give_generic_evaluation(self, current_pose):
        performance = 0
        self.ctr_pose += 1
        target_pose_name, target_pose = self.get_pose()
        elapsed_time = time.time() - self._start_timer

        joints_diff, joint_to_fix = self.evaluate_yoga_pose(current_pose, target_pose)

        if joint_to_fix is None:
            self.ctr_pose_success += 1

        if elapsed_time >= self.user.period:
            performance = (self.ctr_pose_success/self.ctr_pose)*100
            print(performance)

            if performance > 70:
                msg = "Molto bene! Complimenti!"
            elif performance < 70 and performance > 40:
                msg = "Non male, ma potete fare di meglio!"
            else:
                msg = "AI ai ai! Non ci siamo affatto!"
        else:
            msg = ""

        return msg

    def get_personalize_feedback(self):
        feedback_msg = ""
        pose_name = self.get_pose_name()
        if pose_name in self.user.user_dict: 
            max_critical_joint = self.user.get_critical_joints(pose_name)
            if max_critical_joint:
                action_msg = self.yoga_pers_feedbacks[self.current_level][pose_name][max_critical_joint][0]
                feedback_msg = "Miraccomando! Questa volta fai particolare attenzione a " + f"{action_msg}"
            else:
                feedback_msg = "L'ultima volta sei stato molto bravo! Vediamo come va oggi!"

        return feedback_msg

    def get_pose_feedback(self, current_pose):
        target_pose_name, target_pose = self.get_pose()
        diff_angles_l, joint_to_fix = self.evaluate_yoga_pose(current_pose, target_pose)

        if joint_to_fix is None:
            self.ctr_pose_success += 1
            return "Bravissimo"
        else:
            action_msg = self.yoga_feedbacks[self.current_level][target_pose_name][joint_to_fix][0]
            return f"{action_msg}"

    def update_exercise(self):
        if self.total_pose_index >= self.n_poses_session:
            print("Poses finished")
        elif self.pose_index == len(self.yoga_poses[self.current_level]) - 1:  # 2
            self.pose_index = 0
            self.total_pose_index += 1
            self.current_level += 1
        else:
            self.pose_index += 1
            self.total_pose_index += 1

    def test_pose(self, current_pose):
        self.ctr_pose += 1
        target_pose_name, target_pose = self.get_pose()
        elapsed_time = time.time() - self._start_timer

        joints_diff, joint_to_fix = self.evaluate_yoga_pose(current_pose, target_pose)
        self.user.add_joints(joints_diff)

        if joint_to_fix is None:
            self.ctr_pose_success += 1

        if elapsed_time >= self.user.period:
            self.user.save_performance(target_pose_name, self.current_level, self.ctr_pose_success, self.ctr_pose)
            return False

        return True

    def get_pose(self):
        pose_name = list(self.yoga_poses[self.current_level].keys())[self.pose_index]
        pose_values = self.yoga_poses[self.current_level][pose_name]

        return pose_name, pose_values

    def get_pose_name(self):
        return list(self.yoga_poses[self.current_level].keys())[self.pose_index]

