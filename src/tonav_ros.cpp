#include "tonav_ros.h"

#include <cv_bridge/cv_bridge.h>
#include <Eigen/Core>
#include <image_transport/image_transport.h>
#include <ros/ros.h>
#include <sensor_msgs/Imu.h>
#include <sensor_msgs/CameraInfo.h>

#include "calibration.h"

TonavRos::TonavRos() {
    current_camera_matrix_.setZero();
    current_distirtion_params_.setZero();
}

int TonavRos::run(int argc, char* argv[]) {
    ROS_INFO_STREAM("Tonav ROS - tomas789@gmail.com \n");
    ros::init(argc, argv, "tonav_ros");
    if (!parseCommandLineParams(argc, argv)) {
        printHelp();
        return 1;
    }
    if (variables_map_.count("help")) {
        printHelp();
        return 1;
    }
    ros::NodeHandle node_handle;
    
    std::string imu_topic = variables_map_["imu"].as<std::string>();
    std::string camera_topic = variables_map_["image"].as<std::string>();
    std::string camerainfo_topic = variables_map_["camerainfo"].as<std::string>();
    
    image_transport::ImageTransport transport(node_handle);
    image_transport::Subscriber camera_subscriber = transport.subscribe(camera_topic, 5, &TonavRos::cameraCallback, this);
    
    ros::Subscriber camerainfo_subscriber = node_handle.subscribe(camerainfo_topic, 50, &TonavRos::cameraInfoCallback, this);
    
    ros::Subscriber imu_subscriber = node_handle.subscribe(imu_topic, 50, &TonavRos::imuCallback, this);
    
    ros::spin();
    
    return 0;
}

void TonavRos::setAllowedOptionsDescription() {
    options_description_.add_options()
        ("help,h", "print help message")
        ("calibration,c", po::value<std::string>(), "calibration file")
        ("image", po::value<std::string>(), "image topic")
        ("camerainfo", po::value<std::string>(), "camera_info topic")
        ("imu", po::value<std::string>(), "imu topic")
    ;
}

bool TonavRos::parseCommandLineParams(int argc, char* argv[]) {
    setAllowedOptionsDescription();
    try {
        auto parsed = po::parse_command_line(argc, argv, options_description_);
        po::store(parsed, variables_map_);
    } catch (const po::error& e) {
        ROS_ERROR_STREAM(e.what());
        return false;
    }
    
    po::notify(variables_map_);
    
    if (variables_map_.count("image") == 0) {
        ROS_ERROR("Argument image is required");
        return false;
    } else if (variables_map_.count("image") > 1) {
        ROS_ERROR("Got multiple occurrences of image. Expected one.");
        return false;
    } else if (variables_map_.count("camerainfo") == 0) {
        ROS_ERROR("Argument camerainfo is required");
        return false;
    } else if (variables_map_.count("camerainfo") > 1) {
        ROS_ERROR("Got multiple occurrences of camerainfo. Expected one.");
        return false;
    } else if (variables_map_.count("imu") == 0) {
        ROS_ERROR("Argument imu is required");
        return false;
    } else if (variables_map_.count("imu") > 1) {
        ROS_ERROR("Got multiple imu of camerainfo. Expected one.");
        return false;
    }
    
    return true;
}

void TonavRos::printHelp() {
    ROS_INFO_STREAM(options_description_);
}

void TonavRos::cameraCallback(const sensor_msgs::ImageConstPtr &msg) {
    ROS_INFO_STREAM("Camera Seq: [" << msg->header.seq << "]");
    try {
        const std_msgs::Header header = msg->header;
        double timestamp = getMessageTime(header.stamp);
        cv::Mat gray = cv_bridge::toCvShare(msg, "mono8")->image;
        tonav_->updateImage(timestamp, gray);
    } catch (cv_bridge::Exception& e) {
        ROS_ERROR("Could not convert from '%s' to 'mono8'.", msg->encoding.c_str());
    }
}

void TonavRos::cameraInfoCallback(const sensor_msgs::CameraInfoConstPtr &msg) {
    ROS_INFO_STREAM("CameraInfo Seq: [" << msg->header.seq << "]");
    for (std::size_t i = 0; i < 9; ++i) {
        std::size_t row = i / 3;
        std::size_t col = i % 3;
        current_camera_matrix_(row, col) = msg->K[i];
    }
    
    for (std::size_t i = 0; i < 5; ++i) {
        current_distirtion_params_(i, 0) = msg->D[i];
    }
}

void TonavRos::imuCallback(const sensor_msgs::ImuConstPtr &msg) {
    ROS_INFO_STREAM("Imu Seq: [" << msg->header.seq << "]");
    const std_msgs::Header header = msg->header;
    double timestamp = getMessageTime(header.stamp);
    
    Eigen::Vector3d angular_velocity;
    angular_velocity << msg->angular_velocity.x, msg->angular_velocity.y, msg->angular_velocity.z;
    Eigen::Vector3d linear_acceleration;
    linear_acceleration << msg->linear_acceleration.x, msg->linear_acceleration.y, msg->linear_acceleration.z;
    
    tonav_->updateAccelerationAndRotationRate(timestamp, angular_velocity, linear_acceleration);
}

double TonavRos::getMessageTime(ros::Time stamp) {
    if (time_beginning_.isZero()) {
        time_beginning_ = stamp;
    }
    ros::Duration duration = stamp - time_beginning_;
    return duration.toSec();
}