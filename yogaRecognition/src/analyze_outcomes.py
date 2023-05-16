import os
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

plt.style.use('ggplot')

LOG_PATH = "/media/icub/nas/cognitive_interactions/Experiments/2021/Giulia/yogaExp/user_profile"

def load_log():
    all_user = []
    user_dict = {}
    for subj in os.listdir(LOG_PATH):
        user_dict[subj] = {}
        for pose_file in os.listdir(os.path.join(LOG_PATH,subj)):
            if pose_file != "log.csv":
                df_pose = pd.read_csv(os.path.join(LOG_PATH, subj, pose_file))
                pose_name = pose_file.split("_pose")[0]
                user_dict[subj][pose_name] = df_pose
            else:
                df_user = pd.read_csv(os.path.join(LOG_PATH, subj, "log.csv"))
                df_user["accuracy"] = (df_user["performance"]/df_user["fps"])*100

                user_dict[subj]["general"] = df_user

    return user_dict


def visualize_performance(user_name):

    col_dict = {1: 'green', 2: 'orange', 3: 'red'}
    df = user_dict[user_name]['general']
    x = [el.split("_pose")[0] for el in df["pose"]]
    accuracy = df["accuracy"].tolist()
    level = df["level"].to_list()
    x_pos = [i for i, _ in enumerate(x)]

    plt.bar(x_pos, accuracy, color=[col_dict[k] for k in level])
    plt.xlabel("Yoga Poses", fontsize=12)
    plt.ylabel("Accuracy [%]", fontsize=12)
    plt.title("{} performance".format(user_name))

    plt.xticks(x_pos, x, fontsize=9)

    plt.show()

    return accuracy


def visualize_joints_error(user_name):

    for pose in user_dict[name].keys():
        if pose != "general":
            df = user_dict[user_name][pose]
            for joint in df.columns[1:]:
                plt.plot(abs(df[joint]), label=joint)
            plt.axhline(y=10, color='b', linestyle='-',  linewidth=3)
            plt.ylabel('Angles')
            plt.xlabel("Frames")
            plt.title("Pose: {}, User: {}".format(pose, user_name))
            plt.legend()
            plt.show()

def set_skill_level(user_name):
    df = user_dict[user_name]["general"]
    if np.mean(df.accuracy[df.level == 1]) > 60 and np.std(df.accuracy[df.level == 1]) < 20:
        skill_level = 2
    else:
        skill_level = 1
    print(skill_level)

    return skill_level

def set_threshold(user_name, pose_name):
    df = user_dict[user_name]["general"]
    val = df.accuracy[df.pose.values == pose_name]
    if 80 > val.values[0] > 60:
        threshold = 10
    elif val.values[0] > 80:
        threshold = 8
    else:
        threshold = 15
    print(pose_name, threshold)
    return threshold




if __name__ == "__main__":
    user_dict = load_log()
    visualize_performance("Fabio2")
    skill_level = set_skill_level("Fabio2")









