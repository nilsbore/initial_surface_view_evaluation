#include <cstdlib>
#include <stdlib.h>
#include <string>
#include <math.h>
#include <ros/ros.h>
#include <geometry_msgs/Pose.h>
#include <geometry_msgs/Point.h>
#include <nav_msgs/OccupancyGrid.h>
#include <sensor_msgs/PointCloud2.h>
#include <sensor_msgs/point_cloud_conversion.h>
#include <pcl/filters/radius_outlier_removal.h>
#include <pcl/filters/conditional_removal.h>

#include <octomap_ros/conversions.h>
#include <octomap_msgs/Octomap.h>
#include <octomap_msgs/conversions.h>
#include <pcl_conversions/pcl_conversions.h>
#include <pcl/point_types.h>
#include <pcl/PCLPointCloud2.h>
#include <pcl/conversions.h>
#include <pcl_ros/transforms.h>
#include <pcl/io/pcd_io.h>
#include <pcl/filters/statistical_outlier_removal.h>

#include "initial_surface_view_evaluation/CalculateOctreeOverlap.h"
#include "initial_surface_view_evaluation/ConvertCloudToOctomap.h"
#include "initial_surface_view_evaluation/ExtractNormalsFromOctomap.h"

#include "semantic_map_publisher/ObservationOctomapService.h"
#include "surface_based_object_learning/PointInROI.h"

#include <geometry_msgs/PointStamped.h>
#include <tf/transform_listener.h>

using namespace std;
using namespace octomap;
using namespace sensor_msgs;

#define ANGLE_MAX_DIFF (M_PI / 4)

octomap_msgs::Octomap convert_pcd_to_octomap(std::vector<sensor_msgs::PointCloud2> input_clouds)
{
  ROS_INFO("Converting PointCloud to Octree");
  ros::NodeHandle n;
  // when i was your age, we used strongly typed languages
  // "what's a type, grandad?"
  // well, let me show you
  float octree_resolution = 0.03f;
  octomap::OcTree map(octree_resolution);
  ros::Publisher octomap_pub = n.advertise<octomap_msgs::Octomap>("/initial_surface_view_evaluation/converted_octomaps", 5);

  geometry_msgs::Pose robot_pose = *ros::topic::waitForMessage<geometry_msgs::Pose>("/robot_pose", ros::Duration(5));

  // little bit of a hack, this is just a guesstimate of how high up the PTU is
  robot_pose.position.z = 1.76;


  for(std::vector<sensor_msgs::PointCloud2>::iterator iter = input_clouds.begin(), end = input_clouds.end(); iter != end; ++iter) {
      sensor_msgs::PointCloud2 input_cloud = *iter;

      pcl::PCLPointCloud2 pcl_pc2;
      pcl_conversions::toPCL(input_cloud,pcl_pc2);
      pcl::PointCloud<pcl::PointXYZ>::Ptr temp_cloud(new pcl::PointCloud<pcl::PointXYZ>);
      pcl::fromPCLPointCloud2(pcl_pc2,*temp_cloud);

      if(temp_cloud->points.size() == 0) {
        ROS_INFO("Skipping empty point cloud");
        continue;
      } else {
          ROS_INFO("Processing cloud in set");
      }

//	ROS_INFO("Doing some noise reduction");
     // pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_filtered_ptr (new pcl::PointCloud<pcl::PointXYZ>);
     // pcl::RadiusOutlierRemoval<pcl::PointXYZ> outrem;
      //outrem.setInputCloud(temp_cloud);
      //outrem.setRadiusSearch(0.8);
      //outrem.setMinNeighborsInRadius (5);
     // outrem.setNegative(false);
      //outrem.filter (*cloud_filtered_ptr);

/*
      pcl::StatisticalOutlierRemoval<pcl::PointXYZ> f;
      f.setInputCloud (temp_cloud);
      f.setMeanK (50);
      f.setStddevMulThresh (1.0);
      f.setNegative(false);
      f.filter(*cloud_filtered_ptr);
      */

     // pcl::PCDWriter writer;
    //  writer.write<pcl::PointXYZ> ("filtered.pcd", *cloud_filtered_ptr, false);
  //    temp_cloud = cloud_filtered_ptr;

      //ROS_INFO("- Computing centroid");
     // Eigen::Vector4f centroid;
    //  pcl::compute3DCentroid(*temp_cloud, centroid);

      // uh oh, should this be robot_pose?
    //  octomap::point3d octo_centroid(centroid[0],centroid[1],centroid[2]);
    octomap::point3d octo_centroid(robot_pose.position.x,robot_pose.position.y,robot_pose.position.z);

      //octomap::point3d octo_centroid(7.182,2.970,1.76);

      ROS_INFO("- Converting to OctoMap");
      // convert observation cloud to octomap and return it
      octomap::Pointcloud oct_pc;
      octomap::pointCloud2ToOctomap(input_cloud, oct_pc);

      map.insertPointCloud(oct_pc, octo_centroid);

  }


//  ROS_INFO("- Writing to file");
//  map.writeBinary("output.bt");
  ROS_INFO("- Converting to ROS msg");
  octomap_msgs::Octomap octo_msg;
  octo_msg.header.frame_id = "map";
  octomap_msgs::fullMapToMsg(map,octo_msg);
  octomap_pub.publish(octo_msg);
  ROS_INFO("- Done! Returning");
  return octo_msg;
}

