# A simple script for building SPIR-V binaries from glsl shader files. Requires glslangValidator.exe to be accessible.

for i in *.{vert,geom,frag,comp}; do
    glslangValidator.exe -V $i -o $i.spv
done
