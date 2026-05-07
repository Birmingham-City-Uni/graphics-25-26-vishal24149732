
#define _USE_MATH_DEFINES
#include <math.h>
#include <iostream>
#include <algorithm>
#include <lodepng.h>

#include "Image.hpp"
#include "LinAlg.hpp"
#include "Light.hpp"
#include "Mesh.hpp"
#include "Shading.hpp"
#include <cfloat>

enum ShadingMode {
    PHONG,
    BLINN_PHONG
};

struct Triangle {
    std::array<Eigen::Vector3f, 3> screen;
    std::array<Eigen::Vector3f, 3> verts;
    std::array<Eigen::Vector3f, 3> cam;
    std::array<Eigen::Vector3f, 3> norms;
    std::array<Eigen::Vector2f, 3> texs;
};

Eigen::Matrix4f projectionMatrix(int height, int width,
    float horzFov = 70.f * M_PI / 180.f,
    float zFar = 50.f,
    float zNear = 0.1f)
{
    float vertFov = horzFov * float(height) / width;

    Eigen::Matrix4f projection;
    projection <<
        1.0f / tanf(0.5f * horzFov), 0, 0, 0,
        0, 1.0f / tanf(0.5f * vertFov), 0, 0,
        0, 0, zFar / (zFar - zNear), -zFar * zNear / (zFar - zNear),
        0, 0, 1, 0;

    return projection;
}

void findScreenBoundingBox(const Triangle& t, int width, int height,
    int& minX, int& minY, int& maxX, int& maxY)
{
    minX = std::min({ t.screen[0].x(), t.screen[1].x(), t.screen[2].x() });
    minY = std::min({ t.screen[0].y(), t.screen[1].y(), t.screen[2].y() });
    maxX = std::max({ t.screen[0].x(), t.screen[1].x(), t.screen[2].x() });
    maxY = std::max({ t.screen[0].y(), t.screen[1].y(), t.screen[2].y() });

    minX = std::max(0, std::min(minX, width - 1));
    maxX = std::max(0, std::min(maxX, width - 1));
    minY = std::max(0, std::min(minY, height - 1));
    maxY = std::max(0, std::min(maxY, height - 1));
}

