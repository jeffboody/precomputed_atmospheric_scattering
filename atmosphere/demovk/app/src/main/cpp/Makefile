export CC_USE_MATH  = 1
export VKK_USE_VKUI = 1

TARGET   = demovk
CLASSES  = atmo_renderer
SOURCE   = $(TARGET).c $(CLASSES:%=%.c)
OBJECTS  = $(TARGET).o $(CLASSES:%=%.o)
HFILES   = $(CLASSES:%=%.h)
OPT      = -O2 -Wall
CFLAGS   = $(OPT) -I. -I$(VULKAN_SDK)/include `sdl2-config --cflags` -DA3D_GL2
LDFLAGS  = -Llibvkk -lvkk -L$(VULKAN_SDK)/lib -lvulkan `sdl2-config --libs` -Llibpak -lpak -Llibxmlstream -lxmlstream -Ltexgz -ltexgz -Llibcc -lcc -Llibexpat/expat/lib -lexpat -lm -lpthread -lz -ljpeg
CCC      = gcc

all: $(TARGET) libcc libpak libxmlstream texgz libvkk libexpat

$(TARGET): $(OBJECTS) libcc libpak libxmlstream texgz libvkk libexpat
	$(CCC) $(OPT) $(OBJECTS) -o $@ $(LDFLAGS)

.PHONY: libcc libpak libxmlstream texgz libvkk libexpat

libcc:
	$(MAKE) -C libcc

libpak:
	$(MAKE) -C libpak

libxmlstream:
	$(MAKE) -C libxmlstream

texgz:
	$(MAKE) -C texgz

libvkk:
	$(MAKE) -C libvkk

libexpat:
	$(MAKE) -C libexpat/expat/lib

clean:
	rm -f $(OBJECTS) *~ \#*\# $(TARGET)
	$(MAKE) -C libcc clean
	$(MAKE) -C libpak clean
	$(MAKE) -C libxmlstream clean
	$(MAKE) -C texgz clean
	$(MAKE) -C libvkk clean
	$(MAKE) -C libexpat/expat/lib clean

$(OBJECTS): $(HFILES)
