echo ATMO

cd shaders
glslangValidator -V default.vert -o default_vert.spv
glslangValidator -V default.frag -o default_frag.spv
glslangValidator -V simple.frag -o simple_frag.spv
cd ..

pak -a $1 shaders/default_vert.spv
pak -a $1 shaders/default_frag.spv
pak -a $1 shaders/simple_frag.spv
pak -a $1 dat/irradiance.dat
pak -a $1 dat/scattering.dat
pak -a $1 dat/transmittance.dat

rm shaders/*.spv
