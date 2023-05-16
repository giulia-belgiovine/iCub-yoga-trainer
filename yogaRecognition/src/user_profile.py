import os
import pandas as pd
import numpy as np

class UserProfile(object):

    def __init__(self, user_name, log_path):

        self.user_name = user_name
        self.user_dict = {}
        self.known_user = False
        self.threshold = 15 #12
        self.period = 20
        self.performance = 0
        self.row = 0
        self.skill_level = 1
        self.skill_level_thr = 35
        self.skill_level_var = 45
        self.user_df = pd.DataFrame(columns=['pose', 'level', 'threshold', 'period',  'performance', 'fps'])

        self.user_profile_path = log_path
        self.folder_name2 = ""

        self._initialize_test()
        self.columns_joints = ["neck", "lshoulder", "rshoulder", "lelbow", "relbow", "lknee", "rknee"]
        self.joint_df = pd.DataFrame(columns=self.columns_joints )

    def _initialize_test(self):
        if not os.path.exists(os.path.join(self.user_profile_path, self.user_name)):
            os.makedirs(os.path.join(self.user_profile_path, self.user_name))
        else:
            print("loading log of {}".format(self.user_name))
            self.folder_name2 = self.user_name + "_session2"
            if not os.path.exists(os.path.join(self.user_profile_path, self.folder_name2)):
            	os.makedirs(os.path.join(self.user_profile_path, self.folder_name2))
            self.load_log()
            self.set_skill_level()
            self.known_user = True


    def set_skill_level(self):
        df = self.user_dict["general"]
        if np.mean(df.accuracy[df.level == 1]) > self.skill_level_thr and np.std(df.accuracy[df.level == 1]) < self.skill_level_var:
            self.skill_level = 2
        else:
            self.skill_level = 1

    def personalize_threshold(self, pose_name):
        df = self.user_dict["general"]
        val = df.accuracy[df.pose.values == pose_name]
        if not val.empty and val.values[0] > 80:
            self.threshold = 8

        print("[DEBUG] Pose is: {}, threshold is: {}".format(pose_name, self.threshold))

    def save_performance(self, pose_name, level, balance, fps):
        df_tmp = pd.DataFrame.from_dict({"pose": [pose_name], "level": [level], "threshold": [self.threshold],
                                         "period": [self.period], "performance": [balance], "fps": [fps]})
        self.user_df = self.user_df.append(df_tmp, ignore_index=True)
        if self.known_user:
            self.user_df.to_csv(os.path.join(self.user_profile_path, self.folder_name2, "log.csv"))
        else:
            self.user_df.to_csv(os.path.join(self.user_profile_path, self.user_name, "log.csv"))

        self.save_joints(pose_name)

    def add_joints(self, joint_diff):
        df_tmp = pd.DataFrame(joint_diff, index=[0])
        self.joint_df = self.joint_df.append(df_tmp, ignore_index=True)

    def save_joints(self, pose_name):
        if self.known_user:
            self.joint_df.to_csv(os.path.join(self.user_profile_path, self.folder_name2, f"{pose_name}.csv"))
        else:
            self.joint_df.to_csv(os.path.join(self.user_profile_path, self.user_name, f"{pose_name}.csv"))
        self.joint_df = pd.DataFrame(columns=self.columns_joints)

    def load_log(self):
        for pose_file in os.listdir(os.path.join(self.user_profile_path, self.user_name)):
            if pose_file != "log.csv":
                df_pose = pd.read_csv(os.path.join(self.user_profile_path, self.user_name, pose_file))
                pose_name = pose_file.split("_pose")[0]
                self.user_dict[pose_name] = df_pose
            else:
                df_user = pd.read_csv(os.path.join(self.user_profile_path, self.user_name, "log.csv"))
                df_user["accuracy"] = (df_user["performance"] / df_user["fps"]) * 100
                self.user_dict["general"] = df_user

    def get_critical_joints(self, pose):
        #joints_to_correct = []
        pose_name = pose.split("_pose")[0]   
 
        df = self.user_dict[pose_name]
        max_critical_joint_val = 10
        max_critical_joint = ""

        for joint in df.columns[1:]:
            if np.mean(abs(df[joint])) > max_critical_joint_val:
                #joints_to_correct.append(joint)
                max_critical_joint_val = np.mean(abs(df[joint]))
                max_critical_joint = joint

        return max_critical_joint
