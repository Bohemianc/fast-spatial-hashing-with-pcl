
#include <pcl/visualization/pcl_visualizer.h>
#include <pcl/io/pcd_io.h>
#include <pcl/io/vtk_lib_io.h>
#include <pcl/common/transforms.h>
#include <vtkVersion.h>
#include <vtkPLYReader.h>
#include <vtkOBJReader.h>
#include <vtkTriangle.h>
#include <vtkTriangleFilter.h>
#include <vtkPolyDataMapper.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/console/print.h>
#include <pcl/console/parse.h>

inline double uniform_deviate(int seed) {
    double ran = seed * (1.0 / (RAND_MAX + 1.0));
    return ran;
}

inline void randomPointTriangle(float a1, float a2, float a3, float b1,
                                float b2, float b3, float c1, float c2,
                                float c3, float r1, float r2,
                                Eigen::Vector3f& p) {
    float r1sqr = std::sqrt(r1);
    float OneMinR1Sqr = (1 - r1sqr);
    float OneMinR2 = (1 - r2);
    a1 *= OneMinR1Sqr;
    a2 *= OneMinR1Sqr;
    a3 *= OneMinR1Sqr;
    b1 *= OneMinR2;
    b2 *= OneMinR2;
    b3 *= OneMinR2;
    c1 = r1sqr * (r2 * c1 + b1) + a1;
    c2 = r1sqr * (r2 * c2 + b2) + a2;
    c3 = r1sqr * (r2 * c3 + b3) + a3;
    p[0] = c1;
    p[1] = c2;
    p[2] = c3;
}

inline void randPSurface(vtkPolyData* polydata,
                         std::vector<double>* cumulativeAreas, double totalArea,
                         Eigen::Vector3f& p, bool calcNormal,
                         Eigen::Vector3f& n, bool calcColor,
                         Eigen::Vector3f& c) {
    float r = static_cast<float>(uniform_deviate(rand()) * totalArea);

    std::vector<double>::iterator low =
        std::lower_bound(cumulativeAreas->begin(), cumulativeAreas->end(), r);
    vtkIdType el = vtkIdType(low - cumulativeAreas->begin());

    double A[3], B[3], C[3];
    vtkIdType npts = 0;
    vtkIdType* ptIds = nullptr;
    polydata->GetCellPoints(el, npts, ptIds);
    polydata->GetPoint(ptIds[0], A);
    polydata->GetPoint(ptIds[1], B);
    polydata->GetPoint(ptIds[2], C);
    if (calcNormal) {
        // OBJ: Vertices are stored in a counter-clockwise order by default
        Eigen::Vector3f v1 = Eigen::Vector3f(A[0], A[1], A[2]) -
                             Eigen::Vector3f(C[0], C[1], C[2]);
        Eigen::Vector3f v2 = Eigen::Vector3f(B[0], B[1], B[2]) -
                             Eigen::Vector3f(C[0], C[1], C[2]);
        n = v1.cross(v2);
        n.normalize();
    }
    float r1 = static_cast<float>(uniform_deviate(rand()));
    float r2 = static_cast<float>(uniform_deviate(rand()));
    randomPointTriangle(float(A[0]), float(A[1]), float(A[2]), float(B[0]),
                        float(B[1]), float(B[2]), float(C[0]), float(C[1]),
                        float(C[2]), r1, r2, p);

    if (calcColor) {
        vtkUnsignedCharArray* const colors = vtkUnsignedCharArray::SafeDownCast(
            polydata->GetPointData()->GetScalars());
        if (colors && colors->GetNumberOfComponents() == 3) {
            double cA[3], cB[3], cC[3];
            colors->GetTuple(ptIds[0], cA);
            colors->GetTuple(ptIds[1], cB);
            colors->GetTuple(ptIds[2], cC);

            randomPointTriangle(float(cA[0]), float(cA[1]), float(cA[2]),
                                float(cB[0]), float(cB[1]), float(cB[2]),
                                float(cC[0]), float(cC[1]), float(cC[2]), r1,
                                r2, c);
        } else {
            static bool printed_once = false;
            if (!printed_once)
                PCL_WARN(
                    "Mesh has no vertex colors, or vertex colors are not RGB!");
            printed_once = true;
        }
    }
}

