#define _USE_MATH_DEFINES // for C++
#include <cmath>
#include <iostream>
#include <string>
#include <fstream>
#include <filesystem>

#include "meshview/meshview.hpp"
#include "meshview/meshview_imgui.hpp"
#include "Eigen/Core"
#include "Eigen/Geometry"
#include "TSDFVolume.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <opencv2/opencv.hpp>

#include <k4a/k4a.h>
#include <SimpleICP/simpleicp.h>

using namespace meshview;
#define _VOXEL_CARVE 1

//#define _VOXEL_TRUNC 6;
//int VOXRES = 512;
//int VOXSMOOTH = 2;
//
//#define _VOXEL_TRUNC 5;
//int VOXRES = 256;
//int VOXSMOOTH = 2;
////
//#define _VOXEL_TRUNC 4;
//int VOXRES = 128;

#define _VOXEL_TRUNC 3;
int VOXRES = 64;

//#define _VOXEL_TRUNC 2;
//int VOXRES = 32;

int VOXSMOOTH = 1;

//TSDFVolume* theVolume;
Viewer viewer;
std::map<int, Eigen::Matrix4d> extrinsics;
std::map<int, Eigen::Matrix4d> extrinsicsNEW;

std::map<int, Eigen::Matrix3d> intrinsics;
std::map<int, k4a_calibration_t> k4aCalibrations;

std::vector<TRIANGLE> g_tris;
std::vector<k4a_transformation_t> g_transforms;
std::vector<k4a_image_t> g_pointClouds;// = nullptr;
std::vector<Eigen::MatrixXd> g_pointMatrices;

void WritePLY(std::string filename, std::string filepath, std::vector<TRIANGLE> mesh)
{
    std::ofstream writer;

    if (filepath != "")
    {
        std::filesystem::create_directories(filepath);

        writer.open(filepath + "/" + filename);
    }
    else
    {
        writer.open(filename);
    }
    int numVerts = mesh.size() * 3;
    int numTris = mesh.size();
    // write the header

    writer << "ply\n";
    writer << "format ascii 1.0\n";
    writer << "comment author: hogue\n";
    writer << "element vertex " << numVerts << "\n";
    writer << "property float x\n";
    writer << "property float y\n";
    writer << "property float z\n";
    writer << "property uchar red\n";
    writer << "property uchar green\n";
    writer << "property uchar blue\n";
    writer << "element face " << numTris << "\n";
    writer << "property list uchar int vertex_indices\n";
    //writer << "property uchar red\n";
    //writer << "property uchar green\n";
    //writer << "property uchar blue\n";
    writer << "end_header\n";



    // vertices
    int numV = 0;
    for (int i = 0; i < numTris; ++i)
    {
        auto TRI = mesh[i];
        auto v0 = TRI.p[0];
        auto v1 = TRI.p[1];
        auto v2 = TRI.p[2];
        auto c0 = TRI.c;
        int  r, g, b;
        r = (int)(255.f * c0.x());
        g = (int)(255.f * c0.y());
        b = (int)(255.f * c0.z());
        writer << v0.x() << " " << v0.y() << " " << v0.z() << " " << r << " " << g << " " << b << "\n";
        writer << v1.x() << " " << v1.y() << " " << v1.z() << " " << r << " " << g << " " << b << "\n";
        writer << v2.x() << " " << v2.y() << " " << v2.z() << " " << r << " " << g << " " << b << "\n";
        numV += 3;
        /* writer << "v " << v0.x() << " " << v0.y() << " " << v0.z() << "\n";
         writer << "v " << v1.x() << " " << v1.y() << " " << v1.z() << "\n";
         writer << "v " << v2.x() << " " << v2.y() << " " << v2.z() << "\n";*/
    }

    // face indices
    int numT = 0;
    for (int i = 0; i < numTris; ++i)
    {
        auto TRI = mesh[i];
        int i0 = 3 * i + 0;
        int i1 = 3 * i + 1;
        int i2 = 3 * i + 2;
        auto c0 = TRI.c;
        int  r, g, b;
        r = (int)(255.f * c0.x());
        g = (int)(255.f * c0.y());
        b = (int)(255.f * c0.z());

        writer << "3 " << (i0) << " " << (i1) << " " << (i2) << "\n";
        //        writer << "3 " << (i0) << " " << (i1) << " " << (i2) << " " << r << " " << g << " " << b << "\n";
        numT++;
    }
    writer.close();
#ifdef _VERBOSE
    std::cout << "wrote:" << numV << " verts" << std::endl;
    std::cout << "wrote: " << numT << " tris" << std::endl;
#endif
}

void WriteOBJ(std::string filename, std::string filepath, std::vector<TRIANGLE> mesh)
{
    std::ofstream writer;

    if (filepath != "")
    {
        std::filesystem::create_directories(filepath);

        writer.open(filepath + "/" + filename);
    }
    else
    {
        writer.open(filename);
    }
    int numVerts = mesh.size() * 3;
    int numTris = mesh.size();

    for (int i = 0; i < numTris; ++i)
    {
        auto TRI = mesh[i];
        auto v0 = TRI.p[0];
        auto v1 = TRI.p[1];
        auto v2 = TRI.p[2];

        writer << "v " << v0.x() << " " << v0.y() << " " << v0.z() << "\n";
        writer << "v " << v1.x() << " " << v1.y() << " " << v1.z() << "\n";
        writer << "v " << v2.x() << " " << v2.y() << " " << v2.z() << "\n";
    }



    for (int i = 0; i < numTris; ++i)
    {
        auto TRI = mesh[i];
        int i0 = 3 * i + 0;
        int i1 = 3 * i + 1;
        int i2 = 3 * i + 2;

        writer << "f " <<
            (i0 + 1) << " " <<
            (i1 + 1) << " " <<
            (i2 + 1) << "\n";
    }

    writer.close();
}

