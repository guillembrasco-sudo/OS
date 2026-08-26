#include <cuda_runtime.h>

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <vector>
#include <algorithm>

#define CUDA_CHECK(x)                                                     \
    do {                                                                   \
        cudaError_t err__ = (x);                                           \
        if (err__ != cudaSuccess) {                                        \
            fprintf(stderr,                                                \
                    "[CUDA] %s:%d: %s\n",                                  \
                    __FILE__,                                               \
                    __LINE__,                                               \
                    cudaGetErrorString(err__));                            \
            std::exit(EXIT_FAILURE);                                       \
        }                                                                  \
    } while (0)


// ============================================================
// CONFIGURATION
// ============================================================

constexpr int BLOCK_SIZE = 256;

constexpr size_t ELEMENTS_PER_GPU =
    16 * 1024 * 1024;

constexpr int ROUNDS = 4;


// ============================================================
// DEVICE DATA
// ============================================================

struct FabricPartition
{
    size_t globalOffset;
    size_t elements;
};


// ============================================================
// GPU NODE
// ============================================================

struct GPUNode
{
    int id;

    FabricPartition partition;

    float* data;
    float* working;
    float* compressed;
    float* received;
    float* output;

    cudaStream_t computeStream;
    cudaStream_t compressionStream;
    cudaStream_t transferStream;
    cudaStream_t decompressionStream;

    cudaEvent_t computeReady;
    cudaEvent_t compressionReady;
    cudaEvent_t transferReady;
    cudaEvent_t decompressionReady;
};


// ============================================================
// SIMPLE COMPUTATION
// ============================================================
//
// Todas las GPUs ejecutan EXACTAMENTE el mismo kernel.
// Cada GPU recibe una región diferente del tensor.
//

__global__
void computeKernel(
    float* data,
    float* output,
    size_t count,
    float factor)
{
    size_t i =
        blockIdx.x * blockDim.x +
        threadIdx.x;

    if (i >= count)
        return;

    float x = data[i];

    x = x * factor;
    x = x + 1.0f;
    x = x * x;

    output[i] = x;
}


// ============================================================
// GPU COMPRESSION
// ============================================================
//
// Prototipo de compresión.
//
// Aquí actualmente hacemos una transformación reversible
// muy barata.
//
// En una implementación real podría sustituirse por:
//
//   RLE
//   delta encoding
//   bit packing
//   quantization
//   FP16
//   BF16
//   FP8
//   custom tensor compression
//   LZ4 GPU
//   ZSTD GPU
//
// La interfaz no cambia.
//

__global__
void compressKernel(
    const float* input,
    float* compressed,
    size_t count)
{
    size_t i =
        blockIdx.x * blockDim.x +
        threadIdx.x;

    if (i >= count)
        return;

    compressed[i] = input[i];
}


// ============================================================
// GPU DECOMPRESSION
// ============================================================

__global__
void decompressKernel(
    const float* compressed,
    float* output,
    size_t count)
{
    size_t i =
        blockIdx.x * blockDim.x +
        threadIdx.x;

    if (i >= count)
        return;

    output[i] = compressed[i];
}


// ============================================================
// INITIALIZE GPU
// ============================================================

void initializeGPU(
    GPUNode& gpu,
    size_t globalOffset)
{
    CUDA_CHECK(
        cudaSetDevice(gpu.id)
    );

    gpu.partition.globalOffset =
        globalOffset;

    gpu.partition.elements =
        ELEMENTS_PER_GPU;


    // --------------------------------------------------------
    // STREAMS
    // --------------------------------------------------------

    CUDA_CHECK(
        cudaStreamCreateWithFlags(
            &gpu.computeStream,
            cudaStreamNonBlocking
        )
    );

    CUDA_CHECK(
        cudaStreamCreateWithFlags(
            &gpu.compressionStream,
            cudaStreamNonBlocking
        )
    );

    CUDA_CHECK(
        cudaStreamCreateWithFlags(
            &gpu.transferStream,
            cudaStreamNonBlocking
        )
    );

    CUDA_CHECK(
        cudaStreamCreateWithFlags(
            &gpu.decompressionStream,
            cudaStreamNonBlocking
        )
    );


    // --------------------------------------------------------
    // EVENTS
    // --------------------------------------------------------

    CUDA_CHECK(
        cudaEventCreateWithFlags(
            &gpu.computeReady,
            cudaEventDisableTiming
        )
    );

    CUDA_CHECK(
        cudaEventCreateWithFlags(
            &gpu.compressionReady,
            cudaEventDisableTiming
        )
    );

    CUDA_CHECK(
        cudaEventCreateWithFlags(
            &gpu.transferReady,
            cudaEventDisableTiming
        )
    );

    CUDA_CHECK(
        cudaEventCreateWithFlags(
            &gpu.decompressionReady,
            cudaEventDisableTiming
        )
    );


    size_t bytes =
        ELEMENTS_PER_GPU *
        sizeof(float);


    // --------------------------------------------------------
    // MEMORY
    // --------------------------------------------------------

    CUDA_CHECK(
        cudaMalloc(
            &gpu.data,
            bytes
        )
    );

    CUDA_CHECK(
        cudaMalloc(
            &gpu.working,
            bytes
        )
    );

    CUDA_CHECK(
        cudaMalloc(
            &gpu.compressed,
            bytes
        )
    );

    CUDA_CHECK(
        cudaMalloc(
            &gpu.received,
            bytes
        )
    );

    CUDA_CHECK(
        cudaMalloc(
            &gpu.output,
            bytes
        )
    );


    CUDA_CHECK(
        cudaMemset(
            gpu.data,
            0,
            bytes
        )
    );
}