void uniform_sampling(vtkSmartPointer<vtkPolyData> polydata, size_t n_samples,
                      bool calc_normal, bool calc_color,
                      pcl::PointCloud<pcl::PointXYZRGBNormal>& cloud_out) {
    polydata->BuildCells();
    vtkSmartPointer<vtkCellArray> cells = polydata->GetPolys();

    double p1[3], p2[3], p3[3], totalArea = 0;
    std::vector<double> cumulativeAreas(cells->GetNumberOfCells(), 0);
    vtkIdType npts = 0, *ptIds = nullptr;
    size_t cellId = 0;
    for (cells->InitTraversal(); cells->GetNextCell(npts, ptIds); cellId++) {
        polydata->GetPoint(ptIds[0], p1);
        polydata->GetPoint(ptIds[1], p2);
        polydata->GetPoint(ptIds[2], p3);
        totalArea += vtkTriangle::TriangleArea(p1, p2, p3);
        cumulativeAreas[cellId] = totalArea;
    }

    cloud_out.points.resize(n_samples);
    cloud_out.width = static_cast<pcl::uint32_t>(n_samples);
    cloud_out.height = 1;

    for (size_t i = 0; i < n_samples; i++) {
        Eigen::Vector3f p;
        Eigen::Vector3f n(0, 0, 0);
        Eigen::Vector3f c(0, 0, 0);
        randPSurface(polydata, &cumulativeAreas, totalArea, p, calc_normal, n,
                     calc_color, c);
        cloud_out.points[i].x = p[0];
        cloud_out.points[i].y = p[1];
        cloud_out.points[i].z = p[2];
        if (calc_normal) {
            cloud_out.points[i].normal_x = n[0];
            cloud_out.points[i].normal_y = n[1];
            cloud_out.points[i].normal_z = n[2];
        }
        if (calc_color) {
            cloud_out.points[i].r = static_cast<uint8_t>(c[0]);
            cloud_out.points[i].g = static_cast<uint8_t>(c[1]);
            cloud_out.points[i].b = static_cast<uint8_t>(c[2]);
        }
    }
}

using namespace pcl;
using namespace pcl::io;
using namespace pcl::console;

pcl::PointCloud<pcl::PointXYZRGBNormal>::Ptr __mesh_sampling(
    const char* filename, int sample_points, float leaf_size,
    bool write_normals, bool write_colors) {
    vtkSmartPointer<vtkPolyData> polydata1 =
        vtkSmartPointer<vtkPolyData>::New();
    vtkSmartPointer<vtkOBJReader> readerQuery =
        vtkSmartPointer<vtkOBJReader>::New();
    readerQuery->SetFileName(filename);
    readerQuery->Update();
    polydata1 = readerQuery->GetOutput();

    // make sure that the polygons are triangles!
    vtkSmartPointer<vtkTriangleFilter> triangleFilter =
        vtkSmartPointer<vtkTriangleFilter>::New();
    triangleFilter->SetInputData(polydata1);
    triangleFilter->Update();

    vtkSmartPointer<vtkPolyDataMapper> triangleMapper =
        vtkSmartPointer<vtkPolyDataMapper>::New();
    triangleMapper->SetInputConnection(triangleFilter->GetOutputPort());
    triangleMapper->Update();
    polydata1 = triangleMapper->GetInput();

    pcl::PointCloud<pcl::PointXYZRGBNormal>::Ptr cloud_1(
        new pcl::PointCloud<pcl::PointXYZRGBNormal>);

    uniform_sampling(polydata1, sample_points, write_normals, write_colors,
                     *cloud_1);

    // Voxelgrid
    VoxelGrid<PointXYZRGBNormal> grid_;
    grid_.setInputCloud(cloud_1);
    grid_.setLeafSize(leaf_size, leaf_size, leaf_size);

    pcl::PointCloud<pcl::PointXYZRGBNormal>::Ptr voxel_cloud(
        new pcl::PointCloud<pcl::PointXYZRGBNormal>);
    grid_.filter(*voxel_cloud);
    return voxel_cloud;
}

pcl::PointCloud<pcl::PointXYZ>::Ptr mesh_sampling(const char* filename,
                                                  int sample_points,
                                                  float leaf_size) {
    pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_xyz(
        new pcl::PointCloud<pcl::PointXYZ>);
    pcl::copyPointCloud(
        *__mesh_sampling(filename, sample_points, leaf_size, false, false),
        *cloud_xyz);
    return cloud_xyz;
}

pcl::PointCloud<pcl::PointNormal>::Ptr mesh_sampling_with_normal(
    const char* filename, int sample_points, float leaf_size) {
    pcl::PointCloud<pcl::PointNormal>::Ptr cloud(
        new pcl::PointCloud<pcl::PointNormal>);
    pcl::copyPointCloud(
        *__mesh_sampling(filename, sample_points, leaf_size, true, false),
        *cloud);
    return cloud;
}