void AddEigenToViewer(Eigen::MatrixXd& pts, Eigen::Matrix4d& ex, Eigen::Matrix4d& Micp, float r, float g, float b) {
    int n = pts.rows();
    Points p(n, 3);
    for (int i = 0, ind = 0; i < n; i++, ind += 3)
    {
        Eigen::Vector4d v(pts(i, 0), pts(i, 1), pts(i, 2), 1.0);
        Eigen::Vector4d res = Micp * v;
        p(i, 0) = res(0);//pts(i,0);
        p(i, 1) = res(1);//pts(i,1);
        p(i, 2) = res(2);//pts(i,2);
    }
    viewer.add_point_cloud(p, r,g,b);
}

void AddMeshToViewer() {
    int n;
    n = g_tris.size();
    std::cout << "numTris:" << n << std::endl;

    Points verts(3 * n, 3);
    Points colors(3 * n, 3);
    for (int i = 0, ind = 0; i < n; i++, ind += 3)
    {
        verts(ind + 0, 0) = g_tris[i].p[0][0];
        verts(ind + 0, 1) = g_tris[i].p[0][1];
        verts(ind + 0, 2) = g_tris[i].p[0][2];

        verts(ind + 1, 0) = g_tris[i].p[1][0];
        verts(ind + 1, 1) = g_tris[i].p[1][1];
        verts(ind + 1, 2) = g_tris[i].p[1][2];

        verts(ind + 2, 0) = g_tris[i].p[2][0];
        verts(ind + 2, 1) = g_tris[i].p[2][1];
        verts(ind + 2, 2) = g_tris[i].p[2][2];

        colors(ind + 0, 0) = g_tris[i].c[0];
        colors(ind + 0, 1) = g_tris[i].c[1];
        colors(ind + 0, 2) = g_tris[i].c[2];

        colors(ind + 1, 0) = g_tris[i].c[0];
        colors(ind + 1, 1) = g_tris[i].c[1];
        colors(ind + 1, 2) = g_tris[i].c[2];

        colors(ind + 2, 0) = g_tris[i].c[0];
        colors(ind + 2, 1) = g_tris[i].c[1];
        colors(ind + 2, 2) = g_tris[i].c[2];

    }

    auto& pyra = viewer
        .add_mesh(verts,
                  /* Triangle indices (unsigned fx3) here; pass
                     empty to use: 0 1 2, 3 4 5 etc */
                  Triangles(),
                  colors
        )
        .translate(Vector3f(0, 0, 0))
        .set_shininess(32.f);
}

void AddVolumeToViewer(TSDFVolume* v) {

    int n = g_tris.size();
    // * Triangle mesh: single color
    Points verts(3 * n, 3);
    Points colors(3 * n, 3);
    for (int i = 0, ind = 0; i < n; i++, ind += 3)
    {
        verts(ind + 0, 0) = g_tris[i].p[0][0];
        verts(ind + 0, 1) = g_tris[i].p[0][1];
        verts(ind + 0, 2) = g_tris[i].p[0][2];

        verts(ind + 1, 0) = g_tris[i].p[1][0];
        verts(ind + 1, 1) = g_tris[i].p[1][1];
        verts(ind + 1, 2) = g_tris[i].p[1][2];

        verts(ind + 2, 0) = g_tris[i].p[2][0];
        verts(ind + 2, 1) = g_tris[i].p[2][1];
        verts(ind + 2, 2) = g_tris[i].p[2][2];

        colors(ind + 0, 0) = g_tris[i].c[0];
        colors(ind + 0, 1) = g_tris[i].c[1];
        colors(ind + 0, 2) = g_tris[i].c[2];

        colors(ind + 1, 0) = g_tris[i].c[0];
        colors(ind + 1, 1) = g_tris[i].c[1];
        colors(ind + 1, 2) = g_tris[i].c[2];

        colors(ind + 2, 0) = g_tris[i].c[0];
        colors(ind + 2, 1) = g_tris[i].c[1];
        colors(ind + 2, 2) = g_tris[i].c[2];

    }

    auto& pyra = viewer
        .add_mesh(verts,
                  /* Triangle indices (unsigned fx3) here; pass
                     empty to use: 0 1 2, 3 4 5 etc */
                  Triangles(),
                  colors
        )
        .translate(Vector3f(0, 0, 0))
        .set_shininess(32.f);

    /*
    for (int i = 0; i < tris.size(); i++) {
        verts << tris[i].p[0][0], tris[i].p[0][1], tris[i].p[0][2];
    }
    viewer.add_mesh(verts, Triangles(), 0, 1, 0)
        .translate(Vector3f(0.f, 0.f, 0.f))
        .set_shininess(32.f)
        ;
        */


        //for (int k = 0; k < v->res[2]; k++) {
        //    for (int j = 0; j < v->res[1]; j++) {
        //        for (int i = 0; i < v->res[0]; i++) {
        //            Voxel vx = v->get(i, j, k);
        //            if (vx.flag == VOXEL_UNSEEN) {
        //                float r, g, b;
        //                r = vx.r/255.f;
        //                g = 1;// vx.g / 255.f;
        //                b = vx.b/255.f;

        //                viewer.add_cube(Eigen::Vector3f(vx.c(0), vx.c(1), vx.c(2)), v->vSize[0], Eigen::Vector3f(r, g, b));
        //            }
        //            if (vx.flag == VOXEL_FULL) {
        //                float r, g, b;
        //                r = vx.r / 255.f;
        //                g = vx.g / 255.f;
        //                b = vx.b / 255.f;

        //                viewer.add_cube(Eigen::Vector3f(vx.c(0), vx.c(1), vx.c(2)), v->vSize[0], Eigen::Vector3f(r, g, b));
        //            }

        //        }
        //    }
        //}

}

