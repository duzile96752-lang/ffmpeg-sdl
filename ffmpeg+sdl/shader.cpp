#include "shader.h"
#include<iostream>
#include<fstream>
#include<string>
#include<sstream>

//#include"Renderer.h"

Shader::Shader(const std::string& filepath)
{
	//从文件中解析着色器源码
	VertexFilePath = filepath;
	m_RendererID = 0;
	ShaderSource source = ParseShader(filepath);
	m_RendererID = CreateShader(source.VertexSource, source.FragmentSource);
}

Shader::~Shader()
{
	glDeleteProgram(m_RendererID);
}

void Shader::Bind() const
{
	glUseProgram(m_RendererID);
}

void Shader::Unbind() const
{
	glUseProgram(0);
}

ShaderSource Shader::ParseShader(const string & filepath)//解析着色器源码
{
	std::ifstream stream(filepath);
	if (!stream.is_open()) {
		std::cout << "shader文件打开失败: " << filepath << std::endl;
		return { "", "" };
	}
	enum class ShaderType {
		NONE = -1, VERTEX = 0, FRAGMENT = 1
	};

	std::string line;
	std::stringstream ss[2];
	ShaderType type = ShaderType::NONE;
	while (getline(stream, line)) {
		if (line.find("#shader") != string::npos) {
			if(line.find("vertex")!=string::npos){
				type = ShaderType::VERTEX;
			}
			else if (line.find("fragment") != string::npos) {
				type = ShaderType::FRAGMENT;
			}
		}
		else {
			if (type != ShaderType::NONE) {
				ss[(int)type] << line << '\n';
			}
		}
	}
	std::cout << "Vertex source长度: " << ss[0].str().length() << std::endl;
	std::cout << "Fragment source长度: " << ss[1].str().length() << std::endl;

	return {ss[0].str(),ss[1].str()};
}

unsigned int Shader::CompileShader(unsigned int type, const std::string source)
{
	GLuint id;
	id = glCreateShader(type);
	const char* src = source.c_str();
	glShaderSource(id, 1, &src, nullptr);
	glCompileShader(id);

	int result;
	glGetShaderiv(id, GL_COMPILE_STATUS, &result);
	if (result == GL_FALSE) {
		int length;
		glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);

		char* msg = (char*)alloca(length * sizeof(char));
		glGetShaderInfoLog(id, length, &length, msg);
		std::cout << "编译失败：" << msg << std::endl;
		glDeleteShader(id);
		return 0;
	}
	return id;
}

unsigned int Shader::CreateShader(const std::string& vertexShader, const std::string& fragmentShader)
{
	GLuint program = glCreateProgram();
	GLuint vs = CompileShader(GL_VERTEX_SHADER, vertexShader);
	GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fragmentShader);
	//附加着色器
	glAttachShader(program, vs);
	glAttachShader(program, fs);
	glLinkProgram(program);
	//验证程序
	glValidateProgram(program);

	glDeleteShader(vs);
	glDeleteShader(fs);

	return program;
}
