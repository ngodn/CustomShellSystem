"""Clean Blender weights without iterating a collection while removing its entries."""


def clean_weights(obj, max_influences=8):
    for vertex in obj.data.vertices:
        groups = [(group.group, group.weight) for group in vertex.groups]
        kept = sorted(((index, weight) for index, weight in groups if weight > .0001),
                      key=lambda pair: pair[1], reverse=True)[:max_influences]
        if not kept:
            raise ValueError(f'{obj.name}:{vertex.index}: no retained skin weights')
        for index, _ in groups:
            obj.vertex_groups[index].remove([vertex.index])
        if len(vertex.groups):
            raise ValueError(f'{obj.name}:{vertex.index}: weight removal failed')
        total = sum(weight for _, weight in kept)
        for index, weight in kept:
            obj.vertex_groups[index].add([vertex.index], weight / total, 'REPLACE')
