#include "gl_core_4_4.h"

#include <glm/glm.hpp>
#include <GLFW/glfw3.h>
#include <CL/opencl.h>

#include <string>

void APIENTRY debugCallback(GLenum source, GLenum type, GLuint id,
	GLenum severity, GLsizei length, const GLchar * msg, const void * param);

#define CL_CHECK(result) if (result != CL_SUCCESS) { printf("Error: %i\n", result); }

int main(int argc, char* argv[])
{
	// open GL window
	if (!glfwInit())
		exit(EXIT_FAILURE);
	/*
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);*/
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

	// setup
	glClearColor(0, 0, 0, 1);
	glEnable(GL_DEPTH_TEST);
	glDebugMessageCallback(debugCallback, nullptr);
	glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);

	// mc
	size_t			m_gridSize[3];
	cl_float		m_threshold;
	unsigned int	m_maxVertices;
	unsigned int	m_vertexCount;

	// gl objects
//	ShaderProgram	m_shader;
	unsigned int	m_vao, m_vbo;

	// cl objects
	cl_context			m_context;
	cl_command_queue	m_queue;
	cl_program			m_program;
	cl_kernel			m_kernel;

	cl_mem				m_vboLink;
	cl_mem				m_particleBuffer;
	cl_mem				m_vertexCountLink;


	// OpenCL Setup
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
	m_context = clCreateContext(contextProperties, 1, &device, 0, 0, &result);
	CL_CHECK(result);
	m_queue = clCreateCommandQueue(m_context, device, 0, &result);
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
	m_program = clCreateProgramWithSource(m_context, 1, (const char**)&kernelSource, (const size_t*)&size, &result);
	CL_CHECK(result);
	result = clBuildProgram(m_program, 1, &device, 0, 0, 0);
	if (result != CL_SUCCESS)
	{
		size_t len = 0;
		clGetProgramBuildInfo(m_program, device, CL_PROGRAM_BUILD_LOG, 0, 0, &len);
		char* log = new char[len];
		clGetProgramBuildInfo(m_program, device, CL_PROGRAM_BUILD_LOG, len, log, 0);
		OutputDebugStringA("Kernel error:\n");
		OutputDebugStringA(log);
		OutputDebugStringA("\n");
	}
	CL_CHECK(result);
	m_kernel = clCreateKernel(m_program, "kernelMC", &result);
	CL_CHECK(result);


	// MC data
	m_gridSize[0] = 256;
	m_gridSize[1] = 256;
	m_gridSize[2] = 256;

//	m_box = PrimitiveGenerator::createBox(m_gridSize[0], m_gridSize[1], m_gridSize[2]);

	m_threshold = 0.0015f;//50*50;

	m_vertexCount = 0;
	m_maxVertices = 3 * 250000;

	// gl data
	glGenVertexArrays(1, &m_vao);
	glGenBuffers(1, &m_vbo);
	glBindVertexArray(m_vao);
	glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec4) * m_maxVertices * 2, 0, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4) * 2, 0);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_TRUE, sizeof(glm::vec4) * 2, ((char*)0) + 16);
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	// cl mem objects
	m_vboLink = clCreateFromGLBuffer(m_context, CL_MEM_WRITE_ONLY, m_vbo, &result);
	CL_CHECK(result);
	m_vertexCountLink = clCreateBuffer(m_context, CL_MEM_READ_WRITE, sizeof(unsigned int), &m_vertexCount, &result);
	CL_CHECK(result);

	// loop
	while (!glfwWindowShouldClose(window) && 
		   !glfwGetKey(window, GLFW_KEY_ESCAPE)) 
	{
		// march cubes
		
		// ensure GL is complete
		glFinish();

		// reset CL and aquire mem objects
		m_vertexCount = 0;
		cl_event writeEvents[2] = { 0, 0 };

		cl_int result = clEnqueueAcquireGLObjects(m_queue, 1, &m_vboLink, 0, 0, &writeEvents[0]);
		CL_CHECK(result);
		result = clEnqueueWriteBuffer(m_queue, m_vertexCountLink, CL_FALSE, 0, sizeof(unsigned int), &m_vertexCount, 0, nullptr, &writeEvents[1]);
		CL_CHECK(result);

		result = clSetKernelArg(m_kernel, 0, sizeof(cl_mem), &m_vertexCountLink);
		result |= clSetKernelArg(m_kernel, 1, sizeof(cl_mem), &m_vboLink);
		result |= clSetKernelArg(m_kernel, 2, sizeof(cl_float), &m_threshold);
		CL_CHECK(result);

		cl_event processEvent = 0;
		result = clEnqueueNDRangeKernel(m_queue, m_kernel, 3, 0, m_gridSize, 0, 2, writeEvents, &processEvent);
		CL_CHECK(result);

		result = clEnqueueReleaseGLObjects(m_queue, 1, &m_vboLink, 1, &processEvent, 0);
		CL_CHECK(result);

		result = clEnqueueReadBuffer(m_queue, m_vertexCountLink, CL_FALSE, 0, sizeof(unsigned int), &m_vertexCount, 1, &processEvent, 0);
		CL_CHECK(result);

		clFinish(m_queue);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		/*
		// bind shader
	//	m_shader.bind();

		float a_currentTime = 0;

		glm::vec3 target(128, 128, 128);
		glm::vec3 eye(sin(a_currentTime / 1000.f) * 128, 25, cos(a_currentTime / 1000.f) * 128);

		glm::mat4 pvm = glm::perspective(glm::radians(90.0f), 16 / 9.f, 0.1f, 2000.f) * glm::lookAt(target + eye, target, glm::vec3(0, 1, 0));
		glm::mat4 offset = glm::translate(target);

		glUniformMatrix4fv(m_shader.getUniform("pvm"), 1, GL_FALSE, glm::value_ptr(pvm * offset));

	//	m_box->draw();

		glUniformMatrix4fv(m_shader.getUniform("pvm"), 1, GL_FALSE, glm::value_ptr(pvm));

		glBindVertexArray(m_vao);
		glDrawArrays(GL_TRIANGLES, 0, min(m_vertexCount / 2, m_maxVertices));
		*/
		// present
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	clFinish(m_queue);
	clReleaseMemObject(m_vboLink);
	clReleaseMemObject(m_vertexCountLink);
	clReleaseKernel(m_kernel);
	clReleaseProgram(m_program);
	clReleaseCommandQueue(m_queue);
	clReleaseContext(m_context);

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

	printf("ERROR: %s:%s[%s](%d):\n\t%s\n", sourceStr.c_str(), typeStr.c_str(), sevStr.c_str(), id, msg);
}