// calculates the octomap overlap between source and target
// how many points from source are also in target?
float calculate_octo_overlap(sensor_msgs::PointCloud2 source, sensor_msgs::PointCloud2 target) {
  std::vector<sensor_msgs::PointCloud2> s;
  std::vector<sensor_msgs::PointCloud2> t;
  s.push_back(source);
  t.push_back(target);

  octomap_msgs::Octomap source_octo = convert_pcd_to_octomap(s);
  octomap_msgs::Octomap target_octo = convert_pcd_to_octomap(t);

  // that API though
  AbstractOcTree* source_tree = octomap_msgs::fullMsgToMap(source_octo);
  AbstractOcTree* target_tree = octomap_msgs::fullMsgToMap(target_octo);

  OcTree* source_octree = dynamic_cast<OcTree*>(source_tree);
  OcTree* target_octree = dynamic_cast<OcTree*>(target_tree);


  // the highest possible score is that all of the points from source are also in target
  float source_nodes = 0;
  float overlapping_nodes = 0;
  for(OcTree::tree_iterator it = source_octree->begin_tree(), end=source_octree->end_tree(); it!= end; ++it)
    {
      source_nodes+=1;
      point3d source_point = it.getCoordinate();
      for(OcTree::tree_iterator itt = target_octree->begin_tree(), endt=target_octree->end_tree(); itt!= endt; ++itt)
      {
        point3d target_point = itt.getCoordinate();
        float distance = source_point.distance(target_point);
        if(distance <= source_octree->getResolution()) {
          overlapping_nodes++;
          break;
        }
      }

    }

  cout << " -- " << endl;
  cout << "points in source cloud: " << source_nodes << endl;
  cout << "overlapping nodes: " << overlapping_nodes << endl;

  float degree = (100.0/source_nodes)*overlapping_nodes;

  cout << "degree of overlap: " << degree << endl;
  cout << " -- " << endl;


  return degree;
}

std::vector<geometry_msgs::Point> extract_normals_from_octomap(octomap_msgs::Octomap& in)
{
  ROS_INFO("Attempting to extract normals from octomap");
  std::vector<geometry_msgs::Point> output_points;
  AbstractOcTree* ab_tree = octomap_msgs::fullMsgToMap(in);

  if (!ab_tree){
    ROS_ERROR("Failed to recreate octomap");
    return output_points;
  }
  OcTree* tree = dynamic_cast<OcTree*>(ab_tree);
  OcTree* sp_tree = new OcTree(tree->getResolution());
  int free = 0;
  int occupied = 0;
  int supported = 0;

  ROS_INFO("Extracting supporting planes from octomap");

  for(OcTree::leaf_iterator it = tree->begin_leafs(),
        end=tree->end_leafs(); it!= end; ++it)
    {
      if (tree->isNodeOccupied(*it))
        {
          occupied++;
          std::vector<point3d> normals;

          point3d p3d = it.getCoordinate();

          bool got_normals = tree->getNormals(p3d ,normals, true);
          std::vector<point3d>::iterator normal_iter;

          point3d avg_normal (0.0, 0.0, 0.0);
          for(std::vector<point3d>::iterator normal_iter = normals.begin(),
                end = normals.end(); normal_iter!= end; ++normal_iter)
            {
              avg_normal+= (*normal_iter);
            }
          if (normals.size() > 0)
            {
              supported++;
              // cout << "#Normals: " << normals.size() << endl;
              avg_normal/= normals.size();
              point3d z_axis ( 0.0, 0.0, 1.0);
              double angle = avg_normal.angleTo(z_axis);
              point3d coord = it.getCoordinate();
	            double z = it.getZ();

              if ( angle < ANGLE_MAX_DIFF)
                {
                  sp_tree->updateNode(coord,true);
                  geometry_msgs::Point pt;
                  pt.x = coord.x();
                  pt.y = coord.y();
                  pt.z = coord.z();
                  output_points.push_back(pt);
                }
            }
        }
      else
        {
          free++;
        }
    }
  //sp_tree->writeBinary("post_alg.bt");
  ROS_INFO("Extracted map size: %i (%i free, and %i occupied leaf nodes were discarded)", supported, free, occupied - supported);
  return output_points;
}


bool extract_normals_from_octomap_cb(
initial_surface_view_evaluation::ExtractNormalsFromOctomap::Request &req, // phew! what a mouthful!
initial_surface_view_evaluation::ExtractNormalsFromOctomap::Response &res)
{
  std::vector<geometry_msgs::Point> out = extract_normals_from_octomap(req.octomap);
  res.up_facing_points = out;
  return true;
}


bool convert_pcd_to_octomap_cb(
initial_surface_view_evaluation::ConvertCloudToOctomap::Request &req, // phew! what a mouthful!
initial_surface_view_evaluation::ConvertCloudToOctomap::Response &res)
{
  octomap_msgs::Octomap out = convert_pcd_to_octomap(req.clouds);
  res.octomap = out;
  return true;
}

bool calculate_octree_overlap_cb(
initial_surface_view_evaluation::CalculateOctreeOverlap::Request &req, // phew! what a mouthful!
initial_surface_view_evaluation::CalculateOctreeOverlap::Response &res)
{
  float out = calculate_octo_overlap(req.source,req.target);
  res.degree_of_overlap = out;
  return true;
}

int main (int argc, char** argv)
{
  ros::init (argc, argv, "surface_based_object_learnin_octomap_tools");
  ros::NodeHandle node;

  ROS_INFO("Setting up OcTree conversion services");
  ros::ServiceServer conversion_service = node.advertiseService("/surface_based_object_learning/convert_pcd_to_octomap", convert_pcd_to_octomap_cb);
  ros::ServiceServer extract_service = node.advertiseService("/surface_based_object_learning/extract_normals_from_octomap", extract_normals_from_octomap_cb);
  ros::ServiceServer octree_overlap = node.advertiseService("/surface_based_object_learning/calculate_octree_overlap", calculate_octree_overlap_cb);
  ROS_INFO("Done");



  ros::spin();
  return 0;
}
