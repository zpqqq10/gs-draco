// Copyright 2016 The Draco Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
#include <emscripten/emscripten.h>

#include <cinttypes>

#include "draco/compression/decode.h"
#include "draco/io/ply_encoder.h"

int main(int argc, char **argv) {
  printf("wasm decoder loaded\n");
  return 0;
}

#ifdef __cplusplus
extern "C" {
#endif

int EMSCRIPTEN_KEEPALIVE test(int num1, int num2) { return num1 + num2; }

int EMSCRIPTEN_KEEPALIVE drc2ply(const char *input, const int inputLength,
                                 char *output) {
  if (inputLength <= 0) {
    printf("Empty input file.\n");
    return -1;
  }

  std::vector<char> data;
  data.resize(inputLength);
  memcpy(data.data(), input, inputLength);

  // Create a draco decoding buffer. Note that no data is copied in this step.
  draco::DecoderBuffer buffer;
  buffer.Init(data.data(), data.size());

  // Decode the input data into a geometry.
  std::unique_ptr<draco::PointCloud> pc;
  auto type_statusor = draco::Decoder::GetEncodedGeometryType(&buffer);
  if (!type_statusor.ok()) {
    printf("Failed to decode the input file %s\n",
           type_statusor.status().error_msg());
    return -1;
  }
  const draco::EncodedGeometryType geom_type = type_statusor.value();
  if (geom_type == draco::TRIANGULAR_MESH) {
    printf("Unsupported type\n");
    return -1;
  } else if (geom_type == draco::POINT_CLOUD) {
    // Failed to decode it as mesh, so let's try to decode it as a point cloud.
    draco::Decoder decoder;
    auto statusor = decoder.DecodePointCloudFromBuffer(&buffer);
    if (!statusor.ok()) {
      printf("Failed to decode the input file %s\n",
             statusor.status().error_msg());
      return -1;
    }
    pc = std::move(statusor).value();
  }

  if (pc == nullptr) {
    printf("Failed to decode the input file.\n");
    return -1;
  }

  draco::PlyEncoder ply_encoder;
  draco::EncoderBuffer resBuffer;
  if (!ply_encoder.EncodeToBuffer(*pc, &resBuffer)) {
    printf("Failed to store the decoded point cloud as PLY.\n");
    return -1;
  }

  // assgin value
  memcpy(output, resBuffer.data(), resBuffer.size());
  return resBuffer.size();
}

#ifdef __cplusplus
}
#endif