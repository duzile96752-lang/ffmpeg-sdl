#pragma once
#include"GL/glew.h"
#include"GLFW/glfw3.h"
extern "C" {
#include <libavutil/frame.h>
}
#include"shader.h"
#include"VertexArray.h"
#include"VertexBuffer.h"

class Renderer {
public:
    Renderer();
    ~Renderer();
	bool init(int w, int h,Shader* shader);
	void renderFrame(AVFrame* frame);
    void getData(VertexArray* vao, VertexBuffer* vbo)
    {
        vao_ = vao;
        vbo_ = vbo;
    }
private:
    GLuint program_=0;
    VertexArray* vao_;
    VertexBuffer* vbo_;
    GLuint texY_ = 0, texU_ = 0, texV_ = 0;
    int videoW_ = 0, videoH_ = 0;

    GLuint compileShader(GLenum type, const char* src);
    GLuint createProgram(const char* vs, const char* fs);
    void setupTexParams(GLuint tex);
};