void LoadIntrinsics(std::string fname, int cameraNum) {
    k4a_calibration_t calibration;
    std::ifstream intrinsic_file = std::ifstream(fname);
    std::string file_contents;

    intrinsic_file.seekg(0, std::ios::end);
    size_t file_length = intrinsic_file.tellg();
    intrinsic_file.seekg(0, std::ios::beg);

    file_contents.resize(file_length);
    intrinsic_file.read(&file_contents[0], file_length);

    intrinsic_file.close();

    k4a_result_t res = k4a_calibration_get_from_raw(&file_contents[0], file_contents.size() + 1, k4a_depth_mode_t::K4A_DEPTH_MODE_NFOV_UNBINNED, k4a_color_resolution_t::K4A_COLOR_RESOLUTION_720P, &calibration);
    if (res == k4a_result_t::K4A_RESULT_FAILED)
    {
        std::cout << "ERROR: FAILED TO READ INTRINSICS JSON!" << std::endl;
    }

    auto params = calibration.color_camera_calibration.intrinsics.parameters;

    Eigen::Matrix3d intrinsic_mat = Eigen::Matrix3d::Identity();
    intrinsic_mat(0, 0) = params.param.fx;
    intrinsic_mat(1, 1) = params.param.fy;
    intrinsic_mat(0, 2) = params.param.cx;
    intrinsic_mat(1, 2) = params.param.cy;
    std::cout << intrinsic_mat << std::endl;
    intrinsics[cameraNum] = intrinsic_mat;
    k4aCalibrations[cameraNum] = calibration;

}

void LoadExtrinsics(std::string fname) {
    std::fstream extrinsics_filestream;
    extrinsics_filestream.open(fname);
    int count = 0;
    int camNum = 0;
    int modCount;

    while (!extrinsics_filestream.eof()) {
        std::vector<std::string> extrinsics_string;
        std::string parser = "";
        if (!std::getline(extrinsics_filestream, parser)) break;
        std::cout << parser << std::endl;
        std::stringstream str(parser);
        int a, b, c;
        str >> a >> b >> c;
        std::cout << "cam:" << c << std::endl;
        int theCam = c;
        Eigen::Matrix4d theMat;
        for (int i = 0; i < 4; ++i)
        {
            float aa, bb, cc, dd;
            std::getline(extrinsics_filestream, parser);
            std::stringstream ss(parser);
            ss >> aa >> bb >> cc >> dd;
            theMat(i, 0) = aa;
            theMat(i, 1) = bb;
            theMat(i, 2) = cc;
            theMat(i, 3) = dd;

        }
        extrinsics[theCam] = theMat;// .inverse();
        count++;
    }
}
void SaveExtrinsicsLog(std::map<int, Eigen::Matrix4d> e) {
    std::ofstream writer;
    writer.open("outputExtrinsics.log");
    for (int i = 0; i < e.size(); i++) {
        writer << "0    0    " << i << "\n";
        writer << e[i] << "\n";
    }
    
    
    writer.close();
}

float GetAllDepth(std::vector<Eigen::Vector3d>& pts, cv::Mat& imDepth, std::vector<float>& depths) {
    float sum = 0.f;
    int num = 0;
    float min = 1000;
    float max = -10000;
    for (int i = 0; i < pts.size(); i++) {
        int u, v;
        u = (int)pts[i](0);
        v = (int)pts[i](1);
        float uvz = pts[i](2);
        if (u > 0 && u < imDepth.cols && v > 0 && v < imDepth.rows) { // is the projected voxel center in the image?
            ushort depth = imDepth.at<ushort>(v, u); // get depth image value            
            float depthMeasurement = (float)depth / 1000.f;
            depths.push_back(depthMeasurement);
            //if (depthMeasurement < 2) {
            sum += depthMeasurement;
            num++;
            //}
            min = fminf(min, depthMeasurement);
            max = fmaxf(max, depthMeasurement);
        }
    }
    return min;
    //    if (num > 0) return (sum / (float)num);
      //  else return 1;
}

