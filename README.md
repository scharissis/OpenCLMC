OpenCLMC
========

An OpenCL implementation of the well known Marching Cubes algorithm, combined with OpenGL.

Uses OpenCL to implement a kernel to process a 3D volume and keep track of all of the triangles to render.

Uses Atomics to count up the triangles and uses the value as an index into a vertex array used for rendering via OpenGL inter-op.

Included with the source is an OpenCL implementation from NVidia, taken from the CUDA SDK. It is recommended you link with your own version of OpenCL.

NOTE: Current version seems to have an issue in that triangles aren't where they should be and volumes have a "fuzz" of broken triangles. This didn't previously exist, but a rebuild of the project I have taken the code from shows that the problem exists in it as well. Not quite sure why, except perhaps a change in drivers.
