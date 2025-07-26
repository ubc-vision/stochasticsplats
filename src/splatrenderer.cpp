/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.

    Modified by: Shakiba Kheradmand, 2025
*/

#include <cassert>
#include <limits>
#include <memory>
#include <random>

#include <glm/gtc/matrix_transform.hpp>

#ifndef __ANDROID__
  #include <GL/glew.h>
#else
  #include <GLES3/gl3.h>
  #include <GLES3/gl3ext.h>
#endif

#ifdef TRACY_ENABLE
  #include <tracy/Tracy.hpp>
#else
  #define ZoneScoped
  #define ZoneScopedNC(NAME, COLOR)
#endif

#include "splatrenderer.h"
#include "gaussiancloud.h"
#include "core/image.h"
#include "core/log.h"
#include "core/texture.h"
#include "core/util.h"
#include "radix_sort.hpp"

using namespace splat;

static const uint32_t NUM_BLOCKS_PER_WORKGROUP = 1024;
// Full-screen quad vertices (positions and texCoords), for triangle strip
static const float quadVertices[] = {
    // positions   // texCoords
    -1.0f,  1.0f,   0.0f, 1.0f,
    -1.0f, -1.0f,   0.0f, 0.0f,
     1.0f, -1.0f,   1.0f, 0.0f,

    -1.0f,  1.0f,   0.0f, 1.0f,
     1.0f, -1.0f,   1.0f, 0.0f,
     1.0f,  1.0f,   1.0f, 1.0f
};

static void SetupAttrib(int loc, const BinaryAttribute& attrib, int32_t count, size_t stride)
{
    assert(attrib.type == BinaryAttribute::Type::Float);
    glVertexAttribPointer(loc, count, GL_FLOAT, GL_FALSE, (uint32_t)stride, (void*)attrib.offset);
    glEnableVertexAttribArray(loc);
}

SplatRenderer::SplatRenderer()
{
}

SplatRenderer::~SplatRenderer()
{
}

bool SplatRenderer::LoadShader(std::string renderMode, bool useMultiRadixSort)
{
    if (renderMode == "AB"){
        if (!splatProg->LoadVertGeomFrag("shader/splat_vert.glsl", 
            "shader/splat_geom.glsl", "shader/splat_frag.glsl"))
        {
            Log::E("Error loading splat shaders!\n");
            return false;
        }

        preSortProg = std::make_shared<Program>();
        if (!preSortProg->LoadCompute("shader/presort_compute.glsl"))
        {
            Log::E("Error loading pre-sort compute shader!\n");
            return false;
        }

        if (useMultiRadixSort)
        {
            sortProg = std::make_shared<Program>();
            if (!sortProg->LoadCompute("shader/multi_radixsort.glsl"))
            {
                Log::E("Error loading sort compute shader!\n");
                return false;
            }

            histogramProg = std::make_shared<Program>();
            if (!histogramProg->LoadCompute("shader/multi_radixsort_histograms.glsl"))
            {
                Log::E("Error loading histogram compute shader!\n");
                return false;
            }
        }
    }  else if (renderMode == "ST") {
        if (!splatProg->LoadVertGeomFrag("shader/splat_vert.glsl",
          "shader/splat_geom.glsl",
          "shader/splat_frag_ST.glsl")) {
          Log::E("Error loading splat shaders!\n");
          return false; 
        }
      }
      else if (renderMode == "ST-popfree") {
        if (!splatProg->LoadVertGeomFrag("shader/splat_vert_ST_popfree.glsl",
          "shader/splat_geom_ST_popfree.glsl",
          "shader/splat_frag_ST.glsl")) {
          Log::E("Error loading splat shaders!\n");
          return false;
        }
      }
      if (renderMode != "AB" && taa) {
        // warp the previous average frame to current view
        warpProg = std::make_shared<Program>();
        if (!warpProg->LoadVertFrag("shader/warp_vert.glsl",
                                   "shader/warp_frag.glsl")) {
          Log::E("Error loading warp shader!\n");
          return false;
        }
        // average over the previous frame average and the current one
        avgProg = std::make_shared<Program>();
        if (!avgProg->LoadVertFrag("shader/avg_vert.glsl", 
                                   "shader/avg_frag.glsl")) {
            Log::E("Error loading avg shader!\n");
            return false;
        }
        // display the final result
        displayProg = std::make_shared<Program>();
        if (!displayProg->LoadVertFrag("shader/avg_vert.glsl", 
                                       "shader/display_frag.glsl")) {
            Log::E("Error loading display shader!\n");
            return false;
        }
      }
    return true;
}

