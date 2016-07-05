/*******************************************************
 * Copyright (c) 2015-2019, ArrayFire
 * All rights reserved.
 *
 * This file is distributed under 3-clause BSD license.
 * The complete license agreement can be obtained at:
 * http://arrayfire.com/licenses/BSD-3-Clause
 ********************************************************/

#include <err_opengl.hpp>
#include <plot_impl.hpp>
#include <shader_headers/marker2d_vs.hpp>
#include <shader_headers/marker_fs.hpp>
#include <shader_headers/histogram_fs.hpp>
#include <shader_headers/plot3_vs.hpp>
#include <shader_headers/plot3_fs.hpp>

#include <cmath>

using namespace gl;
using namespace std;

namespace opengl
{

void plot_impl::bindResources(const int pWindowId)
{
    if (mVAOMap.find(pWindowId) == mVAOMap.end()) {
        GLuint vao = 0;
        /* create a vertex array object
         * with appropriate bindings */
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        // attach vertices
        glEnableVertexAttribArray(mPlotPointIndex);
        glBindBuffer(GL_ARRAY_BUFFER, mVBO);
        glVertexAttribPointer(mPlotPointIndex, mDimension, mGLType, GL_FALSE, 0, 0);
        // attach colors
        glEnableVertexAttribArray(mPlotColorIndex);
        glBindBuffer(GL_ARRAY_BUFFER, mCBO);
        glVertexAttribPointer(mPlotColorIndex, 3, GL_FLOAT, GL_FALSE, 0, 0);
        // attach alphas
        glEnableVertexAttribArray(mPlotAlphaIndex);
        glBindBuffer(GL_ARRAY_BUFFER, mABO);
        glVertexAttribPointer(mPlotAlphaIndex, 1, GL_FLOAT, GL_FALSE, 0, 0);
        // attach radii
        glEnableVertexAttribArray(mMarkerRadiiIndex);
        glBindBuffer(GL_ARRAY_BUFFER, mRBO);
        glVertexAttribPointer(mMarkerRadiiIndex, 1, GL_FLOAT, GL_FALSE, 0, 0);
        glBindVertexArray(0);
        /* store the vertex array object corresponding to
         * the window instance in the map */
        mVAOMap[pWindowId] = vao;
    }

    glBindVertexArray(mVAOMap[pWindowId]);
}

void plot_impl::unbindResources() const
{
    glBindVertexArray(0);
}

glm::mat4 plot_impl::computeTransformMat(const glm::mat4 pView)
{
    static const glm::mat4 MODEL = glm::rotate(glm::mat4(1.0f), -glm::radians(90.f), glm::vec3(0,1,0)) *
                                   glm::rotate(glm::mat4(1.0f), -glm::radians(90.f), glm::vec3(1,0,0));

    float xRange = mRange[1] - mRange[0];
    float yRange = mRange[3] - mRange[2];
    float zRange = mRange[5] - mRange[4];

    float xDataScale = std::abs(xRange) < 1.0e-3 ? 0.0f : 2/(xRange);
    float yDataScale = std::abs(yRange) < 1.0e-3 ? 0.0f : 2/(yRange);
    float zDataScale = std::abs(zRange) < 1.0e-3 ? 0.0f : 2/(zRange);

    float xDataOffset = (-mRange[0] * xDataScale);
    float yDataOffset = (-mRange[2] * yDataScale);
    float zDataOffset = (-mRange[4] * zDataScale);

    glm::vec3 scaleVector(xDataScale, -1.0f * yDataScale, zDataScale);

    glm::vec3 shiftVector(-(mRange[0]+mRange[1])/2.0f,
                          -(mRange[2]+mRange[3])/2.0f,
                          -(mRange[4]+mRange[5])/2.0f);
    shiftVector += glm::vec3(-1 + xDataOffset, -1 + yDataOffset, -1 + zDataOffset);

    return pView * glm::translate(glm::scale(MODEL, scaleVector), shiftVector);
}

void plot_impl::bindDimSpecificUniforms()
{
    glUniform2fv(mPlotRangeIndex, 3, mRange);
}

plot_impl::plot_impl(const uint pNumPoints, const fg::dtype pDataType,
                     const fg::PlotType pPlotType, const fg::MarkerType pMarkerType, const int pD)
    : mDimension(pD), mMarkerSize(12), mNumPoints(pNumPoints), mDataType(pDataType),
    mGLType(dtype2gl(mDataType)), mMarkerType(pMarkerType), mPlotType(pPlotType), mIsPVROn(false),
    mPlotProgram(-1), mMarkerProgram(-1), mRBO(-1), mPlotMatIndex(-1), mPlotPVCOnIndex(-1),
    mPlotPVAOnIndex(-1), mPlotUColorIndex(-1), mPlotRangeIndex(-1), mPlotPointIndex(-1),
    mPlotColorIndex(-1), mPlotAlphaIndex(-1), mMarkerPVCOnIndex(-1), mMarkerPVAOnIndex(-1),
    mMarkerTypeIndex(-1), mMarkerColIndex(-1), mMarkerMatIndex(-1), mMarkerPointIndex(-1),
    mMarkerColorIndex(-1), mMarkerAlphaIndex(-1), mMarkerRadiiIndex(-1)
{
    CheckGL("Begin plot_impl::plot_impl");
    mIsPVCOn = false;
    mIsPVAOn = false;

    setColor(0, 1, 0, 1);
    mLegend  = std::string("");

    if (mDimension==2) {
        mPlotProgram     = initShaders(glsl::marker2d_vs.c_str(), glsl::histogram_fs.c_str());
        mMarkerProgram   = initShaders(glsl::marker2d_vs.c_str(), glsl::marker_fs.c_str());
        mPlotUColorIndex = glGetUniformLocation(mPlotProgram, "barColor");
        mVBOSize = 2*mNumPoints;
    } else {
        mPlotProgram     = initShaders(glsl::plot3_vs.c_str(), glsl::plot3_fs.c_str());
        mMarkerProgram   = initShaders(glsl::plot3_vs.c_str(), glsl::marker_fs.c_str());
        mPlotRangeIndex  = glGetUniformLocation(mPlotProgram, "minmaxs");
        mVBOSize = 3*mNumPoints;
    }

    mCBOSize = 3*mNumPoints;
    mABOSize = mNumPoints;
    mRBOSize = mNumPoints;

    mPlotMatIndex    = glGetUniformLocation(mPlotProgram, "transform");
    mPlotPVCOnIndex  = glGetUniformLocation(mPlotProgram, "isPVCOn");
    mPlotPVAOnIndex  = glGetUniformLocation(mPlotProgram, "isPVAOn");
    mPlotPointIndex  = glGetAttribLocation (mPlotProgram, "point");
    mPlotColorIndex  = glGetAttribLocation (mPlotProgram, "color");
    mPlotAlphaIndex  = glGetAttribLocation (mPlotProgram, "alpha");

    mMarkerMatIndex   = glGetUniformLocation(mMarkerProgram, "transform");
    mMarkerPVCOnIndex = glGetUniformLocation(mMarkerProgram, "isPVCOn");
    mMarkerPVAOnIndex = glGetUniformLocation(mMarkerProgram, "isPVAOn");
    mMarkerPVROnIndex = glGetUniformLocation(mMarkerProgram, "isPVROn");
    mMarkerTypeIndex  = glGetUniformLocation(mMarkerProgram, "marker_type");
    mMarkerColIndex   = glGetUniformLocation(mMarkerProgram, "marker_color");
    mMarkerPSizeIndex = glGetUniformLocation(mMarkerProgram, "psize");
    mMarkerPointIndex = glGetAttribLocation (mMarkerProgram, "point");
    mMarkerColorIndex = glGetAttribLocation (mMarkerProgram, "color");
    mMarkerAlphaIndex = glGetAttribLocation (mMarkerProgram, "alpha");
    mMarkerRadiiIndex = glGetAttribLocation (mMarkerProgram, "pointsize");

#define PLOT_CREATE_BUFFERS(type)   \
        mVBO = createBuffer<type>(GL_ARRAY_BUFFER, mVBOSize, NULL, GL_DYNAMIC_DRAW);    \
        mCBO = createBuffer<float>(GL_ARRAY_BUFFER, mCBOSize, NULL, GL_DYNAMIC_DRAW);   \
        mABO = createBuffer<float>(GL_ARRAY_BUFFER, mABOSize, NULL, GL_DYNAMIC_DRAW);   \
        mRBO = createBuffer<float>(GL_ARRAY_BUFFER, mRBOSize, NULL, GL_DYNAMIC_DRAW);   \
        mVBOSize *= sizeof(type);   \
        mCBOSize *= sizeof(float);  \
        mABOSize *= sizeof(float);  \
        mRBOSize *= sizeof(float);

        switch(mGLType) {
            case GL_FLOAT          : PLOT_CREATE_BUFFERS(float) ; break;
            case GL_INT            : PLOT_CREATE_BUFFERS(int)   ; break;
            case GL_UNSIGNED_INT   : PLOT_CREATE_BUFFERS(uint)  ; break;
            case GL_SHORT          : PLOT_CREATE_BUFFERS(short) ; break;
            case GL_UNSIGNED_SHORT : PLOT_CREATE_BUFFERS(ushort); break;
            case GL_UNSIGNED_BYTE  : PLOT_CREATE_BUFFERS(float) ; break;
            default: fg::TypeError("plot_impl::plot_impl", __LINE__, 1, mDataType);
        }
#undef PLOT_CREATE_BUFFERS
        CheckGL("End plot_impl::plot_impl");
}

plot_impl::~plot_impl()
{
    CheckGL("Begin plot_impl::~plot_impl");
    for (auto it = mVAOMap.begin(); it!=mVAOMap.end(); ++it) {
        GLuint vao = it->second;
        glDeleteVertexArrays(1, &vao);
    }
    glDeleteBuffers(1, &mVBO);
    glDeleteBuffers(1, &mCBO);
    glDeleteBuffers(1, &mABO);
    glDeleteProgram(mPlotProgram);
    glDeleteProgram(mMarkerProgram);
    CheckGL("End plot_impl::~plot_impl");
}

void plot_impl::setMarkerSize(const float pMarkerSize)
{
    mMarkerSize = pMarkerSize;
}

GLuint plot_impl::markers()
{
    mIsPVROn = true;
    return mRBO;
}

size_t plot_impl::markersSizes() const
{
    return mRBOSize;
}

void plot_impl::render(const int pWindowId,
                       const int pX, const int pY, const int pVPW, const int pVPH,
                       const glm::mat4& pView)
{
    CheckGL("Begin plot_impl::render");
    if (mIsPVAOn) {
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    glm::mat4 viewModelMatrix = this->computeTransformMat(pView);

    if (mPlotType == FG_PLOT_LINE) {
        glUseProgram(mPlotProgram);

        this->bindDimSpecificUniforms();
        glUniformMatrix4fv(mPlotMatIndex, 1, GL_FALSE, glm::value_ptr(viewModelMatrix));
        glUniform1i(mPlotPVCOnIndex, mIsPVCOn);
        glUniform1i(mPlotPVAOnIndex, mIsPVAOn);

        plot_impl::bindResources(pWindowId);
        glDrawArrays(GL_LINE_STRIP, 0, mNumPoints);
        plot_impl::unbindResources();

        glUseProgram(0);
    }

    if (mMarkerType != FG_MARKER_NONE) {
        glEnable(GL_PROGRAM_POINT_SIZE);
        glUseProgram(mMarkerProgram);

        glUniformMatrix4fv(mMarkerMatIndex, 1, GL_FALSE, glm::value_ptr(viewModelMatrix));
        glUniform1i(mMarkerPVCOnIndex, mIsPVCOn);
        glUniform1i(mMarkerPVAOnIndex, mIsPVAOn);
        glUniform1i(mMarkerPVROnIndex, mIsPVROn);
        glUniform1i(mMarkerTypeIndex, mMarkerType);
        glUniform4fv(mMarkerColIndex, 1, mColor);
        glUniform1f(mMarkerPSizeIndex, mMarkerSize);

        plot_impl::bindResources(pWindowId);
        glDrawArrays(GL_POINTS, 0, mNumPoints);
        plot_impl::unbindResources();

        glUseProgram(0);
        glDisable(GL_PROGRAM_POINT_SIZE);
    }

    if (mIsPVAOn) {
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
    }
    CheckGL("End plot_impl::render");
}

glm::mat4 plot2d_impl::computeTransformMat(const glm::mat4 pView)
{
    float xRange = mRange[1] - mRange[0];
    float yRange = mRange[3] - mRange[2];

    float xDataScale = std::abs(xRange) < 1.0e-3 ? 1.0f : 2/(xRange);
    float yDataScale = std::abs(yRange) < 1.0e-3 ? 1.0f : 2/(yRange);

    glm::vec3 shiftVector(-(mRange[0]+mRange[1])/2.0f, -(mRange[2]+mRange[3])/2.0f, 0.0f);
    glm::vec3 scaleVector(xDataScale, yDataScale, 1);

    return pView * glm::translate(glm::scale(IDENTITY, scaleVector), shiftVector);
}

void plot2d_impl::bindDimSpecificUniforms()
{
    glUniform4fv(mPlotUColorIndex, 1, mColor);
}

}
