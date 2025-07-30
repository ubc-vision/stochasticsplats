/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.

    Modified by: Shakiba Kheradmand, 2025
*/

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#ifndef __ANDROID__
    #include <GL/glew.h>
#else
    #include <GLES3/gl3.h>
    #include <GLES3/gl3ext.h>
#endif

#include "core/program.h"
#include "core/vertexbuffer.h"
#include "core/texture.h"
#include "core/framebuffer.h"

#include "gaussiancloud.h"

namespace rgc::radix_sort
{
    struct sorter;
}


namespace splat{

class SplatRenderer
{
public:
    SplatRenderer();
    ~SplatRenderer();

    // Per-eye temporal textures for VR TAA
    struct EyeTemporalTextures {
        std::shared_ptr<Texture> warpAvgTexA;
        std::shared_ptr<Texture> warpAvgTexB;
        std::shared_ptr<Texture> warpXYZTexA;
        std::shared_ptr<Texture> warpXYZTexB;
        std::shared_ptr<Texture> currentFrameTex;
        std::shared_ptr<Texture> depthTex;
        std::shared_ptr<FrameBuffer> sceneFBO;
    };

    // Per-eye temporal state for VR TAA
    struct EyeTemporalState {
        glm::mat4 pvmat;
        glm::mat4 prev_pvmat;
        int frameCount = 0;
    };

    bool Init(std::shared_ptr<GaussianCloud> gaussianCloud,
              bool isFramebufferSRGBEnabledIn, bool useRgcSortOverrideIn,
              std::string renderMode, int ineyeCount, int inwidth, int inheight, bool taa);

    void Sort(const glm::mat4& cameraMat, const glm::mat4& projMat,
              const glm::vec2& nearFar);

    // viewport = (x, y, width, height)
    void Render(const glm::mat4& cameraMat, const glm::mat4& projMat,
                const glm::vec4& viewport, const glm::vec2& nearFar);

    // VR-specific methods
    void SetActiveEye(int eyeIndex) { activeEye = eyeIndex; }
    void SetPresentFbo(GLuint fbo) { presentFbo = fbo; }

    // Configuration methods
    void resetTemporalTextures();
    void resetTemporalTextures(int newW, int newH);

    // Public configuration (used externally)
    uint32_t numBlocksPerWorkgroup = 1024;

protected:

private:
    void Average(const glm::vec4& viewport);
    void drawFullscreenQuad();
    void bindTex2D(int loc, const std::shared_ptr<Texture>& tex);
    void runWarpPass(
        const std::shared_ptr<Texture>& inAvg,
        const std::shared_ptr<Texture>& inXYZ,
        const std::shared_ptr<Texture>& outAvg,
        const std::shared_ptr<Texture>& outXYZ,
        const glm::mat4& pv);
    void runAveragePass(
        const EyeTemporalTextures& T,
        const std::shared_ptr<Texture>& inAvg,
        const std::shared_ptr<Texture>& inXYZ,
        const std::shared_ptr<Texture>& outAvg,
        const std::shared_ptr<Texture>& outXYZ,
        const EyeTemporalState& state,
        bool viewChanged,
        const glm::vec4& viewport);    

    void BuildVertexArrayObject(std::shared_ptr<GaussianCloud> gaussianCloud);
    bool InitializeTAA();
    bool CreateTAATextureBuffers(const Texture::Params& texParams);
    bool InitializeSortingBuffers(bool useMultiRadixSort);
    bool LoadShader(std::string renderMode, bool useMultiRadixSort);

    int width = 0;
    int height = 0;

    std::shared_ptr<Program> splatProg;  
    std::shared_ptr<BufferObject> gaussianDataBuffer;
       
    std::string renderMode = "AB";
    size_t numGaussians;

    // AB parameters
    uint32_t sortCount;
    bool isFramebufferSRGBEnabled;
    bool useRgcSortOverride;

    std::vector<uint32_t> indexVec;
    std::vector<uint32_t> depthVec;
    std::vector<glm::vec4> posVec;
    std::vector<uint32_t> atomicCounterVec;

    std::shared_ptr<rgc::radix_sort::sorter> sorter;
    std::shared_ptr<Program> preSortProg;
    std::shared_ptr<Program> histogramProg;
    std::shared_ptr<Program> sortProg;

    std::shared_ptr<BufferObject> keyBuffer;
    std::shared_ptr<BufferObject> keyBuffer2;
    std::shared_ptr<BufferObject> histogramBuffer;
    std::shared_ptr<BufferObject> valBuffer;
    std::shared_ptr<BufferObject> valBuffer2;
    std::shared_ptr<BufferObject> posBuffer;
    std::shared_ptr<BufferObject> atomicCounterBuffer;
    
    
    // VR state
    int m_eyeCount = 1;
    int activeEye = 0;
    GLuint presentFbo = 0;
    std::vector<EyeTemporalTextures> eyeTextures;
    std::vector<EyeTemporalState> eyeState; 

    // TAA params
    bool taa = false;
    std::shared_ptr<Program> avgProg;
    std::shared_ptr<Program> displayProg;
    std::shared_ptr<Program> warpProg;
    std::shared_ptr<VertexArrayObject> splatVao;   

    GLuint quadVAO = 0, quadVBO = 0;
    std::shared_ptr<FrameBuffer> sumFBO;
};

}