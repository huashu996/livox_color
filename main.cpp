#include <opencv2/opencv.hpp>
#include <cv_bridge/cv_bridge.h>

#include <pcl/filters/passthrough.h>
#include <pcl/common/transforms.h>
#include <pcl/point_types.h>
#include <pcl/pcl_macros.h>
#include <pcl/point_cloud.h>
#include <pcl/io/pcd_io.h>
#include <pcl_conversions/pcl_conversions.h>

#include <Eigen/Core>
#include <Eigen/Geometry>

#include <ros/ros.h>
#include <image_transport/image_transport.h>
#include <sensor_msgs/PointCloud2.h>

#define Hmax 480
#define Wmax 640
#define H Hmax
#define W Wmax

/* 自定义的PointXYZRGBIL（pcl没有PointXYZRGBIL、PointXYZRGBI结构体）*/
struct PointXYZRGBIL
{
	PCL_ADD_POINT4D;
	PCL_ADD_RGB;
	uint32_t label;
	PCL_ADD_INTENSITY;
	EIGEN_MAKE_ALIGNED_OPERATOR_NEW;
} EIGEN_ALIGN16;
POINT_CLOUD_REGISTER_POINT_STRUCT(
	PointXYZRGBIL,
	(float, x, x)(float, y, y)(float, z, z)(float, rgb, rgb)(uint32_t, label, label)(float, intensity, intensity))

typedef PointXYZRGBIL PointType;
// 使用pcl::PointXYZRGBNormal结构体代替自定义的PointXYZRGBI，为了livox_color_mapping建图，自定义结构体不支持很多内置函数
// typedef pcl::PointXYZRGBNormal PointType;

//全局变量都能访问，图像回调中写，点云回调中读
cv::Vec3b image_color[H][W];

// 内外参
cv::Mat distCoeffs(5, 1, cv::DataType<double>::type);	   // 畸变向量
cv::Mat intrisic = cv::Mat::eye(3, 3, CV_64F);			   // 内参3*3矩阵
cv::Mat intrisicMat(3, 4, cv::DataType<double>::type);	   // 内参3*4的投影矩阵，最后一列是三个零
cv::Mat un_intrisicMat(3, 3, cv::DataType<double>::type);	   // 内参3*4的投影矩阵，最后一列是三个零
cv::Mat extrinsicMat_RT(4, 4, cv::DataType<double>::type); // 外参旋转矩阵3*3和平移向量3*1
void CalibrationData(void)
{
	// extrinsic
	// 0.0451423  -0.998715  0.0230348  0.00925535
	// 0.0558064  -0.0205011  -0.998231  0.0499455
	// 0.997421  0.046348  0.0548092  0.42788
	// 0  0  0  1
	extrinsicMat_RT.at<double>(0, 0) = 0.0271569;
	extrinsicMat_RT.at<double>(0, 1) = -0.999612;
	extrinsicMat_RT.at<double>(0, 2) =0.00576845;
	extrinsicMat_RT.at<double>(0, 3) = -0.239215;
	extrinsicMat_RT.at<double>(1, 0) = 0.00747275;
	extrinsicMat_RT.at<double>(1, 1) = 0.00597341;
	extrinsicMat_RT.at<double>(1, 2) = -0.999954;
	extrinsicMat_RT.at<double>(1, 3) = 0.0812632;
	extrinsicMat_RT.at<double>(2, 0) = 0.999603;
	extrinsicMat_RT.at<double>(2, 1) = 0.0271125;
	extrinsicMat_RT.at<double>(2, 2) = 0.00763212;
	extrinsicMat_RT.at<double>(2, 3) = -0.741648;
	extrinsicMat_RT.at<double>(3, 0) = 0.0;
	extrinsicMat_RT.at<double>(3, 1) = 0.0;
	extrinsicMat_RT.at<double>(3, 2) = 0.0;
	extrinsicMat_RT.at<double>(3, 3) = 1.0;

	// intrinsic
	// 2875.097131590431  0  1369.668059923329;
	//  0  2896.420251825658  1114.244269170673;
	//  0  0  1
	// ditortion
	// -0.008326874784366894 -0.06967846599874981 0.006185220615585947 -0.01133018681519818  0.5462976722456516
	intrisicMat.at<double>(0, 0) = intrisic.at<double>(0, 0) = 428.089;
	intrisicMat.at<double>(0, 1) = 0.000000e+00;
	intrisicMat.at<double>(0, 2) = intrisic.at<double>(0, 2) = 306.853;
	intrisicMat.at<double>(0, 3) = 0.000000e+00;
	intrisicMat.at<double>(1, 0) = 0.000000e+00;
	intrisicMat.at<double>(1, 1) = intrisic.at<double>(1, 1) = 437.83;
	intrisicMat.at<double>(1, 2) = intrisic.at<double>(1, 2) = 248.373;
	intrisicMat.at<double>(1, 3) = 0.000000e+00;
	intrisicMat.at<double>(2, 0) = 0.000000e+00;
	intrisicMat.at<double>(2, 1) = 0.000000e+00;
	intrisicMat.at<double>(2, 2) = 1.000000e+00;
	intrisicMat.at<double>(2, 3) = 0.000000e+00;
	un_intrisicMat.at<double>(0, 0) = intrisic.at<double>(0, 0) = 428.089;
	un_intrisicMat.at<double>(0, 1) = 0.000000e+00;
	un_intrisicMat.at<double>(0, 2) = intrisic.at<double>(0, 2) = 306.853;
	un_intrisicMat.at<double>(1, 0) = 0.000000e+00;
	un_intrisicMat.at<double>(1, 1) = intrisic.at<double>(1, 1) = 437.83;
	un_intrisicMat.at<double>(1, 2) = intrisic.at<double>(1, 2) = 248.373;
	un_intrisicMat.at<double>(2, 0) = 0.000000e+00;
	un_intrisicMat.at<double>(2, 1) = 0.000000e+00;
	un_intrisicMat.at<double>(2, 2) = 1.000000e+00;
	distCoeffs.at<double>(0) = 0.156215;
	distCoeffs.at<double>(1) = -0.156542;
	distCoeffs.at<double>(2) = -0.004833;
	distCoeffs.at<double>(3) = -0.001379;
	distCoeffs.at<double>(4) = 0;
}