void drawTriangle(
    std::vector<uint8_t>& image,
    int width, int height,
    std::vector<float>& zBuffer,
    const Triangle& t,
    const std::vector<std::unique_ptr<Light>>& lights,
    const Eigen::Vector3f& albedo,
    const Eigen::Vector3f& specularColor,
    float specularExponent, const std::vector<uint8_t>& textureImage,
    unsigned texWidth, unsigned texHeight,
    ShadingMode shadingMode,
    const Eigen::Vector3f& camWorldPos, Eigen::Vector3f& fogColor)
{
    
    int minX, minY, maxX, maxY;
    findScreenBoundingBox(t, width, height, minX, minY, maxX, maxY);

    Eigen::Vector2f edge1 = v2(t.screen[2] - t.screen[0]);
    Eigen::Vector2f edge2 = v2(t.screen[1] - t.screen[0]);

    float triangleArea = 0.5f * vec2Cross(edge2, edge1);
    if (triangleArea < 0) return;

    for (int x = minX; x <= maxX; ++x)
        for (int y = minY; y <= maxY; ++y)
        {
            Eigen::Vector2f p(x, y);

            float a0 = 0.5f * fabsf(vec2Cross(v2(t.screen[1]) - v2(t.screen[2]), p - v2(t.screen[2])));
            float a1 = 0.5f * fabsf(vec2Cross(v2(t.screen[0]) - v2(t.screen[2]), p - v2(t.screen[2])));
            float a2 = 0.5f * fabsf(vec2Cross(v2(t.screen[0]) - v2(t.screen[1]), p - v2(t.screen[1])));

            float b0 = a0 / triangleArea;
            float b1 = a1 / triangleArea;
            float b2 = a2 / triangleArea;

            if (b0 + b1 + b2 > 1.0001f) continue;

            

            
            float d0 = t.cam[0].z();
            float d1 = t.cam[1].z();
            float d2 = t.cam[2].z();

			//this will check the division issue , if any of the vertices are behind the camera, we will skip this pixel
            if (fabs(d0) < 1e-6 || fabs(d1) < 1e-6 || fabs(d2) < 1e-6)
                continue;

            float invZ = (b0 / d0) + (b1 / d1) + (b2 / d2);
            float depth = -1.0f / invZ;

            int idx = x + y * width;
            if (depth >= zBuffer[idx]) continue;
            zBuffer[idx] = depth;

            //std::cout << depth << std::endl;

            Eigen::Vector3f worldP = depth * ((t.verts[0] * b0) / d0 + (t.verts[1] * b1) / d1 + (t.verts[2] * b2) / d2);

            Eigen::Vector3f normP = ((t.norms[0] * b0) / d0 + (t.norms[1] * b1) / d1 + (t.norms[2] * b2) / d2).normalized();

            float w0 = 1.0f / d0;
            float w1 = 1.0f / d1;
            float w2 = 1.0f / d2;

            float denom = (b0 * w0 + b1 * w1 + b2 * w2);

            Eigen::Vector2f uv =  (t.texs[0] * b0 * w0 +
                    t.texs[1] * b1 * w1 +
                    t.texs[2] * b2 * w2) / denom;

            float u = std::max(0.0f, std::min(uv.x(), 1.0f));
            float v = std::max(0.0f, std::min(uv.y(), 1.0f));

            v = 1.0f - v;

            int texX = static_cast<int>(u * (texWidth - 1));
            int texY = static_cast<int>(v * (texHeight - 1));

            texX = std::max(0, std::min(texX, (int)texWidth - 1));
            texY = std::max(0, std::min(texY, (int)texHeight - 1));

            int idxTex = (texY * texWidth + texX) * 4;

            Eigen::Vector3f texColor(
                textureImage[idxTex + 0] / 255.0f,
                textureImage[idxTex + 1] / 255.0f,
                textureImage[idxTex + 2] / 255.0f
            );

            Eigen::Vector3f color = Eigen::Vector3f::Zero();
            Eigen::Vector3f viewDir = (camWorldPos - worldP).normalized();
            //color = Eigen::Vector3f(1, 0, 0);
            for (auto& light : lights)
            {
                Eigen::Vector3f lightIntensity = light->getIntensityAt(worldP);

                if (light->getType() != Light::Type::AMBIENT)
                {
                    Eigen::Vector3f L = light->getDirection(worldP);

                    float spec;
                    if (shadingMode == PHONG)
                        spec = phongSpecularTerm(L, normP, viewDir, specularExponent);
                    else
                        spec = blinnPhongSpecularTerm(L, normP, viewDir, specularExponent);

                    Eigen::Vector3f specOut = (specularColor * spec).cwiseProduct(lightIntensity);

                    float NdotL = std::max(normP.dot(L), 0.0f);
                    Eigen::Vector3f diffOut = (texColor * NdotL).cwiseProduct(lightIntensity);

                    color += specOut + diffOut;
                }
                else
                {
                    color += lightIntensity.cwiseProduct(texColor);
                }
            }

            float fogStart = 25.0f;
            float fogEnd = 80.0f;

            float distance = (camWorldPos - worldP).norm();

            float fogFactor =
                (distance - fogStart) / (fogEnd - fogStart);

            fogFactor = std::max(0.0f,
                std::min(fogFactor, 1.0f));

            color =
                color * (1.0f - (fogFactor/0.5f)) +
                fogColor * fogFactor;

            Color c;
            c.r = std::min(powf(color.x(), 1 / 2.2f), 1.0f) * 255;
            c.g = std::min(powf(color.y(), 1 / 2.2f), 1.0f) * 255;
            c.b = std::min(powf(color.z(), 1 / 2.2f), 1.0f) * 255;
            c.a = 255;

            setPixel(image, x, y, width, height, c);
        }
}