bool SplatRenderer::Init(std::shared_ptr<GaussianCloud> gaussianCloud,
                         bool isFramebufferSRGBEnabledIn, bool useRgcSortOverrideIn,
                         std::string inrenderMode, int ineyeCount, int inwidth, int inheight, bool intaa)
{
    ZoneScopedNC("SplatRenderer::Init()", tracy::Color::Blue);
    GL_ERROR_CHECK("SplatRenderer::Init() begin");

    // Initialize member variables
    isFramebufferSRGBEnabled = isFramebufferSRGBEnabledIn;
    useRgcSortOverride = useRgcSortOverrideIn;
    bool useMultiRadixSort = GLEW_KHR_shader_subgroup && !useRgcSortOverride;
    numGaussians = gaussianCloud->GetNumGaussians();
    renderMode = inrenderMode;
    width = inwidth;
    height = inheight;
    taa = intaa;
    m_eyeCount = ineyeCount;

    splatProg = std::make_shared<Program>();
    
    if (isFramebufferSRGBEnabled || gaussianCloud->HasFullSH())
    {
        std::string defines = "";
        if (isFramebufferSRGBEnabled)
        {
            defines += "#define FRAMEBUFFER_SRGB\n";
        }
        if (gaussianCloud->HasFullSH())
        {
            defines += "#define FULL_SH\n";
        }
        splatProg->AddMacro("DEFINES", defines);
    }

    // Load shaders
    if (!LoadShader(renderMode, useMultiRadixSort)) {
        return false;
    }

    if (renderMode == "AB") {
        // Build position vector for depth sorting
        posVec.reserve(numGaussians);
        gaussianCloud->ForEachPosWithAlpha([this](const float* pos)
        {
            posVec.emplace_back(glm::vec4(pos[0], pos[1], pos[2], 1.0f));
        });
        depthVec.resize(numGaussians);
    } else if (taa) {
        if (!InitializeTAA()) {
            return false;
        }
    }

    // Build vertex array object
    BuildVertexArrayObject(gaussianCloud);

    // Initialize sorting buffers for alpha blending mode
    if (renderMode == "AB") {
        if (!InitializeSortingBuffers(useMultiRadixSort)) {
            return false;
        }
    }

    GL_ERROR_CHECK("SplatRenderer::Init() end");
    return true;
}

bool SplatRenderer::InitializeTAA()
{
    Texture::Params texParams;
    texParams.magFilter = FilterType::Nearest;
    texParams.minFilter = FilterType::Nearest;
    texParams.sWrap = WrapType::ClampToEdge;
    texParams.tWrap = WrapType::ClampToEdge;

    // Initialize quad geometry
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    if (!CreateTAATextureBuffers(texParams)) {
        return false;
    }

    for (activeEye = 0; activeEye < m_eyeCount; ++activeEye) {
        resetTemporalTextures();
    }
    activeEye = 0;

    return true;
}


