from drc_decoder import drc2ply



def read_drc_as_ply(path):
    with open(path, 'rb') as file:
        drc = file.read()
    ply = drc2ply(drc)
    # print(len(drc), len(ply))
    return ply
    