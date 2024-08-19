# Makefile for building shaders.
# Copyright (C) 2024  dbstream

# HACK!
#  Ideally we'd invoke glslang using CMake, but that is difficult to do
#  in a good way.
#
#  add_custom_commands() is evil.
# ~david

GLSLANG := glslang
GLSLFLAGS := -V100 --glsl-version 460 -Os -g0

all: example.frag.txt example.vert.txt

define DECLARE_SHADER_EXT
%.$1.txt: %.$1.glsl
	$$(GLSLANG) $$(GLSLFLAGS) --vn $$*_$1 -o $$@ $$<
endef

$(foreach ext,frag vert,$(eval $(call DECLARE_SHADER_EXT,$(ext))))
