#include "gl_core_4_4.h"
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <GLFW/glfw3.h>

#ifdef __APPLE__
	#include <OpenCL/cl_gl_ext.h>
	#include <OpenCL/cl.h>
	#include <OpenGL/OpenGL.h>
#else
	#include <CL/cl.h>
	#include <CL/cl_gl_ext.h>
	#include <GL/GL.h>
	#include <windows.h>
#endif

#define STRINGIFY(str) #str
#define CL_CHECK(result) if (result != CL_SUCCESS) { printf("Error: %i\n", result); }

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
	cl_uint	faceCount;
};

struct CLData
{
	cl_context			context;
	cl_command_queue	queue;
	cl_program			program;
	cl_kernel			kernel;

	cl_mem				vboLink;
	cl_mem				faceCountLink;
	cl_mem				particleLink;
};

int main(int argc, char* argv[])
{
	MCData mcData = { { 64, 64, 64 }, 0.04f, 250000, 0 };
	GLData glData = { 0 };
	CLData clData;
	const int particleCount = 8;
	glm::vec4 particles[particleCount];

	// window creation and OpenGL initialisaion
	if (!glfwInit())
		exit(EXIT_FAILURE);
	
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);

	GLFWwindow* window = glfwCreateWindow(1280, 720, "Test", nullptr, nullptr);
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

	glClearColor(0, 0, 0, 1);
	glEnable(GL_DEPTH_TEST);

	// shader
	char* vsSource = STRINGIFY(#version 410\n 
		layout(location = 0) in vec4 Position; 
		layout(location = 1) in vec4 Normal; 
		out vec4 N; 
		uniform mat4 pvm; 
		void main() { 
			gl_Position = pvm * Position; 
			N = Normal; 
		});
	char* fsSource = STRINGIFY(#version 410\n 
		in vec4 N; 
		out vec4 Colour;  	
		void main() { 
			float d = dot(normalize(N.xyz), normalize(vec3(1))); 
			Colour = vec4(mix(vec3(0,0,0.75),vec3(0,0.75,1),d), 1); 
		});

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
	glGenBuffers(1, &glData.vbo);
	glBindBuffer(GL_ARRAY_BUFFER, glData.vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec4) * 2 * mcData.maxFaces * 3, 0, GL_STATIC_DRAW);

	glGenVertexArrays(1, &glData.vao);
	glBindVertexArray(glData.vao);
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4) * 2, 0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_TRUE, sizeof(glm::vec4) * 2, ((char*)0) + sizeof(glm::vec4));
    glBindVertexArray(0);

	glBindVertexArray(0);

	// hand-coded crappy box around the fluid
	glm::vec4 lines[] = {
		glm::vec4(0, 0, 0, 1),															glm::vec4(1),
		glm::vec4(mcData.gridSize[0], 0, 0, 1),											glm::vec4(1),
		glm::vec4(0, 0, mcData.gridSize[2], 1),											glm::vec4(1),
		glm::vec4(mcData.gridSize[0], 0, mcData.gridSize[2], 1),						glm::vec4(1),
		glm::vec4(0, mcData.gridSize[1], 0, 1),											glm::vec4(1),
		glm::vec4(mcData.gridSize[0], mcData.gridSize[1], 0, 1),						glm::vec4(1),
		glm::vec4(0, mcData.gridSize[1], mcData.gridSize[2], 1),						glm::vec4(1),
		glm::vec4(mcData.gridSize[0], mcData.gridSize[1], mcData.gridSize[2], 1),		glm::vec4(1),
		glm::vec4(0, 0, 0, 1),															glm::vec4(1),
		glm::vec4(0, mcData.gridSize[1], 0, 1),											glm::vec4(1),
		glm::vec4(mcData.gridSize[0], 0, 0, 1),											glm::vec4(1),
		glm::vec4(mcData.gridSize[0], mcData.gridSize[1], 0, 1),						glm::vec4(1),
		glm::vec4(0, 0, mcData.gridSize[2], 1),											glm::vec4(1),
		glm::vec4(0, mcData.gridSize[1], mcData.gridSize[2], 1),						glm::vec4(1),
		glm::vec4(mcData.gridSize[0], 0, mcData.gridSize[2], 1),						glm::vec4(1),
		glm::vec4(mcData.gridSize[0], mcData.gridSize[1], mcData.gridSize[2], 1),		glm::vec4(1),
		glm::vec4(0, 0, 0, 1),															glm::vec4(1),
		glm::vec4(0, 0, mcData.gridSize[2], 1),											glm::vec4(1),
		glm::vec4(0, mcData.gridSize[1], 0, 1),											glm::vec4(1),
		glm::vec4(0, mcData.gridSize[1], mcData.gridSize[2], 1),						glm::vec4(1),
		glm::vec4(mcData.gridSize[0], 0, 0, 1),											glm::vec4(1),
		glm::vec4(mcData.gridSize[0], 0, mcData.gridSize[2], 1),						glm::vec4(1),
		glm::vec4(mcData.gridSize[0], mcData.gridSize[1], 0, 1),						glm::vec4(1),
		glm::vec4(mcData.gridSize[0], mcData.gridSize[1], mcData.gridSize[2], 1),		glm::vec4(1)
	};

	GLuint boxVBO, boxVAO;
	glGenBuffers(1, &boxVBO);
	glBindBuffer(GL_ARRAY_BUFFER, boxVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec4) * 48, lines, GL_STATIC_DRAW);

	glGenVertexArrays(1, &boxVAO);
	glBindVertexArray(boxVAO);
	glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4) * 2, 0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_TRUE, sizeof(glm::vec4) * 2, ((char*)0) + sizeof(glm::vec4));
	glBindVertexArray(0);
    
    // opencl setup
    cl_uint numPlatforms = 0;
    cl_int result = clGetPlatformIDs(0, nullptr, &numPlatforms);
    printf("Platforms: %i\n", numPlatforms);
	CL_CHECK(result);

	// I'm only going to care about the first platform
	cl_platform_id platform;
	result = clGetPlatformIDs(1, &platform, 0);
	CL_CHECK(result);
    
    cl_uint numDevices = 0;
	clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 0, nullptr, &numDevices);
    printf("GPU Devices: %i\n", numDevices);
    
    cl_device_id* devices = new cl_device_id[numDevices];
	result = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, numDevices, devices, 0);
    CL_CHECK(result);
    
    // find a device that supports GL interop
    cl_uint glDevice = 0;
    bool glDeviceFound = false;
    for (cl_uint i = 0 ; i < numDevices ; ++i)
    {
        size_t extensionSize = 0;
        result = clGetDeviceInfo(devices[i], CL_DEVICE_EXTENSIONS, 0, nullptr, &extensionSize);
        CL_CHECK(result);
        
        if (result == CL_SUCCESS)
        {
            char* extensions = new char[extensionSize];
            result = clGetDeviceInfo(devices[i], CL_DEVICE_EXTENSIONS, extensionSize, extensions, &extensionSize);
            CL_CHECK(result);

            std::string devString(extensions);
            delete[] extensions;
            
            size_t oldPos = 0;
            size_t spacePos = devString.find(' ', oldPos);
            while (spacePos != devString.npos)
            {
                if (strcmp("cl_khr_gl_sharing", devString.substr(oldPos, spacePos - oldPos).c_str()) == 0 ||
                	strcmp("cl_APPLE_gl_sharing", devString.substr(oldPos, spacePos - oldPos).c_str()) == 0)
                {
                    glDevice = i;
                    glDeviceFound = true;
                    break;
                }
                do {
                    oldPos = spacePos + 1;
                    spacePos = devString.find(' ', oldPos);
                } while (spacePos == oldPos);
            }
        }
    }
    
    if (glDeviceFound == false)
    {
        printf("Failed to find CL-GL shared device!\n");
        exit(EXIT_FAILURE);
    }
    printf("Found CL-GL shared device: id [ %i ]\n", glDevice);
    
