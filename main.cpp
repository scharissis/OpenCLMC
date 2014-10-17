
#define _CRT_SECURE_NO_WARNINGS

#include "gl_core_4_4.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <GLFW/glfw3.h>
#include <CL/opencl.h>

#include <string>

void APIENTRY debugCallback(GLenum source, GLenum type, GLuint id,
	GLenum severity, GLsizei length, const GLchar * msg, const void * param);

#define CL_CHECK(result) if (result != CL_SUCCESS) { printf("Error: %i\n", result); }
#define STRINGIFY(str) #str

struct GLData
{
	GLuint	program;
	GLuint	vao;
	GLuint	vbo;
};

struct MCData
{
	size_t			gridSize[3];
	cl_float		threshold;
	unsigned int	maxFaces;
	unsigned int	faceCount;
};

struct CLData
{
	cl_context			context;
	cl_command_queue	queue;
	cl_program			program;
	cl_kernel			kernel;

	cl_mem				vboLink;
	cl_mem				faceCountLink;
};

int main(int argc, char* argv[])
{
	MCData mcData = { { 256, 256, 256 }, 0.0015f, 250000, 0 };
	GLData glData = { 0 };
	CLData clData;

	//////////////////////////////////////////////////////////////////////////
	// Window creation and OpenGL initialisaion
	if (!glfwInit())
		exit(EXIT_FAILURE);
	
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);

	GLFWwindow* window = glfwCreateWindow(1280, 720, "OpenCL Marching Cubes", nullptr, nullptr);
	if (window == nullptr)
	{
		glfwTerminate();
		exit(EXIT_FAILURE);
	}
	glfwMakeContextCurrent(window);

	// update GL function pointers
	if (ogl_LoadFunctions() == ogl_LOAD_FAILED)
	{
		glfwTerminate();
		exit(EXIT_FAILURE);
	}

	//////////////////////////////////////////////////////////////////////////
	// OpenGL setup
	glClearColor(0, 0, 0, 1);
	glEnable(GL_DEPTH_TEST);
	glDebugMessageCallback(debugCallback, nullptr);
	glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
	
	// shader
	char* vsSource = STRINGIFY(#version 330\n layout(location = 0) in vec4 Position; layout(location = 1)in vec4 Normal; out vec4 N; uniform mat4 pvm; void main() { gl_Position = pvm * Position; N = Normal; });
	char* fsSource = STRINGIFY(#version 330\n in vec4 N; out vec4 Colour; void main() { float d = dot(normalize(N.xyz), normalize(vec3(0, 1, 1))) * 0.8; Colour = vec4(mix(vec3(0.25),vec3(1),d),1); });

	GLuint vs = glCreateShader(GL_VERTEX_SHADER);
	GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);

	glShaderSource(vs, 1, &vsSource, 0);
	glShaderSource(fs, 1, &fsSource, 0);

	glCompileShader(vs);
	glCompileShader(fs);

	glData.program = glCreateProgram();
	glAttachShader(glData.program, vs);
	glAttachShader(glData.program, fs);
	glLinkProgram(glData.program);
	glUseProgram(glData.program);
	glDeleteShader(vs);
	glDeleteShader(fs);

	GLint pvmUniform = glGetUniformLocation(glData.program, "pvm");

	// mesh data
	glGenVertexArrays(1, &glData.vao);
	glBindVertexArray(glData.vao);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	
	glGenBuffers(1, &glData.vbo);
	glBindBuffer(GL_ARRAY_BUFFER, glData.vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec4) * 2 * mcData.maxFaces * 3, 0, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4) * 2, 0);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_TRUE, sizeof(glm::vec4) * 2, ((char*)0) + 16);
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	//////////////////////////////////////////////////////////////////////////
	// OpenCL stup
	cl_platform_id platform;
	cl_int result = clGetPlatformIDs(1, &platform, 0);
	CL_CHECK(result);
	cl_device_id device;
	result = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, 0);
	CL_CHECK(result);

	// request GL/CL context
	cl_context_properties contextProperties[] = {
		CL_GL_CONTEXT_KHR, (cl_context_properties)wglGetCurrentContext(),
		CL_WGL_HDC_KHR, (cl_context_properties)wglGetCurrentDC(),
		CL_CONTEXT_PLATFORM, (cl_context_properties)platform,
		0, 0,
	};
	clData.context = clCreateContext(contextProperties, 1, &device, 0, 0, &result);
	CL_CHECK(result);
	clData.queue = clCreateCommandQueue(clData.context, device, 0, &result);
	CL_CHECK(result);

	// load kernel code
	FILE* file = fopen("mc.cl", "rb");
	fseek(file, 0, SEEK_END);
	unsigned int size = ftell(file);
	fseek(file, 0, SEEK_SET);
	char* kernelSource = new char[size];
	fread(kernelSource, sizeof(char), size, file);
	fclose(file);

	// build program and extract kernel
	clData.program = clCreateProgramWithSource(clData.context, 1, (const char**)&kernelSource, (const size_t*)&size, &result);
	delete[] kernelSource;
	CL_CHECK(result);
	result = clBuildProgram(clData.program, 1, &device, 0, 0, 0);
	if (result != CL_SUCCESS)
	{
		size_t len = 0;
		clGetProgramBuildInfo(clData.program, device, CL_PROGRAM_BUILD_LOG, 0, 0, &len);
		char* log = new char[len];
		clGetProgramBuildInfo(clData.program, device, CL_PROGRAM_BUILD_LOG, len, log, 0);
		OutputDebugStringA("Kernel error:\n");
		OutputDebugStringA(log);
		OutputDebugStringA("\n");
	}
	CL_CHECK(result);
	clData.kernel = clCreateKernel(clData.program, "kernelMC", &result);
	CL_CHECK(result);

	// cl mem objects
	clData.vboLink = clCreateFromGLBuffer(clData.context, CL_MEM_WRITE_ONLY, glData.vbo, &result);
	CL_CHECK(result);
	clData.faceCountLink = clCreateBuffer(clData.context, CL_MEM_READ_WRITE, sizeof(unsigned int), &mcData.faceCount, &result);
	CL_CHECK(result);

	// loop
	while (!glfwWindowShouldClose(window) && 
		   !glfwGetKey(window, GLFW_KEY_ESCAPE)) 
	{
		// ensure GL is complete
		glFinish();

		// reset CL and aquire mem objects
		mcData.faceCount = 0;
		cl_event writeEvents[2] = { 0, 0 };

		cl_int result = clEnqueueAcquireGLObjects(clData.queue, 1, &clData.vboLink, 0, 0, &writeEvents[0]);
		CL_CHECK(result);
		result = clEnqueueWriteBuffer(clData.queue, clData.faceCountLink, CL_FALSE, 0, sizeof(unsigned int), &mcData.faceCount, 0, nullptr, &writeEvents[1]);
		CL_CHECK(result);

		result = clSetKernelArg(clData.kernel, 0, sizeof(cl_mem), &clData.faceCountLink);
		result |= clSetKernelArg(clData.kernel, 1, sizeof(cl_mem), &clData.vboLink);
		result |= clSetKernelArg(clData.kernel, 2, sizeof(cl_float), &mcData.threshold);
		CL_CHECK(result);

		cl_event processEvent = 0;
		result = clEnqueueNDRangeKernel(clData.queue, clData.kernel, 3, 0, mcData.gridSize, 0, 2, writeEvents, &processEvent);
		CL_CHECK(result);

		result = clEnqueueReleaseGLObjects(clData.queue, 1, &clData.vboLink, 1, &processEvent, 0);
		CL_CHECK(result);

		result = clEnqueueReadBuffer(clData.queue, clData.faceCountLink, CL_FALSE, 0, sizeof(unsigned int), &mcData.faceCount, 1, &processEvent, 0);
		CL_CHECK(result);

		clFinish(clData.queue);

		// draw blob
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		float time = (float)glfwGetTime();

		glm::vec3 target(128, 128, 128);
		glm::vec3 eye(sin(time) * 32, 25, cos(time) * 32);
		glm::mat4 pvm = glm::perspective(glm::radians(90.0f), 16 / 9.f, 0.1f, 2000.f) * glm::lookAt(target + eye, target, glm::vec3(0, 1, 0));

		glUniformMatrix4fv(pvmUniform, 1, GL_FALSE, glm::value_ptr(pvm));

		glBindVertexArray(glData.vao);
		glDrawArrays(GL_TRIANGLES, 0, glm::min(mcData.faceCount, mcData.maxFaces) * 3);
		
		// present
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	// cleanup cl
	clFinish(clData.queue);
	clReleaseMemObject(clData.vboLink);
	clReleaseMemObject(clData.faceCountLink);
	clReleaseKernel(clData.kernel);
	clReleaseProgram(clData.program);
	clReleaseCommandQueue(clData.queue);
	clReleaseContext(clData.context);

	// cleanup gl
	glDeleteBuffers(1, &glData.vbo);
	glDeleteVertexArrays(1, &glData.vao);
	glDeleteProgram(glData.program);
	glfwTerminate();

	exit(EXIT_SUCCESS);
}

