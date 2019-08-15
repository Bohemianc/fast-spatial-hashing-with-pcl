#define PCL_NO_PRECOMPILE
#include <pcl/console/parse.h>
#include <pcl/console/print.h>
#include <pcl/visualization/pcl_visualizer.h>
#include <pcl/features/normal_3d.h>
#include <mesh_sampling.h>
#include <random>
#include <vsa.hpp>

using namespace std;
using namespace pcl;
const int default_number_samples = 100000;
float default_leaf_size;

pcl::PointCloud<pcl::Normal>::Ptr get_normals(
    const pcl::PointCloud<pcl::PointXYZ>::Ptr& cloud) {
    pcl::NormalEstimation<pcl::PointXYZ, pcl::Normal> ne;
    pcl::search::KdTree<pcl::PointXYZ>::Ptr tree(
        new pcl::search::KdTree<pcl::PointXYZ>);
    ne.setInputCloud(cloud);
    ne.setSearchMethod(tree);
    ne.setKSearch(50);
    pcl::PointCloud<pcl::Normal>::Ptr cloud_normals(
        new pcl::PointCloud<pcl::Normal>);
    ne.compute(*cloud_normals);
    return cloud_normals;
}

int main(int argc, char** argv) {
    if (argc < 6) {
        console::print_error("no enough param\n");
        return (-1);
    }

    sscanf(argv[2],"%f",&default_leaf_size);

    std::vector<int> obj_file_indices =
        console::parse_file_extension_argument(argc, argv, ".obj");

    if (obj_file_indices.size() != 1) {
        console::print_error("Need a single input OBJ file to continue.\n");
        return (-1);
    }
    const char* filename = argv[obj_file_indices[0]];
    PointCloud<PointXYZ>::Ptr voxel_cloud =
        mesh_sampling(filename, default_number_samples, default_leaf_size);

    PointCloud<Normal>::Ptr normals = get_normals(voxel_cloud);
    PointCloud<PointNormal>::Ptr pNormal(new PointCloud<PointNormal>);
    concatenateFields(*voxel_cloud, *normals, *pNormal);
    // pNormal = mesh_sampling_with_normal(filename, default_number_samples,
    //                                     default_leaf_size);

    int metric;
    float eps;
    int k;
    sscanf(argv[3],"%d",&metric);
    sscanf(argv[4],"%f",&eps);
    sscanf(argv[5],"%d",&k);
    printf("leaf size=%f,metric=%d,eps=%f,k=%d\n",default_leaf_size,metric,eps,k);  //debug
    // clustering
    VSA vsa;
    vsa.setInputCloud(pNormal);
    vsa.setMetricOption(metric);
    vsa.setEps(eps);
    vsa.setK(k);
    auto res = vsa.compute();

    std::default_random_engine generator;
    std::uniform_int_distribution<int> color(0, 255);

    int cnt = 0;
    visualization::PCLVisualizer viewer("VOXELIZED SAMPLES CLOUD");
    for (auto points : res) {
        printf("%lu ", points.size());
        PointCloud<PointNormal>::Ptr pointcloud_ptr(
            new PointCloud<PointNormal>);
        for (auto point : points) {
            pointcloud_ptr->points.push_back(pNormal->points[point]);
        }
        visualization::PointCloudColorHandlerCustom<PointNormal> handler(
            pointcloud_ptr, color(generator), color(generator),
            color(generator));
        viewer.addPointCloud(pointcloud_ptr, handler, string(cnt++, 'x'));
    }
    puts("");
    viewer.setBackgroundColor(0.05, 0.05, 0.05, 0);
    // viewer.addCoordinateSystem(1.0, "cloud", 0);
    viewer.spin();
    printf("%d %d\n", (int)voxel_cloud->points.size(),
           (int)normals->points.size());
    return 0;
}
