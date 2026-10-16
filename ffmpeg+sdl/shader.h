#pragma once
#include<string>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

using std::string;
struct ShaderSource {
	std::string VertexSource;
	std::string FragmentSource;
};

class Shader {
	GLuint m_RendererID;
	string VertexFilePath;
public:
	Shader(const string& filepath);//构造函数，传入着色器文件路径
	~Shader();

	void Bind()const;
	void Unbind()const;
	GLuint GetProgram()const { return m_RendererID; }
private:
	ShaderSource ParseShader(const string& filepath);//获取着色器源码
	unsigned int CompileShader(unsigned int type, const std::string source);
	unsigned int CreateShader(const std::string& vertexShader, 
							const std::string& fragmentShader);
};