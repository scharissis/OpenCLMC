OpenCLMC
========

An OpenCL implementation of the well known Marching Cubes algorithm, combined with OpenGL inter-op.

Uses OpenCL to implement a kernel to process a 3D volume and keep track of all of the triangles to render.
For now a simple hard-coded volume is used, but any could be used by simply modifying / replacing the sampleVolume() method.

Uses atomics to count up the faces and uses the value as an index into a vertex array used for rendering via OpenGL inter-op.

Included with the source is an OpenCL implementation from NVidia, taken from the CUDA SDK. It is recommended you link with your own version of OpenCL.

NOTE: There seems to be a bug with GLM for me in that they haven't properly set up the template parameters for a vec4/float4 precision issue. It's a minor fix that I'll look in to.
