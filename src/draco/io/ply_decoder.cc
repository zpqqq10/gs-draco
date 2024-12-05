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
#include "draco/io/ply_decoder.h"

#include "draco/core/macros.h"
#include "draco/core/status.h"
#include "draco/io/file_utils.h"
#include "draco/io/ply_property_reader.h"

namespace draco {
namespace {
int64_t CountNumTriangles(const PlyElement &face_element,
                          const PlyProperty &vertex_indices) {
  int64_t num_triangles = 0;
  for (int i = 0; i < face_element.num_entries(); ++i) {
    const int64_t list_size = vertex_indices.GetListEntryNumValues(i);
    if (list_size < 3) {
      // Correctly encoded ply files don't have less than three vertices.
      continue;
    }
    num_triangles += list_size - 2;
  }
  return num_triangles;
}
}  // namespace

PlyDecoder::PlyDecoder() : out_mesh_(nullptr), out_point_cloud_(nullptr) {}

Status PlyDecoder::DecodeFromFile(const std::string &file_name,
                                  Mesh *out_mesh) {
  out_mesh_ = out_mesh;
  return DecodeFromFile(file_name, static_cast<PointCloud *>(out_mesh));
}

Status PlyDecoder::DecodeFromFile(const std::string &file_name,
                                  PointCloud *out_point_cloud) {
  std::vector<char> data;
  if (!ReadFileToBuffer(file_name, &data)) {
    return Status(Status::DRACO_ERROR, "Unable to read input file.");
  }
  buffer_.Init(data.data(), data.size());
  return DecodeFromBuffer(&buffer_, out_point_cloud);
}

Status PlyDecoder::DecodeFromBuffer(DecoderBuffer *buffer, Mesh *out_mesh) {
  out_mesh_ = out_mesh;
  return DecodeFromBuffer(buffer, static_cast<PointCloud *>(out_mesh));
}

Status PlyDecoder::DecodeFromBuffer(DecoderBuffer *buffer,
                                    PointCloud *out_point_cloud) {
  out_point_cloud_ = out_point_cloud;
  buffer_.Init(buffer->data_head(), buffer->remaining_size());
  return DecodeInternal();
}

Status PlyDecoder::DecodeInternal() {
  PlyReader ply_reader;
  DRACO_RETURN_IF_ERROR(ply_reader.Read(buffer()));
  // First, decode the connectivity data.
  // no face in point cloud
  if (out_mesh_)
    DRACO_RETURN_IF_ERROR(DecodeFaceData(ply_reader.GetElementByName("face")));
  // Decode all attributes.
  DRACO_RETURN_IF_ERROR(
      DecodeVertexData(ply_reader.GetElementByName("vertex")));
  // In case there are no faces this is just a point cloud which does
  // not require deduplication.
  if (out_mesh_ && out_mesh_->num_faces() != 0) {
#ifdef DRACO_ATTRIBUTE_VALUES_DEDUPLICATION_SUPPORTED
    if (!out_point_cloud_->DeduplicateAttributeValues()) {
      return Status(Status::DRACO_ERROR,
                    "Could not deduplicate attribute values");
    }
#endif
#ifdef DRACO_ATTRIBUTE_INDICES_DEDUPLICATION_SUPPORTED
    out_point_cloud_->DeduplicatePointIds();
#endif
  }
  return OkStatus();
}

Status PlyDecoder::DecodeFaceData(const PlyElement *face_element) {
  // We accept point clouds now.
  if (face_element == nullptr) {
    return OkStatus();
  }
  const PlyProperty *vertex_indices =
      face_element->GetPropertyByName("vertex_indices");
  if (vertex_indices == nullptr) {
    // The property name may be named either "vertex_indices" or "vertex_index".
    vertex_indices = face_element->GetPropertyByName("vertex_index");
  }
  if (vertex_indices == nullptr || !vertex_indices->is_list()) {
    return Status(Status::DRACO_ERROR, "No faces defined");
  }

  // Allocate faces.
  out_mesh_->SetNumFaces(CountNumTriangles(*face_element, *vertex_indices));
  const int64_t num_polygons = face_element->num_entries();

  PlyPropertyReader<PointIndex::ValueType> vertex_index_reader(vertex_indices);
  Mesh::Face face;
  FaceIndex face_index(0);
  for (int i = 0; i < num_polygons; ++i) {
    const int64_t list_offset = vertex_indices->GetListEntryOffset(i);
    const int64_t list_size = vertex_indices->GetListEntryNumValues(i);
    if (list_size < 3) {
      continue;  // All invalid polygons are skipped.
    }

    // Triangulate polygon assuming the polygon is convex.
    const int64_t num_triangles = list_size - 2;
    face[0] = vertex_index_reader.ReadValue(static_cast<int>(list_offset));
    for (int64_t ti = 0; ti < num_triangles; ++ti) {
      for (int64_t c = 1; c < 3; ++c) {
        face[c] = vertex_index_reader.ReadValue(
            static_cast<int>(list_offset + ti + c));
      }
      out_mesh_->SetFace(face_index, face);
      face_index++;
    }
  }
  out_mesh_->SetNumFaces(face_index.value());
  return OkStatus();
}

template <typename DataTypeT>
bool PlyDecoder::ReadPropertiesToAttribute(
    const std::vector<const PlyProperty *> &properties,
    PointAttribute *attribute, int num_vertices) {
  std::vector<std::unique_ptr<PlyPropertyReader<DataTypeT>>> readers;
  readers.reserve(properties.size());
  for (int prop = 0; prop < properties.size(); ++prop) {
    readers.push_back(std::unique_ptr<PlyPropertyReader<DataTypeT>>(
        new PlyPropertyReader<DataTypeT>(properties[prop])));
  }
  std::vector<DataTypeT> memory(properties.size());
  for (PointIndex::ValueType i = 0; i < static_cast<uint32_t>(num_vertices);
       ++i) {
    for (int prop = 0; prop < properties.size(); ++prop) {
      memory[prop] = readers[prop]->ReadValue(i);
    }
    attribute->SetAttributeValue(AttributeValueIndex(i), memory.data());
  }
  return true;
}

Status PlyDecoder::DecodeVertexData(const PlyElement *vertex_element) {
  if (vertex_element == nullptr) {
    return Status(Status::INVALID_PARAMETER, "vertex_element is null");
  }
  const PointIndex::ValueType num_vertices = vertex_element->num_entries();
  out_point_cloud_->set_num_points(num_vertices);
  // TODO(b/34330853): For now, try to load x,y,z vertices, red,green,blue,alpha
  // colors, and nx,ny,nz normals. We need to add other properties later.
  /********************* xyz *********************/
  {
    const PlyProperty *const x_prop = vertex_element->GetPropertyByName("x");
    const PlyProperty *const y_prop = vertex_element->GetPropertyByName("y");
    const PlyProperty *const z_prop = vertex_element->GetPropertyByName("z");
    if (!x_prop || !y_prop || !z_prop) {
      // Currently, we require 3 vertex coordinates (this should be generalized
      // later on).
      return Status(Status::INVALID_PARAMETER,
                    "x, y, or z property is missing");
    }
    // Decode vertex positions.
    {
      // All properties must have the same type.
      if (x_prop->data_type() != y_prop->data_type() ||
          y_prop->data_type() != z_prop->data_type()) {
        return Status(Status::INVALID_PARAMETER,
                      "x, y, and z properties must have the same type");
      }
      // TODO(ostava): For now assume the position types are float32 or int32.
      const DataType dt = x_prop->data_type();
      if (dt != DT_FLOAT32 && dt != DT_INT32) {
        return Status(
            Status::INVALID_PARAMETER,
            "x, y, and z properties must be of type float32 or int32");
      }

      GeometryAttribute va;
      va.Init(GeometryAttribute::POSITION, nullptr, 3, dt, false,
              DataTypeLength(dt) * 3, 0);
      const int att_id = out_point_cloud_->AddAttribute(va, true, num_vertices);
      std::vector<const PlyProperty *> properties;
      properties.push_back(x_prop);
      properties.push_back(y_prop);
      properties.push_back(z_prop);
      if (dt == DT_FLOAT32) {
        ReadPropertiesToAttribute<float>(
            properties, out_point_cloud_->attribute(att_id), num_vertices);
      } else if (dt == DT_INT32) {
        ReadPropertiesToAttribute<int32_t>(
            properties, out_point_cloud_->attribute(att_id), num_vertices);
      }
    }
  }

  /********************* normal *********************/
  {
    // Decode normals if present.
    const PlyProperty *const n_x_prop = vertex_element->GetPropertyByName("nx");
    const PlyProperty *const n_y_prop = vertex_element->GetPropertyByName("ny");
    const PlyProperty *const n_z_prop = vertex_element->GetPropertyByName("nz");
    if (n_x_prop != nullptr && n_y_prop != nullptr && n_z_prop != nullptr) {
      // For now, all normal properties must be set and of type float32
      if (n_x_prop->data_type() == DT_FLOAT32 &&
          n_y_prop->data_type() == DT_FLOAT32 &&
          n_z_prop->data_type() == DT_FLOAT32) {
        PlyPropertyReader<float> x_reader(n_x_prop);
        PlyPropertyReader<float> y_reader(n_y_prop);
        PlyPropertyReader<float> z_reader(n_z_prop);
        GeometryAttribute va;
        va.Init(GeometryAttribute::NORMAL, nullptr, 3, DT_FLOAT32, false,
                sizeof(float) * 3, 0);
        const int att_id =
            out_point_cloud_->AddAttribute(va, true, num_vertices);
        for (PointIndex::ValueType i = 0; i < num_vertices; ++i) {
          std::array<float, 3> val;
          val[0] = x_reader.ReadValue(i);
          val[1] = y_reader.ReadValue(i);
          val[2] = z_reader.ReadValue(i);
          out_point_cloud_->attribute(att_id)->SetAttributeValue(
              AttributeValueIndex(i), &val[0]);
        }
      }
    }
  }

  /********************* rgba *********************/
  {
    // Decode color data if present.
    int num_colors = 0;
    const PlyProperty *const r_prop = vertex_element->GetPropertyByName("red");
    const PlyProperty *const g_prop =
        vertex_element->GetPropertyByName("green");
    const PlyProperty *const b_prop = vertex_element->GetPropertyByName("blue");
    const PlyProperty *const a_prop =
        vertex_element->GetPropertyByName("alpha");
    if (r_prop) {
      ++num_colors;
    }
    if (g_prop) {
      ++num_colors;
    }
    if (b_prop) {
      ++num_colors;
    }
    if (a_prop) {
      ++num_colors;
    }

    if (num_colors) {
      std::vector<std::unique_ptr<PlyPropertyReader<uint8_t>>> color_readers;
      const PlyProperty *p;
      if (r_prop) {
        p = r_prop;
        // TODO(ostava): For now ensure the data type of all components is
        // uint8.
        DRACO_DCHECK_EQ(true, p->data_type() == DT_UINT8);
        if (p->data_type() != DT_UINT8) {
          return Status(Status::INVALID_PARAMETER,
                        "Type of 'red' property must be uint8");
        }
        color_readers.push_back(std::unique_ptr<PlyPropertyReader<uint8_t>>(
            new PlyPropertyReader<uint8_t>(p)));
      }
      if (g_prop) {
        p = g_prop;
        // TODO(ostava): For now ensure the data type of all components is
        // uint8.
        DRACO_DCHECK_EQ(true, p->data_type() == DT_UINT8);
        if (p->data_type() != DT_UINT8) {
          return Status(Status::INVALID_PARAMETER,
                        "Type of 'green' property must be uint8");
        }
        color_readers.push_back(std::unique_ptr<PlyPropertyReader<uint8_t>>(
            new PlyPropertyReader<uint8_t>(p)));
      }
      if (b_prop) {
        p = b_prop;
        // TODO(ostava): For now ensure the data type of all components is
        // uint8.
        DRACO_DCHECK_EQ(true, p->data_type() == DT_UINT8);
        if (p->data_type() != DT_UINT8) {
          return Status(Status::INVALID_PARAMETER,
                        "Type of 'blue' property must be uint8");
        }
        color_readers.push_back(std::unique_ptr<PlyPropertyReader<uint8_t>>(
            new PlyPropertyReader<uint8_t>(p)));
      }
      if (a_prop) {
        p = a_prop;
        // TODO(ostava): For now ensure the data type of all components is
        // uint8.
        DRACO_DCHECK_EQ(true, p->data_type() == DT_UINT8);
        if (p->data_type() != DT_UINT8) {
          return Status(Status::INVALID_PARAMETER,
                        "Type of 'alpha' property must be uint8");
        }
        color_readers.push_back(std::unique_ptr<PlyPropertyReader<uint8_t>>(
            new PlyPropertyReader<uint8_t>(p)));
      }

      GeometryAttribute va;
      va.Init(GeometryAttribute::COLOR, nullptr, num_colors, DT_UINT8, true,
              sizeof(uint8_t) * num_colors, 0);
      const int32_t att_id =
          out_point_cloud_->AddAttribute(va, true, num_vertices);
      for (PointIndex::ValueType i = 0; i < num_vertices; ++i) {
        std::array<uint8_t, 4> val;
        for (int j = 0; j < num_colors; j++) {
          val[j] = color_readers[j]->ReadValue(i);
        }
        out_point_cloud_->attribute(att_id)->SetAttributeValue(
            AttributeValueIndex(i), &val[0]);
      }
    }
  }

  /********************* spherical harmonics *********************/
  {
    int num_high_orders = 0;
    const PlyProperty *const dc0_prop =
        vertex_element->GetPropertyByName("f_dc_0");
    const PlyProperty *const dc1_prop =
        vertex_element->GetPropertyByName("f_dc_1");
    const PlyProperty *const dc2_prop =
        vertex_element->GetPropertyByName("f_dc_2");
    int MAXIMUM_HIGH_ORDER = (3 + 5 + 7 + 9 + 11) * 3;
    const PlyProperty *high_order_props[MAXIMUM_HIGH_ORDER];
    for (num_high_orders = 0; num_high_orders < MAXIMUM_HIGH_ORDER;
         num_high_orders++) {
      const std::string high_order_name =
          "f_rest_" + std::to_string(num_high_orders);
      high_order_props[num_high_orders] =
          vertex_element->GetPropertyByName(high_order_name);
      if (high_order_props[num_high_orders] == nullptr) {
        break;
      }
    }
    if (num_high_orders == MAXIMUM_HIGH_ORDER) {
      printf(
          "Support %d high order coefficients for spherical harmonics at "
          "most\n",
          MAXIMUM_HIGH_ORDER);
    }
    // check number of spherical harmonics coefficients
    if (num_high_orders != 0 && !dc0_prop && !dc1_prop && !dc2_prop) {
      return Status(Status::INVALID_PARAMETER,
                    "incorrect number of spherical harmonics coefficients");
    } else if (num_high_orders != 9 && num_high_orders != 24 &&
               num_high_orders != 45 && num_high_orders != 72 &&
               num_high_orders != 105) {
      return Status(Status::INVALID_PARAMETER,
                    "incorrect number of spherical harmonics coefficients");
    }
    // check data type, which should be float
    if (dc0_prop->data_type() != DT_FLOAT32 &&
        dc1_prop->data_type() == DT_FLOAT32 &&
        dc2_prop->data_type() == DT_FLOAT32) {
      return Status(Status::INVALID_PARAMETER,
                    "spherical harmonics coefficients must be float32");
    }
    for (int i = 0; i < num_high_orders; i++) {
      if (high_order_props[i]->data_type() != DT_FLOAT32) {
        return Status(Status::INVALID_PARAMETER,
                      "spherical harmonics coefficients must be float32");
      }
    }
    // For now, all coefficient properties must be set and of type float32
    {
      PlyPropertyReader<float> dc0_reader(dc0_prop);
      PlyPropertyReader<float> dc1_reader(dc1_prop);
      PlyPropertyReader<float> dc2_reader(dc2_prop);
      GeometryAttribute va;
      va.Init(GeometryAttribute::SH_DC, nullptr, 3, DT_FLOAT32, false,
              sizeof(float) * 3, 0);
      const int att_id = out_point_cloud_->AddAttribute(va, true, num_vertices);
      for (PointIndex::ValueType i = 0; i < num_vertices; ++i) {
        std::array<float, 3> val;
        val[0] = dc0_reader.ReadValue(i);
        val[1] = dc1_reader.ReadValue(i);
        val[2] = dc2_reader.ReadValue(i);
        out_point_cloud_->attribute(att_id)->SetAttributeValue(
            AttributeValueIndex(i), &val[0]);
      }
    }
    {
      std::vector<std::unique_ptr<PlyPropertyReader<float>>> high_order_readers;
      for (int i = 0; i < num_high_orders; i++) {
        high_order_readers.push_back(std::unique_ptr<PlyPropertyReader<float>>(
            new PlyPropertyReader<float>(high_order_props[i])));
      }
      GeometryAttribute va;
      va.Init(GeometryAttribute::SH_REST, nullptr, num_high_orders, DT_FLOAT32,
              false, sizeof(float) * num_high_orders, 0);
      const int att_id = out_point_cloud_->AddAttribute(va, true, num_vertices);
      for (PointIndex::ValueType i = 0; i < num_vertices; ++i) {
        std::vector<float> val(num_high_orders);
        for (int j = 0; j < num_high_orders; j++) {
          val[j] = high_order_readers[j]->ReadValue(i);
        }
        out_point_cloud_->attribute(att_id)->SetAttributeValue(
            AttributeValueIndex(i), val.data());
      }
    }
  }

  /********************* opacity *********************/
  {
    // TODO 0-1?
    const PlyProperty *const opacity_prop =
        vertex_element->GetPropertyByName("opacity");
    if (opacity_prop != nullptr) {
      if (opacity_prop->data_type() != DT_FLOAT32) {
        return Status(Status::INVALID_PARAMETER,
                      "Type of opacity property must be float32");
      }
      PlyPropertyReader<float> opacity_reader(opacity_prop);
      GeometryAttribute va;
      va.Init(GeometryAttribute::OPACITY, nullptr, 1, DT_FLOAT32, false,
              sizeof(float), 0);
      const int att_id = out_point_cloud_->AddAttribute(va, true, num_vertices);
      for (PointIndex::ValueType i = 0; i < num_vertices; ++i) {
        float val = opacity_reader.ReadValue(i);
        out_point_cloud_->attribute(att_id)->SetAttributeValue(
            AttributeValueIndex(i), &val);
      }
    }
  }

  /********************* scales *********************/
  {
    // TODO 0-1?
    int num_scales = 0;
    const PlyProperty *const scale0_prop =
        vertex_element->GetPropertyByName("scale_0");
    const PlyProperty *const scale1_prop =
        vertex_element->GetPropertyByName("scale_1");
    const PlyProperty *const scale2_prop =
        vertex_element->GetPropertyByName("scale_2");
    if (scale0_prop) {
      ++num_scales;
    }
    if (scale1_prop) {
      ++num_scales;
    }
    if (scale2_prop) {
      // if not, this is 2d gaussian
      ++num_scales;
    }
    if (num_scales) {
      std::vector<std::unique_ptr<PlyPropertyReader<float>>> scale_readers;
      if (scale0_prop) {
        if (scale0_prop->data_type() != DT_FLOAT32) {
          return Status(Status::INVALID_PARAMETER,
                        "Type of scale property must be float32");
        }
        scale_readers.push_back(std::unique_ptr<PlyPropertyReader<float>>(
            new PlyPropertyReader<float>(scale0_prop)));
      }
      if (scale1_prop) {
        if (scale1_prop->data_type() != DT_FLOAT32) {
          return Status(Status::INVALID_PARAMETER,
                        "Type of scale property must be float32");
        }
        scale_readers.push_back(std::unique_ptr<PlyPropertyReader<float>>(
            new PlyPropertyReader<float>(scale1_prop)));
      }
      if (scale2_prop) {
        if (scale2_prop->data_type() != DT_FLOAT32) {
          return Status(Status::INVALID_PARAMETER,
                        "Type of scale property must be float32");
        }
        scale_readers.push_back(std::unique_ptr<PlyPropertyReader<float>>(
            new PlyPropertyReader<float>(scale2_prop)));
      }

      GeometryAttribute va;
      va.Init(GeometryAttribute::SCALE, nullptr, num_scales, DT_FLOAT32, false,
              sizeof(float) * num_scales, 0);
      const int32_t att_id =
          out_point_cloud_->AddAttribute(va, true, num_vertices);
      for (PointIndex::ValueType i = 0; i < num_vertices; ++i) {
        std::array<float, 3> val;
        for (int j = 0; j < num_scales; j++) {
          val[j] = scale_readers[j]->ReadValue(i);
        }
        out_point_cloud_->attribute(att_id)->SetAttributeValue(
            AttributeValueIndex(i), &val[0]);
      }
    }
  }

  /********************* quaternions *********************/
  {
    const PlyProperty *const rot0_prop =
        vertex_element->GetPropertyByName("rot_0");
    const PlyProperty *const rot1_prop =
        vertex_element->GetPropertyByName("rot_1");
    const PlyProperty *const rot2_prop =
        vertex_element->GetPropertyByName("rot_2");
    const PlyProperty *const rot3_prop =
        vertex_element->GetPropertyByName("rot_3");
    std::array<std::unique_ptr<PlyPropertyReader<float>>, 4> quat_readers;
    if (rot0_prop) {
      if (rot0_prop->data_type() != DT_FLOAT32) {
        return Status(Status::INVALID_PARAMETER,
                      "Type of rotation property must be float32");
      }
      quat_readers[0] = std::unique_ptr<PlyPropertyReader<float>>(
          new PlyPropertyReader<float>(rot0_prop));
    }
    if (rot1_prop) {
      if (rot1_prop->data_type() != DT_FLOAT32) {
        return Status(Status::INVALID_PARAMETER,
                      "Type of rotation property must be float32");
      }
      quat_readers[1] = std::unique_ptr<PlyPropertyReader<float>>(
          new PlyPropertyReader<float>(rot1_prop));
    }
    if (rot2_prop) {
      if (rot2_prop->data_type() != DT_FLOAT32) {
        return Status(Status::INVALID_PARAMETER,
                      "Type of rotation property must be float32");
      }
      quat_readers[2] = std::unique_ptr<PlyPropertyReader<float>>(
          new PlyPropertyReader<float>(rot2_prop));
    }
    if (rot3_prop) {
      if (rot3_prop->data_type() != DT_FLOAT32) {
        return Status(Status::INVALID_PARAMETER,
                      "Type of rotation property must be float32");
      }
      quat_readers[3] = std::unique_ptr<PlyPropertyReader<float>>(
          new PlyPropertyReader<float>(rot3_prop));
    }

    GeometryAttribute va;
    va.Init(GeometryAttribute::ROTATION, nullptr, 4, DT_FLOAT32, false,
            sizeof(float) * 4, 0);
    const int32_t att_id =
        out_point_cloud_->AddAttribute(va, true, num_vertices);
    for (PointIndex::ValueType i = 0; i < num_vertices; ++i) {
      std::array<float, 4> val;
      for (int j = 0; j < 4; j++) {
        val[j] = quat_readers[j]->ReadValue(i);
      }
      out_point_cloud_->attribute(att_id)->SetAttributeValue(
          AttributeValueIndex(i), &val[0]);
    }
  }

  /********************* auxiliary data  **********************/
  {
    // TODO 0-1?
    const PlyProperty *const aux_prop =
        vertex_element->GetPropertyByName("segment");
    // check data type, which should be float
    if (aux_prop != nullptr && aux_prop->data_type() != DT_FLOAT32) {
      return Status(Status::INVALID_PARAMETER,
                    "Type of auxiliary data must be float32");
    }
    // For now, all auxiliary data properties must be set and of type float32
    PlyPropertyReader<float> aux_reader(aux_prop);
    GeometryAttribute va;
    va.Init(GeometryAttribute::AUX, nullptr, 1, DT_FLOAT32, false, sizeof(float), 0);
    const int att_id = out_point_cloud_->AddAttribute(va, true, num_vertices);
    for (PointIndex::ValueType i = 0; i < num_vertices; ++i) {
      float val = aux_reader.ReadValue(i);
      out_point_cloud_->attribute(att_id)->SetAttributeValue(
          AttributeValueIndex(i), &val);
    }
  }
  // /********************* auxiliary data  **********************/
  // {
  //   // TODO 0-1?
  //   int num_aux = 0;
  //   int MAXIMUM_FEAT_DIM = 16;
  //   const PlyProperty *aux_props[MAXIMUM_FEAT_DIM];
  //   for (num_aux = 0; num_aux < MAXIMUM_FEAT_DIM; num_aux++) {
  //     const std::string aux_name = "f_aux_" + std::to_string(num_aux);
  //     aux_props[num_aux] = vertex_element->GetPropertyByName(aux_name);
  //     if (aux_props[num_aux] == nullptr) {
  //       break;
  //     }
  //   }
  //   if (num_aux == MAXIMUM_FEAT_DIM) {
  //     printf("Support %d feature dim for auxiliary data at most\n",
  //            MAXIMUM_FEAT_DIM);
  //   }
  //   // check data type, which should be float
  //   for (int i = 0; i < num_aux; i++) {
  //     if (aux_props[i]->data_type() != DT_FLOAT32) {
  //       return Status(Status::INVALID_PARAMETER,
  //                     "auxiliary data must be float32");
  //     }
  //   }
  //   // For now, all auxiliary data properties must be set and of type float32
  //   {
  //     std::vector<std::unique_ptr<PlyPropertyReader<float>>> aux_readers;
  //     for (int i = 0; i < num_aux; i++) {
  //       aux_readers.push_back(std::unique_ptr<PlyPropertyReader<float>>(
  //           new PlyPropertyReader<float>(aux_props[i])));
  //     }
  //     GeometryAttribute va;
  //     va.Init(GeometryAttribute::AUX, nullptr, num_aux, DT_FLOAT32, false,
  //             sizeof(float) * num_aux, 0);
  //     const int att_id = out_point_cloud_->AddAttribute(va, true,
  //     num_vertices); for (PointIndex::ValueType i = 0; i < num_vertices; ++i)
  //     {
  //       std::vector<float> val(num_aux);
  //       for (int j = 0; j < num_aux; j++) {
  //         val[j] = aux_readers[j]->ReadValue(i);
  //       }
  //       out_point_cloud_->attribute(att_id)->SetAttributeValue(
  //           AttributeValueIndex(i), val.data());
  //     }
  //   }
  // }

  return OkStatus();
}

}  // namespace draco