bool SplatRenderer::CreateTAATextureBuffers(const Texture::Params& texParams)
{
    eyeTextures.resize(m_eyeCount);
    eyeState.resize(m_eyeCount);

    for (int eye = 0; eye < m_eyeCount; eye++) {
        // Create temporal accumulation textures
        eyeTextures[eye].warpXYZTexA = std::make_shared<Texture>(
            width, height, GL_RGBA32F, GL_RGBA, GL_FLOAT, texParams);
        eyeTextures[eye].warpAvgTexA = std::make_shared<Texture>(
            width, height, GL_RGBA32F, GL_RGBA, GL_FLOAT, texParams);
        eyeTextures[eye].warpXYZTexB = std::make_shared<Texture>(
            width, height, GL_RGBA32F, GL_RGBA, GL_FLOAT, texParams);
        eyeTextures[eye].warpAvgTexB = std::make_shared<Texture>(
            width, height, GL_RGBA32F, GL_RGBA, GL_FLOAT, texParams);
                
        // Create per-eye scene textures
        eyeTextures[eye].currentFrameTex = std::make_shared<Texture>(
            width, height, GL_RGBA32F, GL_RGBA, GL_FLOAT, texParams);
        eyeTextures[eye].depthTex = std::make_shared<Texture>(
            width, height, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT, texParams);

        // Create per-eye scene FBO
        eyeTextures[eye].sceneFBO = std::make_shared<FrameBuffer>();
        eyeTextures[eye].sceneFBO->Bind();
        eyeTextures[eye].sceneFBO->AttachColor(eyeTextures[eye].currentFrameTex, GL_COLOR_ATTACHMENT0);
        eyeTextures[eye].sceneFBO->AttachDepth(eyeTextures[eye].depthTex);

        if (!eyeTextures[eye].sceneFBO->IsComplete()) {
            Log::E("eyeTextures[%d].sceneFBO is not complete!\n", eye);
            return false;
        }
    }

    // Create FBOs for temporal accumulation
    sumFBO = std::make_shared<FrameBuffer>();
    
    // Initialize FBOs with first eye's textures (will be updated per-eye)
    sumFBO->Bind();
    sumFBO->AttachColor(eyeTextures[0].warpAvgTexB, GL_COLOR_ATTACHMENT0);
    sumFBO->AttachColor(eyeTextures[0].warpXYZTexB, GL_COLOR_ATTACHMENT1);
    
    return true;
}

bool SplatRenderer::InitializeSortingBuffers(bool useMultiRadixSort)
{
    depthVec.resize(numGaussians);

    if (useMultiRadixSort)
    {
        Log::I("using multi_radixsort.glsl\n");

        keyBuffer = std::make_shared<BufferObject>(GL_SHADER_STORAGE_BUFFER, depthVec, GL_DYNAMIC_STORAGE_BIT);
        keyBuffer2 = std::make_shared<BufferObject>(GL_SHADER_STORAGE_BUFFER, depthVec, GL_DYNAMIC_STORAGE_BIT);

        const uint32_t NUM_ELEMENTS = static_cast<uint32_t>(numGaussians);
        const uint32_t NUM_WORKGROUPS = (NUM_ELEMENTS + numBlocksPerWorkgroup - 1) / numBlocksPerWorkgroup;
        const uint32_t RADIX_SORT_BINS = 256;

        std::vector<uint32_t> histogramVec(NUM_WORKGROUPS * RADIX_SORT_BINS, 0);
        histogramBuffer = std::make_shared<BufferObject>(GL_SHADER_STORAGE_BUFFER, histogramVec, GL_DYNAMIC_STORAGE_BIT);

        valBuffer = std::make_shared<BufferObject>(GL_SHADER_STORAGE_BUFFER, indexVec, GL_DYNAMIC_STORAGE_BIT);
        valBuffer2 = std::make_shared<BufferObject>(GL_SHADER_STORAGE_BUFFER, indexVec, GL_DYNAMIC_STORAGE_BIT);
        posBuffer = std::make_shared<BufferObject>(GL_SHADER_STORAGE_BUFFER, posVec);
    }
    else
    {
        Log::I("using rgc::radix_sort\n");
        keyBuffer = std::make_shared<BufferObject>(GL_SHADER_STORAGE_BUFFER, depthVec, GL_DYNAMIC_STORAGE_BIT);
        valBuffer = std::make_shared<BufferObject>(GL_SHADER_STORAGE_BUFFER, indexVec, GL_DYNAMIC_STORAGE_BIT);
        posBuffer = std::make_shared<BufferObject>(GL_SHADER_STORAGE_BUFFER, posVec);

        sorter = std::make_shared<rgc::radix_sort::sorter>(numGaussians);
    }

    atomicCounterVec.resize(1, 0);
    atomicCounterBuffer = std::make_shared<BufferObject>(GL_ATOMIC_COUNTER_BUFFER, atomicCounterVec, GL_DYNAMIC_STORAGE_BIT | GL_MAP_READ_BIT);
    
    return true;
}

