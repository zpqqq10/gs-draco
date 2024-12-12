import plyfile
import numpy as np

# reference = plyfile.PlyData.read('../compact3d/gaussian-splatting/output/post-vq/train/point_cloud/post_vq/vq_index_pc.ply')
# restore = plyfile.PlyData.read('restore.ply')

# reference = plyfile.PlyData.read('../compact3d/gaussian-splatting/output/post-vq/train/point_cloud/post_vq/full_vq_pc.ply')
# restore = plyfile.PlyData.read('full_vq_pc.ply')

# reference = plyfile.PlyData.read('../compact3d/gaussian-splatting/output/post-vq/train/point_cloud/post_vq/sh_index_pc.ply')
# restore = plyfile.PlyData.read('sh_index_pc.ply')

# reference = plyfile.PlyData.read('../morton_pc.ply')
# restore = plyfile.PlyData.read('../morton_pc_qp12_decoded.ply')

reference = plyfile.PlyData.read('../processSingleGroup/point_cloud.ply')
restore = plyfile.PlyData.read('./restore.ply')


# convert to dict
# value is numpy array
def ply2dict(ply: plyfile.PlyData):
    data = ply.elements[0].data
    res = {}
    for name in data.dtype.names:
        res[name] = np.array(data[name])
    return res

def difference(ref, res):
    for key in res.keys():
        # draco may change the order of the points
        # so we need to sort the array
        # but if cl is set to 0, sequential encoding is used and the order is kept
        diff = np.abs(ref[key] - res[key])
        # diff = np.abs(np.sort(ref[key]) - np.sort(res[key]))
        print(key)
        print(diff.max(), diff.min())
        print(ref[key].max(), res[key].max(), ref[key].min(), res[key].min())
        print('-------------------------')

reference = ply2dict(reference)
restore = ply2dict(restore)

difference(reference, restore)