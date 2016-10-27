import rospy
import sys
# Brings in the SimpleActionClient
import actionlib
import topological_navigation.msg
import scitos_ptu.msg
from sensor_msgs.msg import JointState
import tf2_ros
import tf, tf2_msgs.msg
from geometry_msgs.msg import PointStamped, Pose, Transform, TransformStamped, Vector3, Quaternion
import math

def reset_gaze():
    rospy.loginfo("Trying to reset gaze")
    ptuClient = actionlib.SimpleActionClient('ResetPtu',scitos_ptu.msg.PtuResetAction)
    ptuClient.wait_for_server()

    goal = scitos_ptu.msg.PtuResetGoal()
    #goal.pan = 0
    #goal.tilt = 0
    #goal.pan_vel = 20
    #goal.tilt_vel = 20
    ptuClient.send_goal(goal)
    ptuClient.wait_for_result()

def look_at_point(pan,tilt):
    ptuClient = actionlib.SimpleActionClient('SetPTUState',scitos_ptu.msg.PtuGotoAction)
    ptuClient.wait_for_server()

    goal = scitos_ptu.msg.PtuGotoGoal()
    goal.tilt = tilt
    goal.tilt_vel = 0.1


    goal.pan = pan
    goal.pan_vel = 0.1

    ptuClient.send_goal(goal)
    ptuClient.wait_for_result()

def transform_target_point(target):
    pan_ref_frame = '/head_xtion_link'
    tilt_ref_frame = '/head_xtion_link'

    tfs = tf.TransformListener()
    # Wait for tf info (timeout in 5 seconds)
    tfs.waitForTransform(pan_ref_frame, target.header.frame_id, rospy.Time(), rospy.Duration(5.0))
    tfs.waitForTransform(tilt_ref_frame, target.header.frame_id, rospy.Time(), rospy.Duration(5.0))

    # Transform target point to pan reference frame & retrieve the pan angle
    pan_target = tfs.transformPoint(pan_ref_frame, target)
    pan_angle = math.atan2(pan_target.point.y, pan_target.point.x)
    print("pan angle: " + str(pan_angle))

    # Transform target point to tilt reference frame & retrieve the tilt angle
    tilt_target = tfs.transformPoint(tilt_ref_frame, target)
    tilt_angle = math.atan2(-tilt_target.point.z,
            math.sqrt(math.pow(tilt_target.point.x, 2) + math.pow(tilt_target.point.y, 2)))

    cur_ptu_state = rospy.wait_for_message("/ptu/state",  JointState, timeout=10)
    print("cur ptu state: " + str(cur_ptu_state.position))
    current_head_pan = cur_ptu_state.position[0]
    current_head_tilt = cur_ptu_state.position[1]

    return [math.degrees(current_head_pan+pan_angle), math.degrees(current_head_tilt+tilt_angle)]

if __name__ == '__main__':
    rospy.init_node('nudge_ptu', anonymous = True)
    reset_gaze()
    pt_s = PointStamped()
    pt_s.header.frame_id = "/map"

    # behind robot
    pt_s.point.x = 5.22
    pt_s.point.y = 2.98
    pt_s.point.z = 0.7515

    # in front of robot
    #pt_s.point.x = 9.09
    #pt_s.point.y = 2.94
    #pt_s.point.z = 1.62

    # to the side of the robot
    #pt_s.point.x = 7.38
    #pt_s.point.y = 1.74
    #pt_s.point.z = 4.19


    tar = transform_target_point(pt_s)
    print("target deltas:" + str(tar))
    look_at_point(tar[0],tar[1])