void SplatRenderer::Sort(const glm::mat4& cameraMat, const glm::mat4& projMat,
                         const glm::vec2& nearFar)
{
    if (renderMode != "AB")   return;

    ZoneScoped;
    GL_ERROR_CHECK("SplatRenderer::Sort() begin");

    const size_t numPoints = posVec.size();
    glm::mat4 modelViewMat = glm::inverse(cameraMat);

    bool useMultiRadixSort = GLEW_KHR_shader_subgroup && !useRgcSortOverride;

    // 24 bit radix sort still has some artifacts on some datasets, so use 32 bit sort.
    //const uint32_t NUM_BYTES = useMultiRadixSort ? 3 : 4;
    //const uint32_t MAX_DEPTH = useMultiRadixSort ? 16777215 : std::numeric_limits<uint32_t>::max();
    const uint32_t NUM_BYTES = 4;
    const uint32_t MAX_DEPTH = std::numeric_limits<uint32_t>::max();

    {
        ZoneScopedNC("pre-sort", tracy::Color::Red4);

        preSortProg->Bind();
        preSortProg->SetUniform("modelViewProj", projMat * modelViewMat);
        preSortProg->SetUniform("nearFar", nearFar);
        preSortProg->SetUniform("keyMax", MAX_DEPTH);

        // reset counter back to zero
        atomicCounterVec[0] = 0;
        atomicCounterBuffer->Update(atomicCounterVec);

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, posBuffer->GetObj());  // readonly
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, keyBuffer->GetObj());  // writeonly
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, valBuffer->GetObj());  // writeonly
        glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 4, atomicCounterBuffer->GetObj());

        const int LOCAL_SIZE = 256;
        glDispatchCompute(((GLuint)numPoints + (LOCAL_SIZE - 1)) / LOCAL_SIZE, 1, 1); // Assuming LOCAL_SIZE threads per group
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT);

        GL_ERROR_CHECK("SplatRenderer::Sort() pre-sort");
    }

    {
        ZoneScopedNC("get-count", tracy::Color::Green);

        atomicCounterBuffer->Read(atomicCounterVec);
        sortCount = atomicCounterVec[0];

        assert(sortCount <= (uint32_t)numPoints);

        GL_ERROR_CHECK("SplatRenderer::Render() get-count");
    }

    if (useMultiRadixSort)
    {
        ZoneScopedNC("sort", tracy::Color::Red4);

        const uint32_t NUM_ELEMENTS = static_cast<uint32_t>(sortCount);
        const uint32_t NUM_WORKGROUPS = (NUM_ELEMENTS + numBlocksPerWorkgroup - 1) / numBlocksPerWorkgroup;

        sortProg->Bind();
        sortProg->SetUniform("g_num_elements", NUM_ELEMENTS);
        sortProg->SetUniform("g_num_workgroups", NUM_WORKGROUPS);
        sortProg->SetUniform("g_num_blocks_per_workgroup", numBlocksPerWorkgroup);

        histogramProg->Bind();
        histogramProg->SetUniform("g_num_elements", NUM_ELEMENTS);
        //histogramProg->SetUniform("g_num_workgroups", NUM_WORKGROUPS);
        histogramProg->SetUniform("g_num_blocks_per_workgroup", numBlocksPerWorkgroup);

        for (uint32_t i = 0; i < NUM_BYTES; i++)
        {
            histogramProg->Bind();
            histogramProg->SetUniform("g_shift", 8 * i);

            if (i == 0 || i == 2)
            {
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, keyBuffer->GetObj());
            }
            else
            {
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, keyBuffer2->GetObj());
            }
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, histogramBuffer->GetObj());

            glDispatchCompute(NUM_WORKGROUPS, 1, 1);

            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

            sortProg->Bind();
            sortProg->SetUniform("g_shift", 8 * i);

            if ((i % 2) == 0)  // even
            {
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, keyBuffer->GetObj());
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, keyBuffer2->GetObj());
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, valBuffer->GetObj());
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, valBuffer2->GetObj());
            }
            else  // odd
            {
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, keyBuffer2->GetObj());
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, keyBuffer->GetObj());
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, valBuffer2->GetObj());
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, valBuffer->GetObj());
            }
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, histogramBuffer->GetObj());

            glDispatchCompute(NUM_WORKGROUPS, 1, 1);

            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        }

        GL_ERROR_CHECK("SplatRenderer::Sort() sort");

        // indicate if keys are sorted properly or not.
        if (false)
        {
            std::vector<uint32_t> sortedKeyVec(numPoints, 0);
            keyBuffer->Read(sortedKeyVec);

            GL_ERROR_CHECK("SplatRenderer::Sort() READ buffer");

            bool sorted = true;
            for (uint32_t i = 1; i < sortCount; i++)
            {
                if (sortedKeyVec[i - 1] > sortedKeyVec[i])
                {
                    sorted = false;
                    break;
                }
            }
        }
    }
    else
    {
        ZoneScopedNC("sort", tracy::Color::Red4);
        sorter->sort(keyBuffer->GetObj(), valBuffer->GetObj(), sortCount);
        GL_ERROR_CHECK("SplatRenderer::Sort() rgc sort");
    }

    {
        ZoneScopedNC("copy-sorted", tracy::Color::DarkGreen);

        if (useMultiRadixSort && (NUM_BYTES % 2) == 1)  // odd
        {
            glBindBuffer(GL_COPY_READ_BUFFER, valBuffer2->GetObj());
        }
        else
        {
            glBindBuffer(GL_COPY_READ_BUFFER, valBuffer->GetObj());
        }
        glBindBuffer(GL_COPY_WRITE_BUFFER, splatVao->GetElementBuffer()->GetObj());
        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, sortCount * sizeof(uint32_t));

        GL_ERROR_CHECK("SplatRenderer::Sort() copy-sorted");
    }
}


