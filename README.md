#Qt + Vulkan Nbody Demo
![image](https://github.com/AndreasReiten/qtvknbody/blob/master/doc/screenshot_nbody_2016_08_15_15_24_22.png)

This is a Qt program that uses the Vulkan API to render a classical N-body simulation. Both graphics and compute shaders are employed. Graphical enhancements include bloom and high dynamic range rendering (hdr).

The program draws inspiration and uses some code from the following examples and tutorials. I would recommend these to anyone wanting to learn the Vulkan API: 
* Niko Kauppi's Vulkan tutorials on Youtube (https://youtu.be/wHt5wcxIPcE). 
* Sascha Willems' Vulkan examples on Github (https://github.com/SaschaWillems/Vulkan).

## Program overview
In short, the program works like this:

![image](https://github.com/AndreasReiten/qtvknbody/blob/master/doc/path4572.png)

## Hardware requirements 
A graphics card that supports Vulkan. See for example https://en.wikipedia.org/wiki/Vulkan_(API)#Compatibility

## Build instructions
Windows: Make sure you have the latest Vulkan SDK and graphics card drivers.  
Linux: Make sure you have the latest Vulkan libraries and graphics drivers from your package manager.

The easiest thing to do next is to open the .pro file using the QtCreator IDE and build it from there.

## Binaries
A pre-compiled 64-bit binary for Windows can be found in the "Releases" tab. 

![alt tag](https://github.com/AndreasReiten/qtvknbody/blob/master/doc/screenshot_nbody_2016_08_15_15_27_52.png)

[![youtube video](https://img.youtube.com/vi/BsCqkKZtRfU/0.jpg)](https://www.youtube.com/watch?v=BsCqkKZtRfU)
