#include <Eigen/Dense>
#include <lodepng.h>
#include <json/json.hpp>
#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include "BVHNode.hpp"
#include "Triangle.hpp"
#include "Scene.hpp"
#include "Camera.hpp"
#include "PointLight.hpp"
#include "DirectionalLight.hpp"
#include "LambertianShader.hpp"
#include "TexturedLambertianShader.hpp"
#include "PhongShader.hpp"
#include "MirrorShader.hpp"
#include "TexCoordTestShader.hpp"
#include "Model.hpp"
#include <fstream>

/// <summary>
/// Load a JSON config file using the nlohmann library.
/// </summary>
nlohmann::json loadConfig(const std::string& filename)
{
	std::ifstream configStream(filename);
	nlohmann::json config = nlohmann::json::parse(configStream);
	return config;
}

/// <summary>
/// Load an Eigen Vector3f from a config file.
/// Call as for example loadVec3FromConfig(config["myVector3"]);
/// </summary>
Eigen::Vector3f loadVec3FromConfig(const nlohmann::json& config)
{
	return Eigen::Vector3f(config[0], config[1], config[2]);
}

int main(int argc, char* argv[]) {

	// *** Load the config file ***
	auto config = loadConfig("../config/config.json");

	const int pixHeight = config["pixHeight"], pixWidth = config["pixWidth"];
	const int nChannels = 4;

	// *** Set up camera and output image ***
	Camera cam(
		loadVec3FromConfig(config["cameraPos"]),
		loadVec3FromConfig(config["cameraForward"]),
		loadVec3FromConfig(config["cameraUp"]),
		pixWidth, pixHeight,
		config["cameraFov"]);


	std::vector<uint8_t> outImage(pixHeight * pixWidth * nChannels);

	Eigen::Vector3f
		red(1.f, 0.f, 0.f),
		blue(0.f, 0.f, 1.f),
		aqua(0.f, .8f, .8f),
		lavender(178.f / 255.f, 164.f / 255.f, 212.f / 255.f);

	// *** Load shaders and textures ***
	std::vector<uint8_t> skyTex;
	unsigned int width6, height6;
	lodepng::decode(skyTex, width6, height6, "../models/sky.png");


	std::vector<uint8_t> spiderTexture;
	unsigned int width, height;
	lodepng::decode(spiderTexture, width, height, "../models/tex.png");

	std::vector<uint8_t> buildTex1;
	unsigned int width2, height2;
	lodepng::decode(buildTex1, width2, height2, "../models/build1Tex.png");

	std::vector<uint8_t> buildTex2;
	unsigned int width3, height3;
	lodepng::decode(buildTex2, width3, height3, "../models/build2Tex.png");

	std::vector<uint8_t> buildTex3;
	unsigned int width4, height4;
	lodepng::decode(buildTex3, width4, height4, "../models/build3Tex.png");

	std::vector<uint8_t> buildTex4;
	unsigned int width5, height5;
	lodepng::decode(buildTex4, width5, height5, "../models/build4Tex.png");

	LambertianShader redLambertianShader(red);
	PhongShader bluePlasticShader(blue, Eigen::Vector3f(1.f, 1.f, 1.f), 100.f);
	LambertianShader aquaLambertianShader(aqua);
	LambertianShader lavenderLambertianShader(lavender);
	TexturedLambertianShader spiderShader(&spiderTexture, width, height);
	TexturedLambertianShader build1Shader(&buildTex1, width2, height2);
	TexturedLambertianShader build2Shader(&buildTex2, width3, height3);
	TexturedLambertianShader build3Shader(&buildTex3, width4, height4);
	TexturedLambertianShader build4Shader(&buildTex4, width5, height5);
	MirrorShader mirrorShader;
	TexCoordTestShader texCoordTestShader;

	// *** Set up scene ***
	Scene scene;

	// Optional code: here's how to add the spider mesh to the scene, using a BVH
	// Try enabling this and comparing it to the non-BVH version below!
	Eigen::Matrix4f transform =
		makeTranslationMatrix(Eigen::Vector3f(-8.0f, -8.0f, -15.0f)) *
		rotateX(0.1f) * uniformScale(5.0f);
	Model spiderMan("../models/sm_final6.obj");
	scene.renderables.push_back(std::make_shared<BVHNode>(spiderMan, &spiderShader, 4,  transform));

	//-----
	Eigen::Matrix4f transform2 =
		makeTranslationMatrix(Eigen::Vector3f(85.0f, -55.0f, -8.0f)) *
		rotateX(0.1f) * uniformScale(50.0f);
	Model build1("../models/build1.obj");
	scene.renderables.push_back(std::make_shared<BVHNode>(build1, &build1Shader, 4, transform2));

	Eigen::Matrix4f transform3 =
		makeTranslationMatrix(Eigen::Vector3f(-3.0f, -10.0f, -8.0f)) *
		rotateX(0.1f) * uniformScale(5.0f);
	Model build2("../models/build2.obj");
	scene.renderables.push_back(std::make_shared<BVHNode>(build2, &build2Shader, 4, transform3));

	Eigen::Matrix4f transform4 =
		makeTranslationMatrix(Eigen::Vector3f(0.0f, -10.0f, -8.0f)) *
		rotateX(0.1f) * uniformScale(5.0f);
	Model build3("../models/build3.obj");
	scene.renderables.push_back(std::make_shared<BVHNode>(build3, &build3Shader, 4, transform4));

	Eigen::Matrix4f transform5 =
		makeTranslationMatrix(Eigen::Vector3f(5.0f, -10.0f, -8.0f)) *
		rotateX(0.1f) * uniformScale(5.0f);
	Model build4("../models/build4.obj");
	scene.renderables.push_back(std::make_shared<BVHNode>(build4, &build4Shader, 4, transform5));


	// Here's how to add the mesh without using the BVH.
	// Try comparing performance to the BVH version above.
	//Model spotModel("../models/spot.obj");
	//scene.renderables.push_back(std::make_shared<Mesh>(&spotShader, &spotModel));
	//scene.renderables.back()->modelToWorld(rotateY(M_PI / 4.0f));

	// *** Add lights to scene ***
	Eigen::Vector3f ambientLight(.1f, .1f, .1f);

	std::vector<std::unique_ptr<Light>> lightSources;
	lightSources.push_back(std::make_unique<PointLight>(Eigen::Vector3f(-1.f, 3.f, -1.f), 3.f * Eigen::Vector3f(1.f, 1.f, 1.f)));
	lightSources.push_back(std::make_unique<DirectionalLight>(Eigen::Vector3f(0.f, -1.f, 1.f), .5f * Eigen::Vector3f(1.f, 1.f, 1.f)));

	// *** Render the scene ***

	// Shuffling the scanline order gets better CPU usage between threads
	// when some lines take longer to render than others.
	std::vector<unsigned int> scanlines(pixHeight);
	for (int i = 0; i < pixHeight; ++i) scanlines[i] = i;

	if (config["shuffleScanlines"]) {
		std::random_device rd;
		std::mt19937 g(rd());
		std::shuffle(scanlines.begin(), scanlines.end(), g);
	}

	auto startTime = std::chrono::steady_clock::now();

	Ray ray = cam.getRay(531, 325);
	HitInfo hitInfo;
	scene.intersect(ray, 1e-6f, 1e6f, hitInfo, VISIBLE_BITMASK);
	float x = hitInfo.hitT;

	const int samplesPerPixel = 4;

	Eigen::Vector3f fogColor(0.7f, 0.7f, 0.75f);
	/*float fogStart = 25.0f;
	float fogEnd = 80.0f;*/
	float fogDensity = 0.03f;
	float fogHeightFalloff = 0.15f;
	float fogStrength = 0.4f;

	float aperture = 0.1f;
	float focusDistance = 15.0f;
	Eigen::Vector3f camForward = loadVec3FromConfig(config["cameraForward"]).normalized();
	Eigen::Vector3f camRight =camForward.cross(loadVec3FromConfig(config["cameraUp"])).normalized();
	Eigen::Vector3f camUp =camRight.cross(camForward).normalized();

	#pragma omp parallel for
	for (int y = 0; y < pixHeight; ++y) {
		for (int x = 0; x < pixWidth; ++x) {
			Eigen::Vector3f finalColor(0.f, 0.f, 0.f);
			bool hit = false;
			for (int s = 0; s < samplesPerPixel; ++s)
			{
				//takes random offset inside each pixel
				float offsetX = (float)rand() / RAND_MAX;
				float offsetY = (float)rand() / RAND_MAX;

				//depth of field. creates a lens, adds a new orgin to the lens and changes the ray direction to the focus point from that lens.
				Ray centerRay = cam.getRay(x + offsetX,scanlines[y] + offsetY);
				Eigen::Vector3f focusPoint =centerRay.origin +centerRay.direction * focusDistance;

				float lensX = (((float)rand() / RAND_MAX) - 0.5f) * aperture;

				float lensY = (((float)rand() / RAND_MAX) - 0.5f) * aperture;

				Eigen::Vector3f newOrigin =centerRay.origin +camRight * lensX +camUp * lensY;

				Eigen::Vector3f newDirection =(focusPoint - newOrigin).normalized();

				Ray ray;
				ray.origin = newOrigin;
				ray.direction = newDirection;


				HitInfo hitInfo;

				if (scene.intersect(ray, 1e-6f, 1e6f, hitInfo, VISIBLE_BITMASK))
				{
					Eigen::Vector3f color =hitInfo.shader->getColor(hitInfo,&scene,lightSources,ambientLight,0,config["maxBounces"]);

					if (hitInfo.shader != &spiderShader)
					{
						float distance = hitInfo.hitT;
						Eigen::Vector3f worldPos = ray.origin + ray.direction * distance; 

						float heightFog = exp(-worldPos.y() * fogHeightFalloff);
						
						float distanceFog = 1.0f - exp(-distance * fogDensity);

						//float fogFactor = (distance - fogStart) / (fogEnd - fogStart);

						float fogFactor = distanceFog * heightFog;

						fogFactor = std::min(std::max(fogFactor, 0.f), 1.f);

						fogFactor *= fogStrength;

						color = (1.0f - fogFactor) * color + fogFactor * fogColor;
					}

					finalColor += color;
					hit = true;
				}
			}
			if(hit) 
			{
				finalColor /= float(samplesPerPixel);

				finalColor.x() = std::min(finalColor.x(), 1.f);
				finalColor.y() = std::min(finalColor.y(), 1.f);
				finalColor.z() = std::min(finalColor.z(), 1.f);

				int line = (pixHeight - scanlines[y]) - 1;

				outImage[(x + line * pixWidth) * nChannels + 0] =finalColor.x() * 255;

				outImage[(x + line * pixWidth) * nChannels + 1] =finalColor.y() * 255;

				outImage[(x + line * pixWidth) * nChannels + 2] =finalColor.z() * 255;

				outImage[(x + line * pixWidth) * nChannels + 3] = 255;
			}
			else 
			{
				int line = (pixHeight - scanlines[y]) - 1;

				
				float u = float(x) / float(pixWidth);
				float v = float(line) / float(pixHeight);

				int texX = std::min(int(u * width6), int(width6 - 1));
				int texY = std::min(int(v * height6), int(height6 - 1));

				int bgIndex = (texX + texY * width6) * 4;

				outImage[(x + line * pixWidth) * nChannels + 0] =skyTex[bgIndex + 0];

				outImage[(x + line * pixWidth) * nChannels + 1] =skyTex[bgIndex + 1];

				outImage[(x + line * pixWidth) * nChannels + 2] =skyTex[bgIndex + 2];

				outImage[(x + line * pixWidth) * nChannels + 3] = 255;
			}
		}
		if (omp_get_thread_num() == omp_get_num_threads()-1) {
			std::clog << "\rScanlines remaining: " << (pixHeight - y) << ' ' << std::flush;
		}

	}

	auto renderTime = std::chrono::steady_clock::now() - startTime;

	std::cout << "Render duration " << std::chrono::duration_cast<std::chrono::milliseconds>(renderTime).count() * 1e-3f << " seconds." << std::endl;

	// *** Save the output image ***
	int errorCode;
	errorCode = lodepng::encode(config["outputFilename"], outImage, pixWidth, pixHeight);
	if (errorCode) { // check the error code, in case an error occurred.
		std::cout << "lodepng error encoding image: " << lodepng_error_text(errorCode) << std::endl;
		return errorCode;
	}

	return 0;
}