Eigen::Vector3d ProjectPoint(Eigen::Vector3d& p, Eigen::Matrix3d& intrin, Eigen::Matrix4d& exInv, Eigen::Vector4d& cc) {
    /* c is center in world coordinates */
                    // convert to camera coordinates
    cc = exInv * Eigen::Vector4d(p(0), p(1), p(2), 1);

    /* project the center of the voxel onto the images */
    Eigen::Vector3d pp;
    float invz = 1.0f / cc(2);
    float fx, fy, cx, cy;
    fx = intrin(0, 0);
    fy = intrin(1, 1);
    cx = intrin(0, 2);
    cy = intrin(1, 2);
    pp(0) = cc(0) * fx * invz + cx;  // u
    pp(1) = cc(1) * fy * invz + cy;  // v
    pp(2) = cc(2); // store the Z value here (depth from camera of this point)

    return pp;
}
void ProjectAllPoints(std::vector<Eigen::Vector3d>& pts, Eigen::Matrix3d& intrin, Eigen::Matrix4d& exInv, std::vector<Eigen::Vector3d>& ptsOut, std::vector<Eigen::Vector4d>& ccOut) {
    for (int i = 0; i < pts.size(); i++) {
        Eigen::Vector4d cc;
        Eigen::Vector3d v = ProjectPoint(pts[i], intrin, exInv, cc);
        ptsOut.push_back(v);
        ccOut.push_back(cc);
    }
}
void CreateAndAddMesh(Eigen::Matrix3d& in, Eigen::Matrix4d& ex, cv::Mat& imRGB, cv::Mat& imMATTE, cv::Mat& imDepth, k4a_image_t& k4a_pointcloud) {
    // go through and make a triangle mesh from the passed in point cloud
#define IND_IM(i,j,w,h) (((j)*(w)) + (i))
    int w, h;
    w = imMATTE.cols;
    h = imMATTE.rows;
    int16_t* pcData = (int16_t*)k4a_image_get_buffer(k4a_pointcloud);
    for (int j = 1; j < imMATTE.rows; j++) {
        for (int i = 0; i < imMATTE.cols - 1; i++) {
            int ind[4];
            int offsets[4][2] = {
                {0,0},
                {0,-1},
                {1,-1},
                {1,0}
            };
            ind[0] = IND_IM(i, j, w, h);
            ind[1] = IND_IM(i, j - 1, w, h);
            ind[2] = IND_IM(i + 1, j - 1, w, h);
            ind[3] = IND_IM(i + 1, j, w, h);

            int matte[4];
            ushort depth[4];
            cv::Vec3b col[4];
            Eigen::Vector3d colf[4];
            Eigen::Vector3d pts[4];
            Eigen::Vector3d pts2[4];
            for (int mm = 0; mm < 4; mm++) {
                cv::Vec3b m = imMATTE.at<cv::Vec3b>(j + offsets[mm][1], i + offsets[mm][0]);  // get matte pixel value
                matte[mm] = (int)m[0];
                col[mm] = imRGB.at<cv::Vec3b>(j + offsets[mm][1], i + offsets[mm][0]); // get colour
                colf[mm][0] = col[mm][2];
                colf[mm][1] = col[mm][1];
                colf[mm][2] = col[mm][0];

                depth[mm] = imDepth.at<ushort>(j + offsets[mm][1], i + offsets[mm][0]); // get depth image value   
                int pcIndex = 3 * ((i + offsets[mm][0]) + (j + offsets[mm][1]) * imRGB.cols);
                float PCX = (float)(pcData[pcIndex + 0]) / 1000.f;
                float PCY = (float)(pcData[pcIndex + 1]) / 1000.f;
                float PCZ = (float)(pcData[pcIndex + 2]) / 1000.f;
                pts[mm][0] = PCX;
                pts[mm][1] = PCY;
                pts[mm][2] = PCZ;
                Eigen::Vector4d v = ex * Eigen::Vector4d(PCX, PCY, PCZ, 1);
                pts2[mm][0] = v[0];
                pts2[mm][1] = v[1];
                pts2[mm][2] = v[2];

            }

            // make 2 triangles
            // triangle 1: index: 0,1,2
            TRIANGLE t0;
            t0.c = (0.3333f / 255.f) * (colf[0] + colf[1] + colf[2]);
            t0.p[0] = pts2[2];
            t0.p[1] = pts2[1];
            t0.p[2] = pts2[0];
            float d1 = (t0.p[0] - t0.p[1]).norm();
            float d2 = (t0.p[0] - t0.p[2]).norm();
            //e1.norm();


            // triangle 2: index: 0,2,3
            TRIANGLE t1;
            t1.c = (0.3333f / 255.f) * (colf[0] + colf[2] + colf[3]);
            t1.p[0] = pts2[3];
            t1.p[1] = pts2[2];
            t1.p[2] = pts2[0];
            float d3 = (t1.p[0] - t1.p[1]).norm();
            float d4 = (t1.p[0] - t1.p[2]).norm();

#define VCHECK(m, i,j,k, v) ((m)[i]>(v)) && ((m)[j]>(v)) && ((m)[k]>(v)) 
#define VCHECK2(m, i,j,k, v) ((m)[i]<(v)) && ((m)[j]<(v)) && ((m)[k]<(v)) 
            // add triangle to viewer list
           // if(VCHECK(matte,0,1,2,250))
            float thresh = 0.05;


            if (d1 < thresh && d2 < thresh)
                if (VCHECK(depth, 0, 1, 2, 200) && VCHECK2(depth, 0, 1, 2, 3000))
                    g_tris.push_back(t0);
            //if (VCHECK(matte, 0, 2, 3, 250))
            if (d3 < thresh && d4 < thresh)
                if (VCHECK(depth, 0, 2, 3, 200) && VCHECK2(depth, 0, 1, 2, 3000))
                    g_tris.push_back(t1);
        }
    }

}