#if defined(__APPLE__) || defined(MACOSX)
    // Get current CGL Context and CGL Share group
    CGLContextObj kCGLContext = CGLGetCurrentContext();
    CGLShareGroupObj kCGLShareGroup = CGLGetShareGroup(kCGLContext);
    
    // Create CL context properties, add handle & share-group enum
    cl_context_properties contextProperties[] = {
        CL_CONTEXT_PROPERTY_USE_CGL_SHAREGROUP_APPLE,
        (cl_context_properties)kCGLShareGroup, 0
    };
#else
    // request GL/CL context
    cl_context_properties contextProperties[] = {
        CL_GL_CONTEXT_KHR, (cl_context_properties)wglGetCurrentContext(),
        CL_WGL_HDC_KHR, (cl_context_properties)wglGetCurrentDC(),
        CL_CONTEXT_PLATFORM, (cl_context_properties)platform,
        0, 0,
    };
#endif
    
    clData.context = clCreateContext(contextProperties, numDevices, devices, 0, 0, &result);
    CL_CHECK(result);

    clData.queue = clCreateCommandQueue(clData.context, devices[glDevice], 0, &result);
    CL_CHECK(result);

	// load kernel code
	FILE* file = fopen("./mc.cl", "rb");
	if (file == nullptr)
	{
		printf("Failed to load kernel file mc.cl!\n");
		exit(EXIT_FAILURE);
	}
	fseek(file, 0, SEEK_END);
	size_t size = ftell(file);
	fseek(file, 0, SEEK_SET);
	char* kernelSource = new char[size];
	fread(kernelSource, sizeof(char), size, file);
	fclose(file);

	// build program and extract kernel
	clData.program = clCreateProgramWithSource(clData.context, 1, (const char**)&kernelSource, &size, &result);
	delete[] kernelSource;
	CL_CHECK(result);
	result = clBuildProgram(clData.program, 1, &devices[glDevice], 0, 0, 0);
	if (result != CL_SUCCESS)
	{
		size_t len = 0;
		clGetProgramBuildInfo(clData.program, devices[glDevice], CL_PROGRAM_BUILD_LOG, 0, 0, &len);
		char* log = new char[len];
		clGetProgramBuildInfo(clData.program, devices[glDevice], CL_PROGRAM_BUILD_LOG, len, log, 0);
		printf("Kernel error:\n%s\n", log);
		delete[] log;

		clReleaseProgram(clData.program);
		clReleaseCommandQueue(clData.queue);
		clReleaseContext(clData.context);

		exit(EXIT_FAILURE);
	}
	CL_CHECK(result);
	clData.kernel = clCreateKernel(clData.program, "kernelMC", &result);
	CL_CHECK(result);

	// cl mem objects
	clData.vboLink = clCreateFromGLBuffer(clData.context, CL_MEM_WRITE_ONLY, glData.vbo, &result);
	CL_CHECK(result);
	clData.faceCountLink = clCreateBuffer(clData.context, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR, sizeof(cl_uint), &mcData.faceCount, &result);
	CL_CHECK(result);
	clData.particleLink = clCreateBuffer(clData.context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, sizeof(glm::vec4) * particleCount, particles, &result);
	CL_CHECK(result);
	
	// loop
	while (!glfwWindowShouldClose(window) && 
		   !glfwGetKey(window, GLFW_KEY_ESCAPE)) 
	{
		float time = (float)glfwGetTime();

		// our sample volume is made of meta balls (they were placed based on a 128^3 grid)
		float scale = mcData.gridSize[0] / (float)128;
		particles[0] = glm::vec4(mcData.gridSize[0], mcData.gridSize[1], mcData.gridSize[2], 0)  * 0.5f;
		particles[1] = glm::vec4(sin(time) * 32, cos(time * 0.5f) * 32, sin(time * 2) * 16, 0) * scale + particles[0];
		particles[2] = glm::vec4(cos(-time * 0.25f) * 8, cos(time * 0.5f), cos(time) * 32, 0) * scale + particles[0];
		particles[3] = glm::vec4(sin(time) * 32, cos(time * 0.5f) * 32, cos(-time * 2) * 16, 0) * scale + particles[0];
		particles[4] = glm::vec4(sin(time) * 16, sin(time * 1.5f) * 16, sin(time * 2) * 32, 0) * scale + particles[0];
		particles[5] = glm::vec4(cos(time * 0.3f) * 32, cos(time * 1.5f) * 32, sin(time * 2) * 32, 0) * scale + particles[0];
		particles[6] = glm::vec4(sin(time) * 16, sin(time * 1.5f) * 16, sin(time * 2) * 32, 0) * scale + particles[0];
		particles[7] = glm::vec4(sin(-time) * 32, sin(time * 1.5f) * 32, cos(time * 4) * 32, 0) * scale + particles[0];

		// ensure GL is complete
		glFinish();

		// reset CL and acquire mem objects
		mcData.faceCount = 0;
		cl_event writeEvents[3] = { 0, 0, 0 };

		cl_int result = clEnqueueAcquireGLObjects(clData.queue, 1, &clData.vboLink, 0, 0, &writeEvents[0]);
		CL_CHECK(result);
		result = clEnqueueWriteBuffer(clData.queue, clData.faceCountLink, CL_FALSE, 0, sizeof(unsigned int), &mcData.faceCount, 0, nullptr, &writeEvents[1]);
		CL_CHECK(result);
		result = clEnqueueWriteBuffer(clData.queue, clData.particleLink, CL_FALSE, 0, sizeof(glm::vec4) * particleCount, particles, 0, nullptr, &writeEvents[2]);
		CL_CHECK(result);

		result = clSetKernelArg(clData.kernel, 0, sizeof(cl_int), &mcData.maxFaces);
		result |= clSetKernelArg(clData.kernel, 1, sizeof(cl_mem), &clData.faceCountLink);
		result |= clSetKernelArg(clData.kernel, 2, sizeof(cl_mem), &clData.vboLink);
		result |= clSetKernelArg(clData.kernel, 3, sizeof(cl_float), &mcData.threshold);
		result |= clSetKernelArg(clData.kernel, 4, sizeof(cl_int), &particleCount);
		result |= clSetKernelArg(clData.kernel, 5, sizeof(cl_mem), &clData.particleLink);
		CL_CHECK(result);

		// march dem cubes!
		cl_event processEvent = 0;
		result = clEnqueueNDRangeKernel(clData.queue, clData.kernel, 3, 0, mcData.gridSize, 0, 3, writeEvents, &processEvent);
		CL_CHECK(result);

		// give GL the vertex data back
		result = clEnqueueReleaseGLObjects(clData.queue, 1, &clData.vboLink, 1, &processEvent, 0);
		CL_CHECK(result);

		// read how many triangles to draw
		result = clEnqueueReadBuffer(clData.queue, clData.faceCountLink, CL_FALSE, 0, sizeof(unsigned int), &mcData.faceCount, 1, &processEvent, 0);
		CL_CHECK(result);

		// wait until cl has finished before we draw
		clFinish(clData.queue);

		// draw
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// target center of grid and spin the camera
		glm::vec3 target(mcData.gridSize[0] / 2, mcData.gridSize[1] / 2, mcData.gridSize[2] / 2);
		glm::vec3 eye(sin(time) * mcData.gridSize[0], 0, cos(time) * mcData.gridSize[0]);
		glm::mat4 pvm = glm::perspective(glm::radians(90.0f), 16 / 9.f, 0.1f, 2000.f) * glm::lookAt(target + eye, target, glm::vec3(0, 1, 0));

		glUniformMatrix4fv(pvmUniform, 1, GL_FALSE, glm::value_ptr(pvm));

		// draw blob
		glBindVertexArray(glData.vao);
		glDrawArrays(GL_TRIANGLES, 0, glm::min(mcData.faceCount, mcData.maxFaces) * 3);
		
		// white box around grid
		glBindVertexArray(boxVAO);
		glDrawArrays(GL_LINES, 0, 48);

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
	delete[] devices;

	// cleanup gl
	glDeleteBuffers(1, &glData.vbo);
	glDeleteVertexArrays(1, &glData.vao);
	glDeleteProgram(glData.program);
	glfwTerminate();

	exit(EXIT_SUCCESS);
}