void APIENTRY debugCallback(GLenum source, GLenum type, GLuint id, 
							GLenum severity, GLsizei length, const GLchar * msg, const void * param)
{

	std::string sourceStr;
	switch (source) 
	{
	case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
		sourceStr = "WindowSys";
		break;
	case GL_DEBUG_SOURCE_APPLICATION:
		sourceStr = "App";
		break;
	case GL_DEBUG_SOURCE_API:
		sourceStr = "OpenGL";
		break;
	case GL_DEBUG_SOURCE_SHADER_COMPILER:
		sourceStr = "ShaderCompiler";
		break;
	case GL_DEBUG_SOURCE_THIRD_PARTY:
		sourceStr = "3rdParty";
		break;
	case GL_DEBUG_SOURCE_OTHER:
		sourceStr = "Other";
		break;
	default:
		sourceStr = "Unknown";
	}

	std::string typeStr;
	switch (type) 
	{
	case GL_DEBUG_TYPE_ERROR:
		typeStr = "Error";
		break;
	case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
		typeStr = "Deprecated";
		break;
	case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
		typeStr = "Undefined";
		break;
	case GL_DEBUG_TYPE_PORTABILITY:
		typeStr = "Portability";
		break;
	case GL_DEBUG_TYPE_PERFORMANCE:
		typeStr = "Performance";
		break;
	case GL_DEBUG_TYPE_MARKER:
		typeStr = "Marker";
		break;
	case GL_DEBUG_TYPE_PUSH_GROUP:
		typeStr = "PushGrp";
		break;
	case GL_DEBUG_TYPE_POP_GROUP:
		typeStr = "PopGrp";
		break;
	case GL_DEBUG_TYPE_OTHER:
		typeStr = "Other";
		break;
	default:
		typeStr = "Unknown";
	}

	std::string sevStr;
	switch (severity)
	{
	case GL_DEBUG_SEVERITY_HIGH:
		sevStr = "HIGH";
		break;
	case GL_DEBUG_SEVERITY_MEDIUM:
		sevStr = "MED";
		break;
	case GL_DEBUG_SEVERITY_LOW:
		sevStr = "LOW";
		break;
	case GL_DEBUG_SEVERITY_NOTIFICATION:
		sevStr = "NOTIFY";
		break;
	default:
		sevStr = "UNK";
	}

	printf("%s: %s [%s] (%d):\n\t%s\n", typeStr.c_str(), sourceStr.c_str(), sevStr.c_str(), id, msg);
}