void CarveWithSilhouette(TSDFVolume* vol, Eigen::Matrix3d& in, Eigen::Matrix4d& ex, cv::Mat& imRGB, cv::Mat& imMATTE, cv::Mat& imDepth, k4a_image_t& k4a_pointcloud) {
    /* ok, so the standard simplest way  */
    /* for each voxel */
    //v->makeSphereSDF(0.75f);
    //return;
#ifdef _VERBOSE
    std::cout << "extrinsics:" << std::endl;
    std::cout << ex << std::endl;
#endif
    /* extrinsics passed in convert from camera to wold
       - i.e. multiplying by camera origin (0,0,0) gives the position of the camera in the world to draw
       - we need the transform to convert world coordinates (voxel coords) to be relative to the camera
       -
    */

    auto ExInv = ex;// .inverse();
    auto R = ex.block<3, 3>(0, 0);
    auto T = ex.block<3, 1>(0, 3);

    auto Rt = R.transpose();
    auto T2 = -Rt * T;
    ExInv.block<3, 3>(0, 0) = Rt;
    ExInv.block<3, 1>(0, 3) = T2;
#ifdef _VERBOSE
    std::cout << "Ex:" << ex << std::endl;
    std::cout << "R:" << R << std::endl;
    std::cout << "T:" << T << std::endl;
    std::cout << "ExInv:" << ExInv << std::endl;
#endif
    float trunc_margin = vol->vSize[0] * _VOXEL_TRUNC;// vol->vSize[0] * 6;
#pragma omp parallel for
    for (int k = 0; k < vol->res[2]; k++) {
#pragma omp parallel for
        for (int j = 0; j < vol->res[1]; j++) {
#pragma omp parallel for
            for (int i = 0; i < vol->res[0]; i++) {
                Voxel& vx = vol->get(i, j, k);
                // if the current voxel is already carved in any image then it should be empty for sure
               // if (vx.flag != VOXEL_EMPTY)
                {
                    Eigen::Vector3d c;
                    vol->GetVoxelCoordsFromIndex(i, j, k, c);
                    std::vector<Eigen::Vector3d> allcorners;
                    // project voxel center on to image
                    Eigen::Vector4d pRotExInv;
                    Eigen::Vector3d proj = ProjectPoint(c, in, ExInv, pRotExInv);

                    //// project all corners of the voxel
                    //vol->GetVoxelCornersFromIndex(i, j, k, allcorners);
                    //std::vector<Eigen::Vector3d> ptsOut;
                    //std::vector<Eigen::Vector4d> ccOut;
                    //ProjectAllPoints(allcorners, in, ExInv, ptsOut, ccOut);

                    //std::vector<float> allDepth;
                    //float avgDepth = GetAllDepth(ptsOut, imDepth, allDepth);

                    /* now look in image */
                    int u, v;
                    u = (int)proj(0);
                    v = (int)proj(1);
                    float uvz = proj(2); // this is the depth of the voxel from the camera

                    if (u > 0 && u < imRGB.cols && v > 0 && v < imRGB.rows) { // is the projected voxel center in the image?
                        cv::Vec3b m = imMATTE.at<cv::Vec3b>(v, u);  // get matte pixel value
                        cv::Vec3b col = imRGB.at<cv::Vec3b>(v, u); // get colour
                        ushort depth = imDepth.at<ushort>(v, u); // get depth image value            

                        int16_t* pcData = (int16_t*)k4a_image_get_buffer(k4a_pointcloud);
                        int pcIndex = 3 * (u + v * imRGB.cols);
                        float PCX = (float)(pcData[pcIndex + 0]) / 1000.f;
                        float PCY = (float)(pcData[pcIndex + 1]) / 1000.f;
                        float PCZ = (float)(pcData[pcIndex + 2]) / 1000.f;

                        float depthMeasurement = PCZ;// (float)depth / 1000.f; // convert to meters
                        int matte = (int)m[0];
                        if (matte > 200 && depthMeasurement > 0 && depthMeasurement < 3) // does it have a depth value and is it in the foreground?
                        {
                            float voxelDepthProjected = uvz;
                            float distFromVoxelToSurfaceSample = depthMeasurement - voxelDepthProjected;


                            // std::cout << "depthMeasurement:" << depthMeasurement << " (u,v):"<<"("<<u<<","<<v<<"), ushort:"<< depth<<" m:"<<matte<<std::endl;
                            if (distFromVoxelToSurfaceSample > -trunc_margin)
                            {
                                // std::cout << "distsdf:" << distsdf << std::endl;
                                float sdf = fminf(1.f, distFromVoxelToSurfaceSample / trunc_margin);
                                float oldweight = vx.weight;
                                float newweight = oldweight + 1;
                                float weightSum = oldweight + newweight;

                                float d_old = vx.sdf;
                                float d_new = sdf;
                                float d = (d_old * oldweight + d_new) / newweight;
                                //  if(oldweight) 
                                  //std::cout << "d_old:" << d_old << " d_new:" << d_new << " d:"<<d<< " distfromVox:"<< distFromVoxelToSurfaceSample << " depthf:"<< depthMeasurement<<std::endl;
                                  // std::cout << "d:" << d << std::endl;

                                vx.sdf = d;// (d < -1) ? -1 : (d > 1) ? 1 : d;// d_old + (1.0 / weightSum) * (d_new - d_old);// fmin(d_old, d_new);// d;
                                vx.weight = newweight;

                                vx.r = (oldweight * vx.r + newweight * col[2]) / weightSum;
                                vx.g = (oldweight * vx.g + newweight * col[1]) / weightSum;
                                vx.b = (oldweight * vx.b + newweight * col[0]) / weightSum;
                                vx.flag = VOXEL_FULL;
                            }
                        }
                        //else {
                        //    // the voxel projected to a pixel that didn't have a valid depth OR matte 
                        //    vx.weight = 0;
                        //    vx.sdf = trunc_margin;
                        //    vx.flag = VOXEL_EMPTY;  // carve voxel
                        //}
                    }
                }
            }
        }
    }

}