void SplatRenderer::Render(const glm::mat4& cameraMat, const glm::mat4& projMat,
                           const glm::vec4& viewport, const glm::vec2& nearFar)
{
    ZoneScoped;

    GL_ERROR_CHECK("SplatRenderer::Render() begin");

    {
        glViewport((GLint)viewport.x, (GLint)viewport.y,
           (GLint)viewport.z, (GLint)viewport.w);
        
        float width = viewport.z;
        float height = viewport.w;
        float aspectRatio = width / height;
        glm::mat4 viewMat = glm::inverse(cameraMat);
        glm::vec3 eye = glm::vec3(cameraMat[3]);
        float multiplier = (nearFar.x - nearFar.y) * projMat[3][2];

        splatProg->Bind();
        splatProg->SetUniform("viewMat", viewMat);
        splatProg->SetUniform("projMat", projMat);
        splatProg->SetUniform("projParams", glm::vec3(width, height, multiplier));
        splatProg->SetUniform("eye", eye);

        if (renderMode != "AB") {
            uint32_t randomSeed = rand();
            splatProg->SetUniform("u_randomSeed", randomSeed);
        }

        splatVao->Bind();
        
        if (renderMode == "AB") {
            glDrawElements(GL_POINTS, sortCount, GL_UNSIGNED_INT, nullptr);
        }
        else {
            if (taa) {
                EyeTemporalState& currentEyeState = eyeState[activeEye];
                EyeTemporalTextures& currentEyeTextures = eyeTextures[activeEye];
                currentEyeState.frameCount++;
                
                if (currentEyeState.frameCount > 1) {
                currentEyeState.prev_pvmat = currentEyeState.pvmat;
                } else {
                currentEyeState.prev_pvmat = projMat * viewMat;
                }
                
                currentEyeState.pvmat = projMat * viewMat;
                // Use per-eye scene FBO for VR
                currentEyeTextures.sceneFBO->Bind();
                glViewport(0, 0, (GLint)viewport.z, (GLint)viewport.w);
                glEnable(GL_DEPTH_TEST);
                glDepthFunc(GL_LESS);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            }
            glDrawElements(GL_POINTS, (GLsizei)numGaussians, GL_UNSIGNED_INT, nullptr);

        }
        splatVao->Unbind();

        GL_ERROR_CHECK("SplatRenderer::Render() draw");
    }
    if (renderMode != "AB" && taa) {
        Average(viewport);
    }
}