void drawMesh(
    std::vector<uint8_t>& image,
    std::vector<float>& zBuffer,
    const Mesh& mesh,
    const Eigen::Vector3f& albedo,
    const Eigen::Vector3f& specularColor,
    float specularExponent,
    ShadingMode mode,
    const Eigen::Vector3f& camWorldPos,
    const Eigen::Matrix4f& modelToWorld,
    const Eigen::Matrix4f& worldToCam,
    const Eigen::Matrix4f& projection,
    const std::vector<uint8_t>& textureImage,
    unsigned texWidth, unsigned texHeight,
    const std::vector<std::unique_ptr<Light>>& lights,
    int width, int height, Eigen::Vector3f& fogColor)
{
    std::cout << "Faces: " << mesh.vFaces.size() << std::endl;
    std::cout << "Tex coords count: " << mesh.texs.size() << std::endl;
    for (int i = 0; i < mesh.vFaces.size(); ++i)
    {
        Triangle t;

        for (int j = 0; j < 3; ++j)
        {

            auto v = mesh.verts[mesh.vFaces[i][j]];

            if (i == 0 && j == 0) {
                std::cout << "Sample vertex (model space): "
                    << v.transpose() << std::endl;
            }
            auto n = mesh.norms[mesh.nFaces[i][j]];
            auto tCoord = mesh.texs[mesh.tFaces[i][j]];



            t.verts[j] = (modelToWorld * vec3ToVec4(v)).head<3>();
            if (i == 0 && j == 0) {
                std::cout << "Original vertex (model space): "
                    << v.transpose() << std::endl;
            }
            t.cam[j] = (worldToCam * modelToWorld * vec3ToVec4(v)).head<3>();

            Eigen::Vector4f clip =
                projection * worldToCam * modelToWorld * vec3ToVec4(v);
            clip /= clip.w();

            t.screen[j] = {
                (clip.x() + 1) * width / 2,
                (-clip.y() + 1) * height / 2,
                clip.z()
            };

            t.norms[j] =
                (modelToWorld.block<3, 3>(0, 0).inverse().transpose() * n).normalized();

            t.texs[j] = tCoord;
        }

        drawTriangle(image, width, height, zBuffer, t,
            lights, albedo, specularColor,
            specularExponent, textureImage, texWidth, texHeight, mode, camWorldPos, fogColor);
    }
}

void applyDepthOfField(
    std::vector<uint8_t>& image,
    const std::vector<float>& zBuffer,
    int width,
    int height,
    float focusDepth)
{
    std::vector<uint8_t> original = image;

    for (int y = 2; y < height - 2; y++)
    {
        for (int x = 2; x < width - 2; x++)
        {
            int idx = x + y * width;

            float depth = zBuffer[idx];

            float blurAmount =
                fabs(depth - focusDepth) * 0.18f;

            blurAmount =
                std::min(blurAmount, 4.0f);

            int radius = int(blurAmount);

            if (radius < 1)
                continue;

            Eigen::Vector3f color(0, 0, 0);
            int count = 0;

            for (int oy = -radius; oy <= radius; oy++)
            {
                for (int ox = -radius; ox <= radius; ox++)
                {
                    int sx = std::max(0,
                        std::min(x + ox, width - 1));

                    int sy = std::max(0,
                        std::min(y + oy, height - 1));

                    int sampleIdx =
                        (sy * width + sx) * 4;

                    color.x() += original[sampleIdx + 0];
                    color.y() += original[sampleIdx + 1];
                    color.z() += original[sampleIdx + 2];

                    count++;
                }
            }

            color /= float(count);

            int outIdx = idx * 4;

            image[outIdx + 0] = (uint8_t)color.x();
            image[outIdx + 1] = (uint8_t)color.y();
            image[outIdx + 2] = (uint8_t)color.z();
        }
    }
}