void TransformDepth(int cam, cv::Mat& old_depth, cv::Mat& new_depth, k4a_calibration_t& calibration, k4a_image_t& k4a_pointcloud) {
    k4a_image_t k4a_transformed_depth = nullptr;
    k4a_image_t k4a_depth = nullptr;
    //    k4a_image_t k4a_pointcloud = nullptr;

    int oldStride = old_depth.step[0];
    if (K4A_RESULT_SUCCEEDED !=
        k4a_image_create_from_buffer(k4a_image_format_t::K4A_IMAGE_FORMAT_DEPTH16, old_depth.cols, old_depth.rows, oldStride,
        old_depth.data, old_depth.step[0] * old_depth.rows, nullptr, nullptr, &k4a_depth))
    {
        std::cout << "Transform Depth error: failed to create k4a image" << std::endl;
    }

    int newStride = new_depth.step[0];
    int cw = calibration.depth_camera_calibration.resolution_width;
    int ch = calibration.depth_camera_calibration.resolution_height;
    int ccolw = calibration.color_camera_calibration.resolution_width;
    int ccolh = calibration.color_camera_calibration.resolution_height;
    int old_step1 = old_depth.step1(0);
    int old_nc = old_depth.channels();

    if (K4A_RESULT_SUCCEEDED !=
        k4a_image_create_from_buffer(k4a_image_format_t::K4A_IMAGE_FORMAT_DEPTH16, new_depth.cols, new_depth.rows, newStride,
        new_depth.data, new_depth.step[0] * new_depth.rows, nullptr, nullptr, &k4a_transformed_depth))
    {
        std::cout << "error k4a_image_create_from_buffer" << std::endl;
    }
    /* k4a_image_create_from_buffer(
        K4A_IMAGE_FORMAT_DEPTH16, new_depth.cols, new_depth.rows,
        new_depth.step[0], new_depth.data,
        new_depth.step[0] * new_depth.rows, NULL, NULL,
        &k4a_transformed_depth);*/


    k4a_transformation_t transform = g_transforms[cam];
    if (transform == 0) {
        transform = k4a_transformation_create(&calibration);
        g_transforms[cam] = transform;
    }

    if (K4A_RESULT_SUCCEEDED != k4a_transformation_depth_image_to_color_camera(transform, k4a_depth, k4a_transformed_depth))
    {
        std::cout << "error transforming depth to rgb" << std::endl;
    }
    if (k4a_pointcloud == NULL)
    {
        k4a_image_create(K4A_IMAGE_FORMAT_CUSTOM, new_depth.cols, new_depth.rows, new_depth.cols * 3 * (int)sizeof(int16_t), &k4a_pointcloud);
    }
    k4a_transformation_depth_image_to_point_cloud(transform, k4a_transformed_depth, K4A_CALIBRATION_TYPE_COLOR, k4a_pointcloud);


    // release memory
    k4a_image_release(k4a_depth);
    k4a_image_release(k4a_transformed_depth);
    //k4a_transformation_destroy(transform);
}

Eigen::Matrix<double, 4, 4> ICP(int indFixed, int indMovable) {
    int ncorrespondences = 2000;
    int neighbors = 10;
    double min_planarity = 0.3;
    double min_change = 1;
    int max_iterations=100;
    Eigen::Matrix<double, 4, 4> H = SimpleICP(g_pointMatrices[indFixed],
                                              g_pointMatrices[indMovable],
                                              ncorrespondences,
                                              neighbors,
                                              min_planarity,
                                              min_change,
                                              max_iterations);                                       
    return H;
}

Eigen::MatrixXd ConvertPCToEigen(k4a_image_t &pc, Eigen::Matrix4d& ex, cv::Mat &matte) {
    /* count number of valid points in point cloud */
#define IND_IM(i,j,w,h) (((j)*(w)) + (i))
    int w, h;
    w = matte.cols;
    h = matte.rows;
    int16_t* pcData = (int16_t*)k4a_image_get_buffer(pc);
    int numPoints = 0;
    for (int j = 1; j < matte.rows; j++) {
        for (int i = 0; i < matte.cols - 1; i++) {
            int ind = IND_IM(i, j, w, h);
            Eigen::Vector3d pts[4];
            Eigen::Vector3d pts2[4];
            cv::Vec3b m = matte.at<cv::Vec3b>(j, i);  // get matte pixel value
            int matteVal = (int)m[0];
            if (matteVal > 200) {
                numPoints++;
                //int pcIndex = 3 * (i + j * matte.cols);
                //float PCX = (float)(pcData[pcIndex + 0]) / 1000.f;
                //float PCY = (float)(pcData[pcIndex + 1]) / 1000.f;
                //float PCZ = (float)(pcData[pcIndex + 2]) / 1000.f;
            }
        }
    }
    std::cout << "numPoints=" << numPoints << std::endl;
    /* allocate eigen matrix of N x 3 */
    /* add points to eigen matrix */
    /* return matrix */
    Eigen::MatrixXd X(numPoints, 3);
    int n = 0;
    for (int j = 1; j < matte.rows; j++) {
        for (int i = 0; i < matte.cols - 1; i++) {
            int ind = IND_IM(i, j, w, h);
            Eigen::Vector3d pts[4];
            Eigen::Vector3d pts2[4];
            cv::Vec3b m = matte.at<cv::Vec3b>(j, i);  // get matte pixel value
            int matteVal = (int)m[0];
            if (matteVal > 200) {
                int pcIndex = 3 * (i + j * matte.cols);
                float PCX = (float)(pcData[pcIndex + 0]) / 1000.f;
                float PCY = (float)(pcData[pcIndex + 1]) / 1000.f;
                float PCZ = (float)(pcData[pcIndex + 2]) / 1000.f;
                
                Eigen::Vector4d v = ex * Eigen::Vector4d(PCX, PCY, PCZ, 1);
            
                X(n, 0) = v[0];
                X(n, 1) = v[1];
                X(n, 2) = v[2];
                n++;
            }
        }
    }

     return X;
}