class livox_lidar_color
{
public:
	ros::NodeHandle n;
	sensor_msgs::PointCloud2 msg;																									   //接收到的点云消息
	sensor_msgs::PointCloud2 fusion_msg;																							   //等待发送的点云消息
	ros::Subscriber subCloud = n.subscribe<sensor_msgs::PointCloud2>("/livox/lidar", 1, &livox_lidar_color::pointCloudCallback, this); //接收点云数据，进入回调函数pointCloudCallback
	ros::Publisher pubCloud = n.advertise<sensor_msgs::PointCloud2>("/livox/color_lidar", 1);										   //建立了一个发布器，方便之后发布加入颜色之后的点云

private:
	//点云回调函数
	void pointCloudCallback(const sensor_msgs::PointCloud2ConstPtr &laserCloudMsg)
	{
		pcl::PointCloud<pcl::PointXYZI>::Ptr raw_pcl_ptr(new pcl::PointCloud<pcl::PointXYZI>); //livox点云消息包含xyz和intensity
		pcl::fromROSMsg(*laserCloudMsg, *raw_pcl_ptr);										   //把msg消息指针转化为PCL点云
		cv::Mat X(4, 1, cv::DataType<double>::type);
		cv::Mat Y(3, 1, cv::DataType<double>::type);

		pcl::PointCloud<PointType>::Ptr fusion_pcl_ptr(new pcl::PointCloud<PointType>); //放在这里是因为，每次都需要重新初始化

		for (int i = 0; i < raw_pcl_ptr->points.size(); i++)
		{
			X.at<double>(0, 0) = raw_pcl_ptr->points[i].x;
			X.at<double>(1, 0) = raw_pcl_ptr->points[i].y;
			X.at<double>(2, 0) = raw_pcl_ptr->points[i].z;
			X.at<double>(3, 0) = 1;
			Y = intrisicMat * extrinsicMat_RT * X; //雷达坐标转换到相机坐标，相机坐标投影到像素坐标
			cv::Point pt;						   // (x,y) 像素坐标
			// Y是3*1向量，pt.x是Y的第一个值除以第三个值，pt.y是Y的第二个值除以第三个值，为什么是下面这种写法？？
			pt.x = Y.at<double>(0, 0) / Y.at<double>(2, 0);
			pt.y = Y.at<double>(1, 0) / Y.at<double>(2, 0);
			// std::cout<<Y<<pt<<std::endl;
			if (pt.x >= 0 && pt.x < W && pt.y >= 0 && pt.y < H && raw_pcl_ptr->points[i].x > 0) //&& raw_pcl_ptr->points[i].x>0去掉图像后方的点云
			{

				PointType p;
				p.x = raw_pcl_ptr->points[i].x;
				p.y = raw_pcl_ptr->points[i].y;
				p.z = raw_pcl_ptr->points[i].z;
				//点云颜色由图像上对应点确定
				p.b = image_color[pt.y][pt.x][0];
				p.g = image_color[pt.y][pt.x][1];
				p.r = image_color[pt.y][pt.x][2];
				p.intensity = raw_pcl_ptr->points[i].intensity; //继承之前点云的intensity
				fusion_pcl_ptr->points.push_back(p);
			}
		}

		fusion_pcl_ptr->width = fusion_pcl_ptr->points.size();
		fusion_pcl_ptr->height = 1;
		// std::cout<<  fusion_pcl_ptr->points.size() << std::endl;
		pcl::toROSMsg(*fusion_pcl_ptr, fusion_msg);			   //将点云转化为消息才能发布
		fusion_msg.header.frame_id = "livox_frame";			   //帧id改成和/livox/lidar一样的，同一坐标系
		fusion_msg.header.stamp = laserCloudMsg->header.stamp; // 时间戳和/livox/lidar 一致
		pubCloud.publish(fusion_msg);						   //发布调整之后的点云数据
	}
};

void imageCallback(const sensor_msgs::ImageConstPtr &msg)
{
	try
	{
		// 使用cv_bridge将ROS图像消息转换为OpenCV格式
		cv_bridge::CvImagePtr cv_ptr = cv_bridge::toCvCopy(msg, sensor_msgs::image_encodings::BGR8);
		// 获取图像数据
		cv::Mat image = cv_ptr->image;
		cv::Mat undistortedImage;
		cv::undistort(image, undistortedImage, un_intrisicMat, distCoeffs);
		for (int row = 0; row < H; row++)
		{
			for (int col = 0; col < W; col++)
			{
				image_color[row][col] = (cv::Vec3b)image.at<cv::Vec3b>(row, col);
			}
		}
	}
	catch (cv_bridge::Exception &e)
	{
		ROS_ERROR("Could not conveextrinsicMat_RT from '%s' to 'bgr8'.", msg->encoding.c_str());
	}
	
}

int main(int argc, char **argv)
{
	CalibrationData();
	ros::init(argc, argv, "livox_color");
	livox_lidar_color llc;

	ros::NodeHandle n;
	image_transport::ImageTransport it(n);
	image_transport::Subscriber sub = it.subscribe("/usb_cam/image_raw", 1, &imageCallback);
	ros::spin();
}