int main()
{
	std::cout << "Starting rasteriser..." << std::endl;
    const int finalWidth = 1920;
    const int finalHeight = 1080;

    const int scale = 2;

    const int width = finalWidth * scale;
    const int height = finalHeight * scale;

    std::vector<uint8_t> image(width * height * 4);
    std::vector<float> zBuffer(width * height, FLT_MAX);
    std::vector<float> zBuffer2(width * height, 5.0f);

    //sky background 
    std::vector<uint8_t> skyImage;
    unsigned skyWidth, skyHeight;

    unsigned skyError = lodepng::decode(
        skyImage,
        skyWidth,
        skyHeight,
        "../sky.png");

    if (skyError)
    {
        std::cout << "Sky load error: "
            << lodepng_error_text(skyError)
            << std::endl;
    }

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            int screenIdx = (y * width + x) * 4;

            float u = float(x) / width;
            float v = float(y) / height;

            int skyX = int(u * (skyWidth - 1));
            int skyY = int(v * (skyHeight - 1));

            int skyIdx = (skyY * skyWidth + skyX) * 4;

            image[screenIdx + 0] = skyImage[skyIdx + 0];
            image[screenIdx + 1] = skyImage[skyIdx + 1];
            image[screenIdx + 2] = skyImage[skyIdx + 2];
            image[screenIdx + 3] = 255;
        }
    }

    std::vector<uint8_t> textureImage;
    unsigned texWidth, texHeight;

    unsigned error = lodepng::decode(textureImage, texWidth, texHeight, "../tex.PNG");
    if (error) {
        std::cout << "Texture load error: " << lodepng_error_text(error) << std::endl;
    }

    // -------- CAMERA --------
    Eigen::Matrix4f cameraToWorld = Eigen::Matrix4f::Identity();

    Eigen::Matrix4f worldToCam = cameraToWorld.inverse();
    Eigen::Matrix4f proj = projectionMatrix(height, width);

    Eigen::Vector3f camPos =
        (cameraToWorld * Eigen::Vector4f(0, 0, 0, 1)).head<3>();

    // lights ... 
    std::vector<std::unique_ptr<Light>> lights;

    lights.emplace_back(new AmbientLight(Eigen::Vector3f(0.12f, 0.09f, 0.08f)));

    // Sun light
    lights.emplace_back(new DirectionalLight(
        Eigen::Vector3f(1.8f, 1.1f, 0.55f),
        Eigen::Vector3f(-1, -1, -0.5f)));

    // Rim light
    /*lights.emplace_back(new PointLight(
        Eigen::Vector3f(2.5f, 2.0f, 2.0f),
        Eigen::Vector3f(1.5f, 1.5f, 2.0f)));*/

    //warm fog
    Eigen::Vector3f fogColor(0.85f, 0.65f, 0.5f);
    

    // loading the models
    
    Mesh spiderman = loadMeshFile("../models/sm_final6.obj");

    Eigen::Vector3f minV(FLT_MAX, FLT_MAX, FLT_MAX);
    Eigen::Vector3f maxV(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    for (const auto& v : spiderman.verts) {
        minV = minV.cwiseMin(v);
        maxV = maxV.cwiseMax(v);
    }

    Eigen::Vector3f center = (minV + maxV) * 0.5f;

    std::cout << "Model center: " << center.transpose() << std::endl;


    Eigen::Matrix4f model =

        translationMatrix(Eigen::Vector3f(-8.0f, -8.0f, 0.0f)) *
        rotateXMatrix(0.1f) *
        translationMatrix(-center)*
        scaleMatrix(5.0f);

   

    drawMesh(image, zBuffer, spiderman,
        Eigen::Vector3f(1.0f, 1.0f, 1.0f), 
        Eigen::Vector3f(1.2f, 1.2f, 1.2f),
        300.0f,
        ShadingMode::BLINN_PHONG,
        camPos,
        model,
        worldToCam,
        proj,
		textureImage,
		texWidth, texHeight,
        lights,
        width,
        height, fogColor);

    // building models
    std::vector<uint8_t> textureImage2;
    unsigned texWidth2, texHeight2;

    unsigned error2 = lodepng::decode(textureImage2, texWidth2, texHeight2, "../build1Tex.PNG");
    if (error2) {
        std::cout << "Texture load error: " << lodepng_error_text(error) << std::endl;
    }
    Mesh build2 = loadMeshFile("../models/build1.obj");

    Eigen::Matrix4f model2 =

        translationMatrix(Eigen::Vector3f(85.0f, -35.0f, -10.0f)) *
        rotateXMatrix(0.1f) *
        scaleMatrix(50.0f);



    drawMesh(image, zBuffer, build2,
        Eigen::Vector3f(1.0f, 1.0f, 1.0f),   
        Eigen::Vector3f(1.2f, 1.2f, 1.2f),
        300.0f,
        ShadingMode::BLINN_PHONG,
        camPos,
        model2,
        worldToCam,
        proj,
        textureImage2,
        texWidth2, texHeight2,
        lights,
        width,
        height, fogColor);

    //------
    std::vector<uint8_t> textureImage3;
    unsigned texWidth3, texHeight3;

    unsigned error3 = lodepng::decode(textureImage3, texWidth3, texHeight3, "../build2Tex.PNG");
    if (error3) {
        std::cout << "Texture load error: " << lodepng_error_text(error) << std::endl;
    }
    Mesh build3 = loadMeshFile("../models/build2.obj");

    Eigen::Matrix4f model3 =

        translationMatrix(Eigen::Vector3f(-3.0f, -10.0f, -10.0f)) *
        rotateXMatrix(0.1f) *
        scaleMatrix(5.0f);



    drawMesh(image, zBuffer, build3,
        Eigen::Vector3f(1.0f, 1.0f, 1.0f),   
        Eigen::Vector3f(1.2f, 1.2f, 1.2f),
        300.0f,
        ShadingMode::BLINN_PHONG,
        camPos,
        model3,
        worldToCam,
        proj,
        textureImage3,
        texWidth3, texHeight3,
        lights,
        width,
        height, fogColor);

    //------
    std::vector<uint8_t> textureImage4;
    unsigned texWidth4, texHeight4;

    unsigned error4 = lodepng::decode(textureImage4, texWidth4, texHeight4, "../build3Tex.PNG");
    if (error4) {
        std::cout << "Texture load error: " << lodepng_error_text(error4) << std::endl;
    }
    Mesh build4 = loadMeshFile("../models/build3.obj");

    Eigen::Matrix4f model4 =

        translationMatrix(Eigen::Vector3f(0.0f, -10.0f, -10.0f))*
        rotateXMatrix(0.1f) *
        scaleMatrix(5.0f);



    drawMesh(image, zBuffer, build4,
        Eigen::Vector3f(1.0f, 1.0f, 1.0f),   
        Eigen::Vector3f(1.2f, 1.2f, 1.2f),
        300.0f,
        ShadingMode::BLINN_PHONG,
        camPos,
        model4,
        worldToCam,
        proj,
        textureImage4,
        texWidth4, texHeight4,
        lights,
        width,
        height, fogColor);

    //------
    std::vector<uint8_t> textureImage5;
    unsigned texWidth5, texHeight5;

    unsigned error5 = lodepng::decode(textureImage5, texWidth5, texHeight5, "../build4Tex.PNG");
    if (error5) {
        std::cout << "Texture load error: " << lodepng_error_text(error5) << std::endl;
    }
    Mesh build5 = loadMeshFile("../models/build4.obj");

    Eigen::Matrix4f model5 =

        translationMatrix(Eigen::Vector3f(5.0f, -10.0f, -10.0f)) *
        rotateXMatrix(0.1f) *
        scaleMatrix(5.0f);



    drawMesh(image, zBuffer2, build5,
        Eigen::Vector3f(1.0f, 1.0f, 1.0f),   
        Eigen::Vector3f(1.2f, 1.2f, 1.2f),
        300.0f,
        ShadingMode::BLINN_PHONG,
        camPos,
        model5,
        worldToCam,
        proj,
        textureImage5,
        texWidth5, texHeight5,
        lights,
        width,
        height, fogColor);


    std::cout << "Model center: " << center.transpose() << std::endl;

    applyDepthOfField(
        image,
        zBuffer,
        width,
        height,
        12.0f);

    std::vector<uint8_t> finalImage(finalWidth * finalHeight * 4);

    for (int y = 0; y < finalHeight; y++)
    {
        for (int x = 0; x < finalWidth; x++)
        {
            Eigen::Vector3f color(0, 0, 0);

            for (int oy = 0; oy < scale; oy++)
            {
                for (int ox = 0; ox < scale; ox++)
                {
                    int srcX = x * scale + ox;
                    int srcY = y * scale + oy;

                    int srcIdx = (srcY * width + srcX) * 4;

                    color.x() += image[srcIdx + 0];
                    color.y() += image[srcIdx + 1];
                    color.z() += image[srcIdx + 2];
                }
            }

            color /= float(scale * scale);

            int dstIdx = (y * finalWidth + x) * 4;

            finalImage[dstIdx + 0] = (uint8_t)color.x();
            finalImage[dstIdx + 1] = (uint8_t)color.y();
            finalImage[dstIdx + 2] = (uint8_t)color.z();
            finalImage[dstIdx + 3] = 255;
        }
    }

    
    lodepng::encode("output.png", finalImage, finalWidth, finalHeight);

    return 0;
}