int main(int argc, char** argv) {
    k4a_image_t k4a_pc = nullptr;
    Eigen::Vector3d theCenter(0, 0, 0);
    float sz = 2;
    Eigen::Vector3d theSize(sz, sz, sz);
    int res = VOXRES;
   /* theVolume = new TSDFVolume(res, res, res, theCenter, theSize);
    theVolume->SetAllVoxels(VOXEL_UNSEEN, VOXEL_MAXDIST, 0);
    theVolume->ComputeAllVoxelCenters();*/

    viewer.draw_axes = true;
    viewer.light_pos = Vector3f(3, 3, 0);
    // Adjust camera
    viewer.camera.dist_to_center = 5.f;

    /* carve */
    // load in a matte, rgb, and depth image
    std::string path = "C:\\Users\\hogue\\Desktop\\DATA\\aug19_hogue-rawsync_0\\";

    int startFRAME = 50;
    int endFRAME = 51;
    std::string fnameExtrinsics = path + "Extrinsics_Open3D.log";
    std::string obj_prefix = "frame_";
    std::string obj_filepath = path + "ply\\";
    int camID0 = 0;
    int camID1 = 1;
    int camID2 = 2;
    int camID3 = 3;
    int camID4 = 4;
    int camID5 = 5;
    std::vector<std::string> pathsINTRINSICS;
    std::vector<int>cameraIDS;


    std::string fnameIntrinsics0 = "client_0\\Intrinsics_Calib_0.json";
    std::string fnameIntrinsics1 = "client_1\\Intrinsics_Calib_1.json";
    std::string fnameIntrinsics2 = "client_2\\Intrinsics_Calib_2.json";
    std::string fnameIntrinsics3 = "client_3\\Intrinsics_Calib_3.json";
    std::string fnameIntrinsics4 = "client_4\\Intrinsics_Calib_4.json";
    std::string fnameIntrinsics5 = "client_5\\Intrinsics_Calib_5.json";

    pathsINTRINSICS.push_back(path + fnameIntrinsics0);
    cameraIDS.push_back(camID0);
    pathsINTRINSICS.push_back(path + fnameIntrinsics1);
    cameraIDS.push_back(camID1);
    pathsINTRINSICS.push_back(path + fnameIntrinsics2);
    cameraIDS.push_back(camID2);
    pathsINTRINSICS.push_back(path + fnameIntrinsics3);
    cameraIDS.push_back(camID3);
    pathsINTRINSICS.push_back(path + fnameIntrinsics4);
    cameraIDS.push_back(camID4);
    pathsINTRINSICS.push_back(path + fnameIntrinsics5);
    cameraIDS.push_back(camID5);

    k4a_transformation_t transform0 = 0;
    k4a_transformation_t transform1 = 0;
    k4a_transformation_t transform2 = 0;
    k4a_transformation_t transform3 = 0;
    k4a_transformation_t transform4 = 0;
    k4a_transformation_t transform5 = 0;
    g_transforms.push_back(transform0);
    g_transforms.push_back(transform1);
    g_transforms.push_back(transform2);
    g_transforms.push_back(transform3);
    g_transforms.push_back(transform4);
    g_transforms.push_back(transform5);

    for(int i=0;i<6;i++) g_pointClouds.push_back(k4a_pc);

    LoadExtrinsics(fnameExtrinsics);
    for (int i = 0; i < 6; i++) {
        LoadIntrinsics(pathsINTRINSICS[i], i);
    }
    float percentage = 0;
    int F = startFRAME;
    //for (int F = startFRAME; F <= endFRAME; F++) 
    {
        //percentage = ((float)(F - startFRAME) / (float)(endFRAME - startFRAME)) * 100.f;
       // std::cout << percentage << "%" << std::endl;
        // reset things for this frame
        g_tris.resize(0);
        g_tris.erase(g_tris.begin(), g_tris.end());
        g_tris.shrink_to_fit();

        std::string fnameRGB0 = "client_0\\Color_" + std::to_string(F) + ".jpg";
        std::string fnameMATTE0 = "client_0\\Color_" + std::to_string(F) + ".matte.png";
        std::string fnameDEPTH0 = "client_0\\Depth_" + std::to_string(F) + ".tiff";// 

        std::string fnameRGB1 = "client_1\\Color_" + std::to_string(F) + ".jpg";
        std::string fnameMATTE1 = "client_1\\Color_" + std::to_string(F) + ".matte.png";
        std::string fnameDEPTH1 = "client_1\\Depth_" + std::to_string(F) + ".tiff";// 

        std::string fnameRGB2 = "client_2\\Color_" + std::to_string(F) + ".jpg";
        std::string fnameMATTE2 = "client_2\\Color_" + std::to_string(F) + ".matte.png";
        std::string fnameDEPTH2 = "client_2\\Depth_" + std::to_string(F) + ".tiff";// 

        std::string fnameRGB3 = "client_3\\Color_" + std::to_string(F) + ".jpg";
        std::string fnameMATTE3 = "client_3\\Color_" + std::to_string(F) + ".matte.png";
        std::string fnameDEPTH3 = "client_3\\Depth_" + std::to_string(F) + ".tiff";// 

        std::string fnameRGB4 = "client_4\\Color_" + std::to_string(F) + ".jpg";
        std::string fnameMATTE4 = "client_4\\Color_" + std::to_string(F) + ".matte.png";
        std::string fnameDEPTH4 = "client_4\\Depth_" + std::to_string(F) + ".tiff";// 

        std::string fnameRGB5 = "client_5\\Color_" + std::to_string(F) + ".jpg";
        std::string fnameMATTE5 = "client_5\\Color_" + std::to_string(F) + ".matte.png";
        std::string fnameDEPTH5 = "client_5\\Depth_" + std::to_string(F) + ".tiff";// 

        std::vector<std::string> pathsRGB;
        std::vector<std::string> pathsMATTE;
        std::vector<std::string> pathsDEPTH;

        pathsRGB.push_back(path + fnameRGB0);
        pathsMATTE.push_back(path + fnameMATTE0);
        pathsDEPTH.push_back(path + fnameDEPTH0);

        // front side
        pathsRGB.push_back(path + fnameRGB1);
        pathsMATTE.push_back(path + fnameMATTE1);
        pathsDEPTH.push_back(path + fnameDEPTH1);

        // back angle
        pathsRGB.push_back(path + fnameRGB2);
        pathsMATTE.push_back(path + fnameMATTE2);
        pathsDEPTH.push_back(path + fnameDEPTH2);

        ////
        pathsRGB.push_back(path + fnameRGB3);
        pathsMATTE.push_back(path + fnameMATTE3);
        pathsDEPTH.push_back(path + fnameDEPTH3);

        pathsRGB.push_back(path + fnameRGB4);
        pathsMATTE.push_back(path + fnameMATTE4);
        pathsDEPTH.push_back(path + fnameDEPTH4);

        // backside
        pathsRGB.push_back(path + fnameRGB5);
        pathsMATTE.push_back(path + fnameMATTE5);
        pathsDEPTH.push_back(path + fnameDEPTH5);

        /* load in all of the files */
        for (int CAMERA = 0; CAMERA < pathsRGB.size(); CAMERA++)
        {
            int CID = cameraIDS[CAMERA];
#ifdef _VERBOSE
            std::cout << "CAMERA:" << CAMERA << std::endl;
#endif
            cv::Mat imRGB, imMATTE, imDEPTH16, imDEPTH16_transformed;
            imRGB = cv::imread(pathsRGB[CAMERA]);
            imMATTE = cv::imread(pathsMATTE[CAMERA]);
            imDEPTH16 = cv::imread(pathsDEPTH[CAMERA], cv::IMREAD_ANYDEPTH); // 16bit short

           /* transform depth to RGB size */
            imDEPTH16_transformed = cv::Mat::zeros(imRGB.rows, imRGB.cols, CV_16UC1);

            TransformDepth(CAMERA, imDEPTH16, imDEPTH16_transformed, k4aCalibrations[CID], g_pointClouds[CAMERA]);
            /* convert point cloud to eigen matrix */
            auto H = ConvertPCToEigen(g_pointClouds[CAMERA], extrinsics[CID], imMATTE);
            g_pointMatrices.push_back(H);

            // clean up memory!!!!!!
            imRGB.release();
            imMATTE.release();
            imDEPTH16.release();
            imDEPTH16_transformed.release();

        }

        Eigen::Matrix4d I,M1,M2,M3,M4,M5,M0;
        I.setIdentity();
        M0.setIdentity();
        M1.setIdentity();
        M2.setIdentity();
        M3.setIdentity();
        M4.setIdentity();
        M5.setIdentity();


        M1 =  ICP(3,1);
        std::cout << "M(3,1)=" << M1 << std::endl;
        M5 = M1 * ICP(1, 5);
        std::cout << "M(0,5)=" << M5 << std::endl;
        //Eigen::Matrix4d M15 = M1 * M5;
        M2 = ICP(0,2);

        extrinsicsNEW[0] = M0*extrinsics[0];
        extrinsicsNEW[1] = M1*extrinsics[1];
        extrinsicsNEW[2] = M2*extrinsics[2];
        extrinsicsNEW[3] = M3*extrinsics[3];
        extrinsicsNEW[4] = M4*extrinsics[4];
        extrinsicsNEW[5] = M5*extrinsics[5];

        
        AddEigenToViewer(g_pointMatrices[0], extrinsics[0], M0, 1,0,0);//red
        AddEigenToViewer(g_pointMatrices[1], extrinsics[1], M1, 0, 1, 0); //green
        AddEigenToViewer(g_pointMatrices[2], extrinsics[2], M2, 0, 0, 1); //blue
        AddEigenToViewer(g_pointMatrices[3], extrinsics[3], M3, 0, 1, 1);//cyan
        AddEigenToViewer(g_pointMatrices[4], extrinsics[4], M4, 1, 1, 1);//white
        AddEigenToViewer(g_pointMatrices[5], extrinsics[5], M5, 0.5, 0.5, 0.5); // grey

        SaveExtrinsicsLog(extrinsicsNEW);

       // viewer.add_point_cloud(g_pointMatrices[1], 1.f, 0.0f, 0.0f);
       // viewer.add_point_cloud(g_pointMatrices[3], 0.f, 1.0f, 0.0f);



        int frameNum = F;
        std::string obj_postfix = std::to_string(frameNum) + ".ply";

       // WritePLY(obj_prefix + obj_postfix, obj_filepath, g_tris);
        //        WriteOBJ(obj_prefix + obj_postfix, obj_filepath, g_tris);

    }

    // VIEWER STUFF
    // * Events: key handler
    viewer.on_key = [&](int button, input::Action action, int mods) -> bool {
        if (action != input::Action::release) {
            if (button == 'd') {

            }
            else if (button == 'e') {


            }
        }
        return true;  // Don't prevent default
    };
    // * Events: loop callback
    viewer.on_loop = [&]() -> bool {
        return false;  // True to update all meshes and camera
    };

    viewer.on_gui = [&]() -> bool {
        ImGui::SetNextWindowSize(ImVec2(200, 100));
        ImGui::Begin("Hello");
        ImGui::Text("Hello world");
        ImGui::Button("Panic button");
        ImGui::End();
        return false;  // True to update all meshes and camera
    };

    viewer.show();
}