// ============================================================
// ENABLE P2P
// ============================================================

bool enableP2P(
    int source,
    int destination)
{
    int capable = 0;

    CUDA_CHECK(
        cudaDeviceCanAccessPeer(
            &capable,
            source,
            destination
        )
    );

    if (!capable)
        return false;


    CUDA_CHECK(
        cudaSetDevice(source)
    );


    cudaError_t result =
        cudaDeviceEnablePeerAccess(
            destination,
            0
        );


    if (result ==
        cudaErrorPeerAccessAlreadyEnabled)
    {
        cudaGetLastError();
        return true;
    }


    if (result != cudaSuccess)
        return false;


    return true;
}


// ============================================================
// COMPUTE PHASE
// ============================================================

void runCompute(
    GPUNode& gpu,
    float factor)
{
    CUDA_CHECK(
        cudaSetDevice(gpu.id)
    );


    int blocks =
        static_cast<int>(
            (gpu.partition.elements +
             BLOCK_SIZE - 1) /
            BLOCK_SIZE
        );


    computeKernel<<<
        blocks,
        BLOCK_SIZE,
        0,
        gpu.computeStream
    >>>(
        gpu.data,
        gpu.working,
        gpu.partition.elements,
        factor
    );


    CUDA_CHECK(
        cudaGetLastError()
    );


    CUDA_CHECK(
        cudaEventRecord(
            gpu.computeReady,
            gpu.computeStream
        )
    );
}


// ============================================================
// COMPRESSION PHASE
// ============================================================

void runCompression(
    GPUNode& gpu)
{
    CUDA_CHECK(
        cudaSetDevice(gpu.id)
    );


    CUDA_CHECK(
        cudaStreamWaitEvent(
            gpu.compressionStream,
            gpu.computeReady,
            0
        )
    );


    int blocks =
        static_cast<int>(
            (gpu.partition.elements +
             BLOCK_SIZE - 1) /
            BLOCK_SIZE
        );


    compressKernel<<<
        blocks,
        BLOCK_SIZE,
        0,
        gpu.compressionStream
    >>>(
        gpu.working,
        gpu.compressed,
        gpu.partition.elements
    );


    CUDA_CHECK(
        cudaGetLastError()
    );


    CUDA_CHECK(
        cudaEventRecord(
            gpu.compressionReady,
            gpu.compressionStream
        )
    );
}


// ============================================================
// GPU → GPU TRANSFER
// ============================================================
//
// IMPORTANT:
//
// Todas las GPUs son equivalentes.
//
// GPU A puede enviar a GPU B.
// GPU B puede simultáneamente enviar a GPU C.
// GPU C puede enviar a GPU A.
//
// No existe una GPU "maestra".
//

void transfer(
    GPUNode& source,
    GPUNode& destination)
{
    CUDA_CHECK(
        cudaSetDevice(destination.id)
    );


    CUDA_CHECK(
        cudaStreamWaitEvent(
            destination.transferStream,
            source.compressionReady,
            0
        )
    );


    size_t bytes =
        source.partition.elements *
        sizeof(float);


    CUDA_CHECK(
        cudaMemcpyPeerAsync(
            destination.received,
            destination.id,

            source.compressed,
            source.id,

            bytes,

            destination.transferStream
        )
    );


    CUDA_CHECK(
        cudaEventRecord(
            destination.transferReady,
            destination.transferStream
        )
    );
}


// ============================================================
// DECOMPRESSION
// ============================================================

void runDecompression(
    GPUNode& gpu)
{
    CUDA_CHECK(
        cudaSetDevice(gpu.id)
    );


    CUDA_CHECK(
        cudaStreamWaitEvent(
            gpu.decompressionStream,
            gpu.transferReady,
            0
        )
    );


    int blocks =
        static_cast<int>(
            (gpu.partition.elements +
             BLOCK_SIZE - 1) /
            BLOCK_SIZE
        );


    decompressKernel<<<
        blocks,
        BLOCK_SIZE,
        0,
        gpu.decompressionStream
    >>>(
        gpu.received,
        gpu.output,
        gpu.partition.elements
    );


    CUDA_CHECK(
        cudaGetLastError()
    );


    CUDA_CHECK(
        cudaEventRecord(
            gpu.decompressionReady,
            gpu.decompressionStream
        )
    );
}


