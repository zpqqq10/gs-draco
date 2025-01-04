TARGET = drc2ply.js

# define Emscripten compiler
EMCC = emcc

# compile flags
# CFLAGS = -s WASM=1 -s MODULARIZE=1 -s 'EXPORT_NAME="drc2plyModule"' -s EXPORTED_RUNTIME_METHODS='["ccall", "cwrap"]'
CFLAGS = -s WASM=1 -s MODULARIZE=1 -s 'EXPORT_NAME="drc2plyModule"' -s EXPORTED_FUNCTIONS='["_main", "_drc2ply", "_test", "_malloc", "_free", "getValue"]' -s EXPORTED_RUNTIME_METHODS='["ccall", "cwrap"]'
INCLUDE_DIRS = \
./draco/compression ./draco/io ./draco ./draco/core ./draco/compression/point_cloud \
../build
# ../third_party/googletest/googletest/include

# source files
SRCDIRS = \
./wasm_decoder.cc ./draco/compression ./draco/io ./darco/core \
./draco/compression/point_cloud ./draco/compression/mesh ./draco/compression/attributes ./draco/compression/point_cloud/algorithms ./draco/compression/bit_coders ./draco/compression/entropy \
./draco/mesh ./draco/point_cloud ./draco/attributes ./draco/metadata

SRCS := \
wasm_decoder.cc \
draco/compression/decode.cc \
draco/core/decoder_buffer.cc \
draco/core/encoder_buffer.cc \
draco/core/draco_types.cc \
draco/core/data_buffer.cc \
draco/core/bit_utils.cc \
draco/core/options.cc \
draco/core/quantization_utils.cc \
draco/point_cloud/point_cloud.cc \
draco/mesh/mesh.cc \
draco/mesh/corner_table.cc \
draco/mesh/mesh_attribute_corner_table.cc \
draco/io/mesh_io.cc \
draco/io/ply_decoder.cc \
draco/io/ply_encoder.cc \
draco/io/ply_reader.cc \
draco/io/point_cloud_io.cc \
draco/compression/point_cloud/point_cloud_decoder.cc \
draco/compression/point_cloud/point_cloud_kd_tree_decoder.cc \
draco/compression/point_cloud/point_cloud_sequential_decoder.cc \
draco/compression/mesh/mesh_decoder.cc \
draco/compression/mesh/mesh_sequential_decoder.cc \
draco/compression/mesh/mesh_edgebreaker_decoder.cc \
draco/compression/mesh/mesh_edgebreaker_decoder_impl.cc \
draco/compression/attributes/attributes_decoder.cc \
draco/compression/attributes/kd_tree_attributes_decoder.cc \
draco/compression/attributes/sequential_attribute_decoders_controller.cc \
draco/compression/attributes/sequential_attribute_decoder.cc \
draco/compression/attributes/sequential_integer_attribute_decoder.cc \
draco/compression/attributes/sequential_normal_attribute_decoder.cc \
draco/compression/attributes/sequential_quantization_attribute_decoder.cc \
draco/compression/point_cloud/algorithms/dynamic_integer_points_kd_tree_decoder.cc \
draco/compression/point_cloud/algorithms/float_points_tree_decoder.cc \
draco/compression/bit_coders/direct_bit_decoder.cc \
draco/compression/bit_coders/rans_bit_decoder.cc \
draco/compression/entropy/symbol_decoding.cc \
draco/attributes/attribute_quantization_transform.cc \
draco/attributes/attribute_octahedron_transform.cc \
draco/attributes/attribute_transform.cc \
draco/attributes/point_attribute.cc \
draco/attributes/geometry_attribute.cc \
draco/metadata/metadata.cc \
draco/metadata/geometry_metadata.cc \
draco/metadata/metadata_decoder.cc

CFLAGS += $(addprefix -I,$(INCLUDE_DIRS))
# 默认目标
all: $(TARGET)

$(TARGET): $(SRCS)
	$(EMCC) $(CFLAGS) $^ -o $@

# 清理生成的文件
clean:
	rm -rf $(TARGET)

# 重新构建整个项目
rebuild: clean all

.PHONY: all clean rebuild