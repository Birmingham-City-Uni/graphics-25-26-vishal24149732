// This define is necessary to get the M_PI constant.
#define _USE_MATH_DEFINES
#include <math.h>

#include <iostream>
#include <lodepng.h>
#include "Mesh.hpp"


Eigen::Vector4f vec3ToVec4(const Eigen::Vector3f& v)
{
	Eigen::Vector4f output;
	output << v.x(), v.y(), v.z(), 1.0f;
	return output;
}


float vec2Cross(const Eigen::Vector2f& v0, const Eigen::Vector2f& v1)
{
	return v0.x() * v1.y() - v0.y() * v1.x();
}

void setPixel(std::vector<uint8_t>& image, int x, int y, int width, int height, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
{
	int pixelIdx = x + y * width;
	image[pixelIdx * 4 + 0] = r;
	image[pixelIdx * 4 + 1] = g;
	image[pixelIdx * 4 + 2] = b;
	image[pixelIdx * 4 + 3] = a;
}

void drawTriangle(std::vector<uint8_t>& image, int width, int height,
	const Eigen::Vector2f& p0, const Eigen::Vector2f& p1, const Eigen::Vector2f& p2,
	uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
{
	
	int minX, minY, maxX, maxY;
	minX = std::min(std::min(p0.x(), p1.x()), p2.x());
	minY = std::min(std::min(p0.y(), p1.y()), p2.y());
	maxX = std::max(std::max(p0.x(), p1.x()), p2.x());
	maxY = std::max(std::max(p0.y(), p1.y()), p2.y());

	
	minX = std::min(std::max(minX, 0), width);
	maxX = std::min(std::max(maxX, 0), width);
	minY = std::min(std::max(minY, 0), height);
	maxY = std::min(std::max(maxY, 0), height);

	Eigen::Vector2f edge1 = p1 - p0;
	Eigen::Vector2f edge2 = p2 - p0;
	float triangleArea = 0.5f * vec2Cross(edge2, edge1);
	if (triangleArea < 0) {
		
		return;
	}

	for (int x = minX; x <= maxX; ++x)
		for (int y = minY; y <= maxY; ++y) {
			Eigen::Vector2f p(x, y);

			float a0 = 0.5f * fabsf(vec2Cross(p1 - p2, p - p2));
			float a1 = 0.5f * fabsf(vec2Cross(p0 - p2, p - p2));
			float a2 = 0.5f * fabsf(vec2Cross(p0 - p1, p - p1));

			float b0 = a0 / triangleArea;
			float b1 = a1 / triangleArea;
			float b2 = a2 / triangleArea;

			float sum = b0 + b1 + b2;
			if (sum > 1.0001) {
				continue;
			}

			setPixel(image, x, y, width, height, r, g, b, a);
		}
}


void drawMesh(std::vector<unsigned char>& image, const Mesh& mesh,
	const Eigen::Vector3f& baseColor, const Eigen::Matrix4f& transform,
	int width, int height)
{
	for (const auto& face : mesh.faces) {
		Eigen::Vector3f
			v0 = mesh.verts[face[0]],
			v1 = mesh.verts[face[1]],
			v2 = mesh.verts[face[2]];

		Eigen::Vector4f tv0, tv1, tv2;

		
		Eigen::Vector4f v0h = vec3ToVec4(v0);
		Eigen::Vector4f v1h = vec3ToVec4(v1);
		Eigen::Vector4f v2h = vec3ToVec4(v2);
		tv0 = transform * v0h;
		tv1 = transform * v1h;
		tv2 = transform * v2h;
		

		Eigen::Vector2f p0(tv0.x() * 250 + width / 2, -tv0.y() * 250 + height / 2);
		Eigen::Vector2f p1(tv1.x() * 250 + width / 2, -tv1.y() * 250 + height / 2);
		Eigen::Vector2f p2(tv2.x() * 250 + width / 2, -tv2.y() * 250 + height / 2);


		Eigen::Vector3f edge1 = tv1.block<3, 1>(0, 0) - tv0.block<3, 1>(0, 0);
		Eigen::Vector3f edge2 = tv2.block<3, 1>(0, 0) - tv0.block<3, 1>(0, 0);
		Eigen::Vector3f normal = edge1.cross(edge2).normalized();

		float intensity = normal.dot(Eigen::Vector3f(0, 0, 1));
		if (intensity > 0.f) {
			drawTriangle(image, width, height, p0, p1, p2,
				baseColor.x() * intensity * 255,
				baseColor.y() * intensity * 255,
				baseColor.z() * intensity * 255);
		}
	}
}


Eigen::Matrix4f translationMatrix(const Eigen::Vector3f& t)
{
	
	Eigen::Matrix4f M = Eigen::Matrix4f::Identity();
	M(0, 3) = t.x();
	M(1, 3) = t.y();
	M(2, 3) = t.z();
	return M;
}


Eigen::Matrix4f scaleMatrix(float s)
{
	// *** Your code here ***
	Eigen::Matrix4f M = Eigen::Matrix4f::Identity();
	M(0, 0) = s;
	M(1, 1) = s;
	M(2, 2) = s;
	return M;
}


Eigen::Matrix4f rotateYMatrix(float theta)
{
	
	Eigen::Matrix4f M = Eigen::Matrix4f::Identity();
	M(0, 0) = cosf(theta);
	M(0, 2) = sinf(theta);
	M(2, 0) = -sinf(theta);
	M(2, 2) = cosf(theta);
	return M;
}

int main()
{

	std::string outputFilename = "output.png";

	const int width = 512, height = 512;
	const int nChannels = 4;

	std::vector<uint8_t> imageBuffer(height * width * nChannels);

	
	memset(&imageBuffer[0], 0, width * height * nChannels * sizeof(uint8_t));

	std::string bunnyFilename = "../models/stanford_bunny_simplified.obj";
	std::string dragonFilename = "../models/stanford_dragon_simplified.obj";
	std::string spiderManFilename = "../models/sm.obj";

	Mesh bunnyMesh = loadMeshFile(bunnyFilename);
	Mesh dragonMesh = loadMeshFile(dragonFilename);
	Mesh spiderManMesh = loadMeshFile(spiderManFilename);


	

	Eigen::Matrix4f bunnyTransform = Eigen::Matrix4f::Identity();
	Eigen::Matrix4f dragonTransform = Eigen::Matrix4f::Identity();
	Eigen::Matrix4f spiderManTransform = Eigen::Matrix4f::Identity();
	std::cout << "huh: " << bunnyTransform << std::endl;
	bunnyTransform =
		translationMatrix(Eigen::Vector3f(-0.5f, -0.2f, 0.0f)) * rotateYMatrix(0.5f) * scaleMatrix(0.01f);
	std::cout << "myMat4: " << bunnyTransform << std::endl;
	dragonTransform =
		translationMatrix(Eigen::Vector3f(0.5f, 0.2f, 0.0f)) * rotateYMatrix(1.0f) * scaleMatrix(0.01f);
	spiderManTransform =
		translationMatrix(Eigen::Vector3f(0.0f, 0.0f, 0.0f)) * rotateYMatrix(0.5f) * scaleMatrix(0.5f);

	

	drawMesh(imageBuffer, bunnyMesh, Eigen::Vector3f(0, 1, 0), bunnyTransform, width, height);
	drawMesh(imageBuffer, dragonMesh, Eigen::Vector3f(0, 1, 1), dragonTransform, width, height);
	drawMesh(imageBuffer, spiderManMesh, Eigen::Vector3f(0, 1, 1), spiderManTransform, width, height);

	
	int errorCode;
	errorCode = lodepng::encode(outputFilename, imageBuffer, width, height);
	if (errorCode) { 
		std::cout << "lodepng error encoding image: " << lodepng_error_text(errorCode) << std::endl;
		return errorCode;
	}

	return 0;
}