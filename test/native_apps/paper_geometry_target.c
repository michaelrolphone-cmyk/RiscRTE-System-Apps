#include "PortablePaperGeometry.h"

/* Runtime arguments keep both ordinary and wide-reference paths in the ELF. */
__attribute__((visibility("default")))
bool paper_project_probe(int32_t start, int32_t length, int32_t offset,
        uint32_t extent, uint32_t reference, int32_t *position, int32_t *size) {
    return portable_paper_project_axis(start,length,offset,extent,reference,
                                       position,size);
}
