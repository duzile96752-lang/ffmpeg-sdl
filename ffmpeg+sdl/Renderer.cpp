#include "Renderer.h"
#include"VertexArray.h"
#include"VertexBuffer.h"
#include"shader.h"
#include<iostream>

Renderer::Renderer()
{
    program_ = 0;
    vao_=nullptr;
    vbo_=nullptr;
    texY_ = 0, texU_ = 0, texV_ = 0;
    videoW_ = 0, videoH_ = 0;
}

Renderer::~Renderer()
{}

bool Renderer::init(int w, int h,Shader* shader)
{
    if (!vao_ || !vbo_) {
        std::cout << "vao_/vbo_ 未设置，请先调用 getData()" << std::endl;
        return false;
    }

    videoW_ = w;
    videoH_ = h;

    program_ = shader->GetProgram();
    if (!program_) {
        std::cout << "program_ 创建失败" << std::endl;
        return false;
    }
    vao_->Bind();
    vao_->AddBuffer(*vbo_);

    vao_->Unbind();
    vbo_->UnBind();

    // 3) 创建三个纹理对象
    glGenTextures(1, &texY_);
    glGenTextures(1, &texU_);
    glGenTextures(1, &texV_);
    setupTexParams(texY_);
    setupTexParams(texU_);
    setupTexParams(texV_);

    int cw = videoW_ / 2, ch = videoH_ / 2;
    glBindTexture(GL_TEXTURE_2D, texY_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, videoW_, videoH_, 0,
        GL_RED, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, texU_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, cw, ch, 0,
        GL_RED, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, texV_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, cw, ch, 0,
        GL_RED, GL_UNSIGNED_BYTE, nullptr);

    // 4) 绑定 sampler 到纹理单元
    glUseProgram(program_);
    glUniform1i(glGetUniformLocation(program_, "texY"), 0);
    glUniform1i(glGetUniformLocation(program_, "texU"), 1);
    glUniform1i(glGetUniformLocation(program_, "texV"), 2);
    glUseProgram(0);

    return true;
}

void Renderer::renderFrame(AVFrame* frame)
{
    glClearColor(1, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(program_);
    //glBindVertexArray(vao_->GetVao());
    vao_->Bind();

    int cw = videoW_ / 2, ch = videoH_ / 2;
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texY_);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, frame->linesize[0]);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, videoW_, videoH_,
        GL_RED, GL_UNSIGNED_BYTE, frame->data[0]);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, texU_);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, frame->linesize[1]);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, cw, ch,
        GL_RED, GL_UNSIGNED_BYTE, frame->data[1]);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, texV_);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, frame->linesize[2]);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, cw, ch,
        GL_RED, GL_UNSIGNED_BYTE, frame->data[2]);

    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

GLuint Renderer::compileShader(GLenum type, const char* src)
{
    return GLuint();
}

GLuint Renderer::createProgram(const char* vs, const char* fs)
{
    return GLuint();
}

void Renderer::setupTexParams(GLuint tex)
{
    glBindTexture(GL_TEXTURE_2D,tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
        GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
        GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
        GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
        GL_CLAMP_TO_EDGE);
}
