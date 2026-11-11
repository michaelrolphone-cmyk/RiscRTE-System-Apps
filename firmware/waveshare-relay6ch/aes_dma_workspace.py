"""Pinned ESP-IDF AES descriptor lifetime repair for the no-PSRAM relay.
The driver already waits for DMA completion before freeing these descriptors.
Keep normal TLS-record descriptors on the internal task stack for that lifetime;
retain the SDK heap path for larger operations or non-DMA-capable stacks.
"""
import hashlib
import json
import pathlib
import sys

EXPECTED = "e9d05275143c0d9e5d02193162f7401a9df0c6810d99a724f42e8beb01ddd52e"

def patch(source):
    if hashlib.sha256(source.encode()).hexdigest() != EXPECTED:
        raise ValueError("Unrecognized pinned ESP-IDF AES driver")
    declaration = "    crypto_dma_desc_t *block_desc = NULL, *block_in_desc = NULL, *block_out_desc = NULL;"
    workspace = """    crypto_dma_desc_t stack_block_desc[16] __attribute__((aligned(DMA_DESC_MEM_ALIGN_SIZE))) = {0};
    _Static_assert(sizeof(stack_block_desc) <= 256, "Bounded AES descriptor stack workspace");
    _Static_assert(16 >= 2 * ((16384 + DMA_DESCRIPTOR_BUFFER_MAX_SIZE_4B_ALIGNED - 1) /
                   DMA_DESCRIPTOR_BUFFER_MAX_SIZE_4B_ALIGNED) + 1, "Workspace covers a full TLS record");
"""
    if source.count(declaration) != 2:
        raise ValueError("AES driver descriptor declarations changed")
    source = source.replace(declaration, workspace + declaration)
    allocations = [
        ("crypto_dma_desc_num * 2", "        "),
        ("(crypto_dma_desc_num * 2) + 1", "    "),
    ]
    for count, indent in allocations:
        old = indent + "block_desc = heap_caps_aligned_calloc(DMA_DESC_MEM_ALIGN_SIZE, " + count + ", sizeof(crypto_dma_desc_t), MALLOC_CAP_DMA);"
        if source.count(old) != 1:
            raise ValueError("AES descriptor allocation changed")
        new = indent + "block_desc = (" + count + " <= 16 && esp_ptr_dma_capable(stack_block_desc)) ? stack_block_desc :\n"
        new += indent + "    heap_caps_aligned_calloc(DMA_DESC_MEM_ALIGN_SIZE, " + count + ", sizeof(crypto_dma_desc_t), MALLOC_CAP_DMA);"
        source = source.replace(old, new)
    if source.count("    free(block_desc);") != 2:
        raise ValueError("AES descriptor cleanup changed")
    source = source.replace("    free(block_desc);", "    if (block_desc != stack_block_desc) free(block_desc);")
    return source

if __name__ == "__main__":
    original = pathlib.Path(sys.argv[1]).read_text()
    result = patch(original)
    pathlib.Path(sys.argv[2]).write_text(result)
    print("AES_STACK_DMA_SOURCE_PROOF " + json.dumps({
        "original_sha256": EXPECTED,
        "patched_sha256": hashlib.sha256(result.encode()).hexdigest(),
        "descriptor_workspaces": 2,
        "descriptors_per_workspace": 16,
        "maximum_workspace_bytes": 256,
        "larger_operation_heap_path_retained": True,
    }))