void SplatRenderer::BuildVertexArrayObject(std::shared_ptr<GaussianCloud> gaussianCloud)
{
    splatVao = std::make_shared<VertexArrayObject>();

    // allocate large buffer to hold interleaved vertex data
    gaussianDataBuffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER,
                                                        gaussianCloud->GetRawDataPtr(),
                                                        gaussianCloud->GetTotalSize(), 0);

    const size_t numGaussians = gaussianCloud->GetNumGaussians();

    // build element array
    indexVec.reserve(numGaussians);
    assert(numGaussians <= std::numeric_limits<uint32_t>::max());
    for (uint32_t i = 0; i < (uint32_t)numGaussians; i++)
    {
        indexVec.push_back(i);
    }
    auto indexBuffer = std::make_shared<BufferObject>(GL_ELEMENT_ARRAY_BUFFER, indexVec, GL_DYNAMIC_STORAGE_BIT);

    splatVao->Bind();
    gaussianDataBuffer->Bind();

    const size_t stride = gaussianCloud->GetStride();
    SetupAttrib(splatProg->GetAttribLoc("position"), gaussianCloud->GetPosWithAlphaAttrib(), 4, stride);
    SetupAttrib(splatProg->GetAttribLoc("r_sh0"), gaussianCloud->GetR_SH0Attrib(), 4, stride);
    SetupAttrib(splatProg->GetAttribLoc("g_sh0"), gaussianCloud->GetG_SH0Attrib(), 4, stride);
    SetupAttrib(splatProg->GetAttribLoc("b_sh0"), gaussianCloud->GetB_SH0Attrib(), 4, stride);
    if (gaussianCloud->HasFullSH())
    {
        SetupAttrib(splatProg->GetAttribLoc("r_sh1"), gaussianCloud->GetR_SH1Attrib(), 4, stride);
        SetupAttrib(splatProg->GetAttribLoc("r_sh2"), gaussianCloud->GetR_SH2Attrib(), 4, stride);
        SetupAttrib(splatProg->GetAttribLoc("r_sh3"), gaussianCloud->GetR_SH3Attrib(), 4, stride);
        SetupAttrib(splatProg->GetAttribLoc("g_sh1"), gaussianCloud->GetG_SH1Attrib(), 4, stride);
        SetupAttrib(splatProg->GetAttribLoc("g_sh2"), gaussianCloud->GetG_SH2Attrib(), 4, stride);
        SetupAttrib(splatProg->GetAttribLoc("g_sh3"), gaussianCloud->GetG_SH3Attrib(), 4, stride);
        SetupAttrib(splatProg->GetAttribLoc("b_sh1"), gaussianCloud->GetB_SH1Attrib(), 4, stride);
        SetupAttrib(splatProg->GetAttribLoc("b_sh2"), gaussianCloud->GetB_SH2Attrib(), 4, stride);
        SetupAttrib(splatProg->GetAttribLoc("b_sh3"), gaussianCloud->GetB_SH3Attrib(), 4, stride);
    }
    SetupAttrib(splatProg->GetAttribLoc("cov3_col0"), gaussianCloud->GetCov3_Col0Attrib(), 3, stride);
    SetupAttrib(splatProg->GetAttribLoc("cov3_col1"), gaussianCloud->GetCov3_Col1Attrib(), 3, stride);
    SetupAttrib(splatProg->GetAttribLoc("cov3_col2"), gaussianCloud->GetCov3_Col2Attrib(), 3, stride);

    splatVao->SetElementBuffer(indexBuffer);
    gaussianDataBuffer->Unbind();
}

