#pragma once
#include"shader.h"
#include"VertexBuffer.h"

class VertexArray {
private:
	GLuint m_RendererID;
public:
	VertexArray();
	~VertexArray();

	void Bind()const;
	void Unbind()const;

	void AddBuffer(const VertexBuffer& vb);
	GLuint GetVao()const { return m_RendererID; }
};