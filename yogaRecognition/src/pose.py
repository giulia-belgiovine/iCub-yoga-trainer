import pickle
from yogaRecognition.src.util import getAngle


class Pose(object):

    def __init__(self):
        self.angle_dict = {"neck": ["neck", "nose"], "lshoulder": ["neck", "lshoulder"], "rshoulder": ["neck", "rshoulder"],
                      "lelbow": ["lelbow", "lshoulder"], "relbow": ["relbow", "rshoulder"],
                      "lknee": ["lknee", "lhip"], "rknee": ["rknee", "rhip"], "rwrist": ["rwrist", "relbow"],
                           "lwrist": ["lwrist", "lelbow"]}

        self.joint_dictionary =  {"nose": 0, "neck": 1, "lshoulder": 5, "rshoulder": 2, "lelbow": 6, "relbow": 3, "lwrist": 4,
                            "rwrist": 7, "rhip": 8, "lhip": 11, "lknee": 12, "rknee": 9}
        self.yoga_angles_dict = {}

    def __call__(self, joint_values):

        for name_angle, parts_angle in self.angle_dict.items():
            root_joint = self.joint_dictionary[parts_angle[0]]
            target_joint = self.joint_dictionary[parts_angle[1]]
            self.yoga_angles_dict[name_angle] = getAngle(joint_values[root_joint][0],
                                                         joint_values[root_joint][1],
                                                         joint_values[target_joint][0],
                                                         joint_values[target_joint][1])
        #print("angle dict", self.yoga_angles_dict)
        return self.yoga_angles_dict


if __name__ == "__main__":

    pose = Pose()
    # load joint angles for testing
    a_file = open("joint_example.pkl", "rb")
    joint_test = pickle.load(a_file)
    a_file.close()

    yoga_pose_detected = pose(joint_test)
    print(yoga_pose_detected)

