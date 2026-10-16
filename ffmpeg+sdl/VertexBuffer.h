#pragma once
#include"GL/glew.h"
#include"GLFW/glfw3.h"

class VertexBuffer {
private:
	GLuint m_RendererID;
public:
	VertexBuffer(const void* data,GLuint size);
	~VertexBuffer();

	void Bind()const;
	void UnBind()const;
	GLuint GetVbo()const { return m_RendererID; }
};