from drc_decoder import drc2ply



def drc_file_to_ply(path):
    with open(path, 'rb') as file:
        drc = file.read()
    return drc_bytes_to_ply(drc)

def drc_bytes_to_ply(drc):
    ply = drc2ply(drc)
    # print(len(drc), len(ply))
    return ply
    