// ============================================================
// RING COMMUNICATION
// ============================================================
//
// GPU 0 → GPU 1
// GPU 1 → GPU 2
// GPU 2 → GPU 3
// ...
// GPU N → GPU 0
//
// Cada GPU calcula exactamente lo mismo sobre su propia
// partición y comunica una parte del resultado.
//

void ringRound(
    std::vector<GPUNode>& gpus,
    float factor)
{
    const int count =
        static_cast<int>(
            gpus.size()
        );


    // --------------------------------------------------------
    // COMPUTE
    // --------------------------------------------------------

    for (auto& gpu : gpus)
        runCompute(
            gpu,
            factor
        );


    // --------------------------------------------------------
    // COMPRESS
    // --------------------------------------------------------

    for (auto& gpu : gpus)
        runCompression(
            gpu
        );


    // --------------------------------------------------------
    // RING TRANSFER
    // --------------------------------------------------------

    for (int i = 0; i < count; ++i)
    {
        int source =
            i;

        int destination =
            (i + 1) % count;


        transfer(
            gpus[source],
            gpus[destination]
        );
    }


    // --------------------------------------------------------
    // DECOMPRESS
    // --------------------------------------------------------

    for (auto& gpu : gpus)
        runDecompression(
            gpu
        );
}


// ============================================================
// SYNCHRONIZE FABRIC
// ============================================================

void synchronizeFabric(
    std::vector<GPUNode>& gpus)
{
    for (auto& gpu : gpus)
    {
        CUDA_CHECK(
            cudaSetDevice(gpu.id)
        );

        CUDA_CHECK(
            cudaEventSynchronize(
                gpu.decompressionReady
            )
        );
    }
}


// ============================================================
// MAIN
// ============================================================

int main()
{
    int gpuCount = 0;

    CUDA_CHECK(
        cudaGetDeviceCount(
            &gpuCount
        )
    );


    printf(
        "Detected GPUs: %d\n\n",
        gpuCount
    );


    if (gpuCount < 2)
    {
        printf(
            "This system requires at least 2 GPUs.\n"
        );

        return 0;
    }


    // --------------------------------------------------------
    // CREATE GPU FABRIC
    // --------------------------------------------------------

    std::vector<GPUNode> gpus(
        gpuCount
    );


    size_t globalOffset = 0;


    for (int i = 0; i < gpuCount; ++i)
    {
        gpus[i].id = i;

        initializeGPU(
            gpus[i],
            globalOffset
        );

        globalOffset +=
            ELEMENTS_PER_GPU;
    }


    // --------------------------------------------------------
    // P2P MATRIX
    // --------------------------------------------------------

    printf(
        "P2P topology:\n"
    );


    for (int source = 0;
         source < gpuCount;
         ++source)
    {
        for (int destination = 0;
             destination < gpuCount;
             ++destination)
        {
            if (source == destination)
                continue;


            bool enabled =
                enableP2P(
                    source,
                    destination
                );


            printf(
                "GPU %d -> GPU %d : %s\n",
                source,
                destination,
                enabled
                    ? "P2P"
                    : "HOST"
            );
        }
    }


    printf(
        "\nStarting distributed workload...\n\n"
    );


    // --------------------------------------------------------
    // DISTRIBUTED COMPUTATION
    // --------------------------------------------------------

    for (int round = 0;
         round < ROUNDS;
         ++round)
    {
        printf(
            "Round %d/%d\n",
            round + 1,
            ROUNDS
        );


        ringRound(
            gpus,
            1.001f + round * 0.001f
        );
    }


    // --------------------------------------------------------
    // WAIT
    // --------------------------------------------------------

    synchronizeFabric(
        gpus
    );


    printf(
        "\nGPU fabric computation completed.\n"
    );


    // --------------------------------------------------------
    // CLEANUP
    // --------------------------------------------------------

    for (auto& gpu : gpus)
    {
        CUDA_CHECK(
            cudaSetDevice(
                gpu.id
            )
        );


        cudaFree(gpu.data);
        cudaFree(gpu.working);
        cudaFree(gpu.compressed);
        cudaFree(gpu.received);
        cudaFree(gpu.output);

        cudaStreamDestroy(
            gpu.computeStream
        );

        cudaStreamDestroy(
            gpu.compressionStream
        );

        cudaStreamDestroy(
            gpu.transferStream
        );

        cudaStreamDestroy(
            gpu.decompressionStream
        );

        cudaEventDestroy(
            gpu.computeReady
        );

        cudaEventDestroy(
            gpu.compressionReady
        );

        cudaEventDestroy(
            gpu.transferReady
        );

        cudaEventDestroy(
            gpu.decompressionReady
        );
    }


    return 0;
}