#include "sensors.h"

#include <geometry_msgs/Pose.h>
#include <geometry_msgs/Quaternion.h>
#include <tf2_ros/transform_listener.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.h> //--needed for tf2::Matrix3x3

#include <nist_gear/Model.h>

Sensors::Sensors(ros::NodeHandle* nodehandle, const std::string &id): 
  m_nh{*nodehandle}, 
  m_id{id} {}

Sensors::~Sensors() {}

LogicalCamera::LogicalCamera(ros::NodeHandle* nodehandle, const std::string& id): 
  Sensors(nodehandle, id)
{
    m_sensor_subscriber = m_nh.subscribe("/ariac/" + id, 10, &LogicalCamera::sensor_callback, this); 

    tf2_ros::Buffer tfBuffer;
    tf2_ros::TransformListener tfListener(tfBuffer);

    ros::Duration timeout(5.0);
    try {
      m_camera_frame = tfBuffer.lookupTransform("world", id + "_frame",
                                                   ros::Time(0), timeout);
    }
    catch (tf2::TransformException& ex) {
      ROS_WARN("%s", ex.what());
      ros::Duration(1.0).sleep();
      return; 
    }

    // m_camera_q_tf = tf2::Quaternion(
    //   transformStamped.transform.rotation.x,
    //   transformStamped.transform.rotation.y,
    //   transformStamped.transform.rotation.z,
    //   transformStamped.transform.rotation.w);
    //
}

void LogicalCamera::sensor_callback(const nist_gear::LogicalCameraImage::ConstPtr& msg)
{
  // const std::lock_guard<std::mutex> lock(*m_mutex_ptr); 
  m_parts_camera_frame.clear(); 
  for (auto& model: msg->models){
    m_parts_camera_frame.emplace_back(std::make_unique<nist_gear::Model>(model)); 
  }
    //   // ROS_INFO("%s in /world frame: [%f,%f,%f]",
  //   //     transformed_model.type.c_str(), 
  //   //     transformed_model.pose.position.x,
  //   //     transformed_model.pose.position.y,
  //   //     transformed_model.pose.position.z); 
  //
  // }
    
}

void LogicalCamera::camera_to_world()
{
  m_parts_world_frame.clear(); 
  for (auto &model: m_parts_camera_frame) {
    geometry_msgs::Pose model_pose = model->pose; 
    geometry_msgs::Pose transformed_model_pose; 

    tf2::doTransform(model_pose, transformed_model_pose, m_camera_frame); 

    nist_gear::Model transformed_model; 
    transformed_model.type = model->type; 
    transformed_model.pose = transformed_model_pose; 
    m_parts_world_frame.emplace_back(std::make_unique<nist_gear::Model>(transformed_model)); 
  }
}

int LogicalCamera::find_parts(const std::string& product_type)
{
  this->camera_to_world(); 
  // const std::lock_guard<std::mutex> lock(*m_mutex_ptr); 

  int count = 0; 
  for (auto &part: m_parts_world_frame){
    // ROS_INFO("%s", part->type.c_str()); 
    if (part->type == product_type) {
      // calculate orientation in world frame
      tf2::Quaternion part_q_tf;
      tf2::convert(part->pose.orientation, part_q_tf); 
      part_q_tf.normalize(); 

      //convert Quaternion to Euler angles
      tf2::Matrix3x3 m(part_q_tf);
      double roll, pitch, yaw;
      m.getRPY(roll, pitch, yaw);

      ROS_INFO("%s in /world frame: [%f,%f,%f] [%f,%f,%f]",
        part->type.c_str(), 
        part->pose.position.x,
        part->pose.position.y,
        part->pose.position.z,
        roll,
        pitch,
        yaw);

      count++; 
    }
  }
  return count; 
}

DepthCamera::DepthCamera(ros::NodeHandle* nodehandle, const std::string& id):
  Sensors(nodehandle, id)
{
    m_sensor_subscriber = m_nh.subscribe("/ariac/" + id, 10, &DepthCamera::sensor_callback, this); 
}

void DepthCamera::sensor_callback(const sensor_msgs::PointCloud::ConstPtr& msg)
{
   //ROS_INFO_STREAM_THROTTLE(10, m_id);
   //ROS_INFO_THROTTLE(1, "Callback triggered for Topic %s", m_id.c_str());
   return; 
}

ProximitySensor::ProximitySensor(ros::NodeHandle* nodehandle, const std::string& id): 
  Sensors(nodehandle, id)
{
    m_sensor_subscriber = m_nh.subscribe("/ariac/" + id, 10, &ProximitySensor::sensor_callback, this); 
}

void ProximitySensor::sensor_callback(const sensor_msgs::Range::ConstPtr& msg)
{
  ROS_INFO_THROTTLE(1, "Callback triggered for Topic %s", m_id.c_str());
  if ((msg->max_range - msg->range) > 0.01) {
    // If there is an object in proximity.
    ROS_INFO_THROTTLE(1, "%s: sensed object", m_id.c_str());
  }
}

LaserProfiler::LaserProfiler(ros::NodeHandle* nodehandle, const std::string& id): 
  Sensors(nodehandle, id)
{
    m_sensor_subscriber = m_nh.subscribe("/ariac/" + id, 10, &LaserProfiler::sensor_callback, this); 
}

void LaserProfiler::sensor_callback(const sensor_msgs::LaserScan::ConstPtr& msg)
{
  // size_t number_of_valid_ranges = std::count_if(
  //   msg->ranges.begin(), msg->ranges.end(), [](const float f)
  //   {
  //     return std::isfinite(f);
  //   });
  // if (number_of_valid_ranges > 0)
  // {
  //   ROS_INFO_THROTTLE(1, "%s: sensed object", m_id.c_str());
  // }
  ROS_INFO_THROTTLE(1, "Callback triggered for Topic %s", m_id.c_str());
}

BreakBeam::BreakBeam(ros::NodeHandle* nodehandle, const std::string& id): 
  Sensors(nodehandle, id)
{
    m_sensor_subscriber = m_nh.subscribe("/ariac/" + id, 10, &BreakBeam::sensor_callback, this); 
    m_sensor_change_subscriber = m_nh.subscribe("/ariac/" + id + "_change", 10, &BreakBeam::sensor_change_callback, this); 
}

void BreakBeam::sensor_callback(const nist_gear::Proximity::ConstPtr& msg)
{
    ROS_INFO_THROTTLE(1, "Callback triggered for Topic %s", m_id.c_str());
    if (msg->object_detected) {  // If there is an object in proximity.
      ROS_INFO("%s triggered.", m_id.c_str());
    }
}

void BreakBeam::sensor_change_callback(const nist_gear::Proximity::ConstPtr& msg)
{
    ROS_INFO_THROTTLE(1, "Callback triggered for Topic %s", m_id.c_str());
    if (msg->object_detected) {  // If there is an object in proximity.
      ROS_INFO("%s triggered.", (m_id + "_change").c_str());
    }
}
