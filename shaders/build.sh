for i in *.{vert,geom,frag,comp}; do
    glslangValidator.exe -V $i -o $i.spv
done