void SplatRenderer::drawFullscreenQuad()
{
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6); 
    glBindVertexArray(0);
}

void SplatRenderer::bindTex2D(int loc, const std::shared_ptr<Texture>& tex)
{
    glActiveTexture(GL_TEXTURE0 + loc);
    glBindTexture(GL_TEXTURE_2D, tex->GetObj());
}

void SplatRenderer::runWarpPass(
    const std::shared_ptr<Texture>& inAvg,
    const std::shared_ptr<Texture>& inXYZ,
    const std::shared_ptr<Texture>& outAvg,
    const std::shared_ptr<Texture>& outXYZ,
    const glm::mat4& pv)
{
    static const GLfloat ZEROS[4] = {0,0,0,0};
    glClearTexImage(outAvg->GetObj(), 0, GL_RGBA, GL_FLOAT, ZEROS);
    glClearTexImage(outXYZ->GetObj(), 0, GL_RGBA, GL_FLOAT, ZEROS);

    warpProg->Bind();
    bindTex2D(0, inAvg); warpProg->SetUniform("colorTexture", 0);
    bindTex2D(1, inXYZ); warpProg->SetUniform("xyzTexture",   1);
    warpProg->SetUniform("currentViewMatrix", pv);

    glBindImageTexture(0, outAvg->GetObj(), 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
    glBindImageTexture(1, outXYZ->GetObj(), 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

    drawFullscreenQuad();
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
}

void SplatRenderer::runAveragePass(
    const EyeTemporalTextures& T,
    const std::shared_ptr<Texture>& inAvg,
    const std::shared_ptr<Texture>& inXYZ,
    const std::shared_ptr<Texture>& outAvg,
    const std::shared_ptr<Texture>& outXYZ,
    const EyeTemporalState& state,
    bool viewChanged,
    const glm::vec4& viewport)
{     
    avgProg->Bind();
    avgProg->SetUniform("invProjViewMat", glm::inverse(state.pvmat));
    avgProg->SetUniform("viewChanged",    viewChanged);

    bindTex2D(0, T.currentFrameTex); avgProg->SetUniform("currentColorTexture", 0);
    bindTex2D(1, inXYZ);             avgProg->SetUniform("warpedXYZTexture",    1);
    bindTex2D(2, T.depthTex);        avgProg->SetUniform("currentDepthTexture", 2);
    bindTex2D(3, inAvg);             avgProg->SetUniform("warpedColorTexture",  3);

    sumFBO->Bind();
    glViewport(0, 0, (GLint)viewport.z, (GLint)viewport.w);

    sumFBO->AttachColor(outAvg, GL_COLOR_ATTACHMENT0);
    sumFBO->AttachColor(outXYZ, GL_COLOR_ATTACHMENT1);
    const GLenum drawBufs[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, drawBufs);

    drawFullscreenQuad();
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
}

bool matricesNotEqual(const glm::mat4& a, const glm::mat4& b,
    float epsilon = 1e-2f) {
        for (int i = 0; i < 4; ++i) {
            if (!glm::all(glm::lessThan(glm::abs(a[i] - b[i]), glm::vec4(epsilon))))
                return true;
        }
        return false;
}

void SplatRenderer::Average(const glm::vec4& viewport)
{
    ZoneScoped;
    GL_ERROR_CHECK("SplatRenderer::Average() begin");
    {        
        ZoneScopedNC("draw", tracy::Color::Red4);

        glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);

        EyeTemporalState& S = eyeState[activeEye];
        EyeTemporalTextures& T = eyeTextures[activeEye];
    
        // [TODO] this threshold can be adjusted
        bool view_changed = matricesNotEqual(S.pvmat, S.prev_pvmat, 1e-3f);

        std::shared_ptr<Texture> currXYZTex = T.warpXYZTexA;
        std::shared_ptr<Texture> nextXYZTex = T.warpXYZTexB;
        std::shared_ptr<Texture> currAvgTex = T.warpAvgTexA;
        std::shared_ptr<Texture> nextAvgTex = T.warpAvgTexB;
        
        if (S.frameCount <= 1) {
            nextXYZTex = currXYZTex;
            nextAvgTex = currAvgTex;
        } else if (view_changed) {
            runWarpPass(currAvgTex, currXYZTex, nextAvgTex, nextXYZTex, S.pvmat);
        } else {
            std::swap(T.warpXYZTexA, T.warpXYZTexB);
            std::swap(T.warpAvgTexA, T.warpAvgTexB);
        }

        runAveragePass(T, nextAvgTex, nextXYZTex, currAvgTex, currXYZTex, S, view_changed, viewport);

        glBindFramebuffer(GL_FRAMEBUFFER, presentFbo);
        glViewport((GLint)viewport.x, (GLint)viewport.y, (GLint)viewport.z, (GLint)viewport.w);
        glDisable(GL_DEPTH_TEST);
        // display the final quad
        displayProg->Bind();
        bindTex2D(0, currAvgTex);
        displayProg->SetUniform("textureSum", 0);
        drawFullscreenQuad();

        GL_ERROR_CHECK("SplatRenderer::Average()");
    }
}

void SplatRenderer::resetTemporalTextures()
{
    static const GLfloat ZEROS[4] = {0.f, 0.f, 0.f, 0.f};

    EyeTemporalTextures& T      = eyeTextures[activeEye];
    EyeTemporalState&    state  = eyeState[activeEye];
    
    auto clear = [](const std::shared_ptr<Texture>& tex,
                    GLenum fmt = GL_RGBA, GLenum type = GL_FLOAT)
    {
        if (tex)  // tex can be null during first initialisation
            glClearTexImage(tex->GetObj(), 0, fmt, type, ZEROS);
    };

    clear(T.warpAvgTexA); clear(T.warpAvgTexB);
    clear(T.warpXYZTexA); clear(T.warpXYZTexB);

    // start fresh
    state.frameCount   = 0;
    state.prev_pvmat   = state.pvmat;      // keep current for next compare
}

void SplatRenderer::resetTemporalTextures(int newW, int newH)
{
    Texture::Params texParams;
    texParams.magFilter = FilterType::Nearest;
    texParams.minFilter = FilterType::Nearest;
    texParams.sWrap = WrapType::ClampToEdge;
    texParams.tWrap = WrapType::ClampToEdge;

    auto makeRGBA  = [&](int w, int h) {
        return std::make_shared<Texture>(w, h,
                                         GL_RGBA32F,  // IF
                                         GL_RGBA,     // format
                                         GL_FLOAT,    // type
                                         texParams);
    };
    auto makeDepth = [&](int w, int h) {
        return std::make_shared<Texture>(w, h,
                                         GL_DEPTH_COMPONENT32F,
                                         GL_DEPTH_COMPONENT,
                                         GL_FLOAT,
                                         texParams);
    };

    EyeTemporalTextures& T = eyeTextures[activeEye];
    T.warpAvgTexA   = makeRGBA(newW, newH);
    T.warpAvgTexB   = makeRGBA(newW, newH);
    T.warpXYZTexA   = makeRGBA(newW, newH);
    T.warpXYZTexB   = makeRGBA(newW, newH);
    T.currentFrameTex = makeRGBA(newW, newH);
    T.depthTex        = makeDepth(newW, newH);

    // Re‑create / re‑attach FBO
    T.sceneFBO = std::make_shared<FrameBuffer>();
    T.sceneFBO->Bind();
    T.sceneFBO->AttachColor(T.currentFrameTex, GL_COLOR_ATTACHMENT0);
    T.sceneFBO->AttachDepth (T.depthTex);

    if (!T.sceneFBO->IsComplete())
        Log::E("sceneFBO incomplete after resize!");
    
    // Update cached dimensions so Render() uses the correct viewport
    width  = newW;
    height = newH;

    // Reset per‑eye state
    EyeTemporalState& S = eyeState[activeEye];
    S.frameCount = 0;
}
