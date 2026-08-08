

#Demo Nbody con Qt + Vulkan
![image](https://github.com/AndreasReiten/qtvknbody/blob/master/doc/screenshot_nbody_2016_08_15_15_24_22.png)

###_[Video en Youtube](https://youtu.be/BsCqkKZtRfU)_

Actualización: Este ejemplo no se ha actualizado desde los commits iniciales. Desde entonces, ha habido algunos avances en Vulkan y se han introducido las clases basadas en QtVulkan. También hay muchas cosas que haría de manera diferente con respecto al código limpio y la legibilidad. ¡Así que úsalo bajo tu propio riesgo!

Este es un programa de Qt que utiliza la API de Vulkan para renderizar una simulación clásica Nbody. Se emplean tanto shaders gráficos como de cálculo (compute). Las mejoras gráficas incluyen bloom (resplandor) y renderizado de alto rango dinámico (HDR).

El programa se inspira y utiliza parte del código de los siguientes ejemplos y tutoriales. Los recomendaría a cualquiera que desee aprender la API de Vulkan: 
* Los tutoriales de Vulkan de Niko Kauppi en Youtube (https://youtu.be/wHt5wcxIPcE). 
* Los ejemplos de Vulkan de Sascha Willems en Github (https://github.com/SaschaWillems/Vulkan).

## Descripción general del programa
En resumen, el programa funciona de la siguiente manera:

![image](https://github.com/AndreasReiten/qtvknbody/blob/master/doc/path4572.png)

## Requisitos de hardware 
Una tarjeta gráfica que admita Vulkan. Consulta, por ejemplo, https://en.wikipedia.org/wiki/Vulkan_(API)#Compatibility

## Instrucciones de compilación
Windows: Asegúrate de tener el último SDK de Vulkan y los controladores de la tarjeta gráfica.  
Linux: Asegúrate de tener las últimas bibliotecas de Vulkan y los controladores gráficos desde tu gestor de paquetes.

Lo más fácil que puedes hacer a continuación es abrir el archivo .pro utilizando el IDE QtCreator y compilarlo desde allí.

## Binarios
Un binario de 64 bits precompilado para Windows se puede encontrar en la pestaña "Releases". 

![alt tag](https://github.com/AndreasReiten/qtvknbody/blob/master/doc/screenshot_nbody_2016_08_15_15_27_52.png)
